/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: read.c
 *    This file is part of the Layla Shell project.
 *
 *    Layla Shell is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Layla Shell is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Layla Shell.  If not, see <http://www.gnu.org/licenses/>.
 */    

#define _XOPEN_SOURCE   500     /* fdopen() */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY         "read"


/*
 * check if the given file descriptor fd is a FIFO (named pipe).
 */
int isfifo(int fd)
{
    struct stat stats;
    fstat(fd, &stats);
    return (S_ISFIFO(stats.st_mode)) ? 1 : 0;
}


/*
 * determine if input is available on stdin, which should be the terminal or
 * a FIFO (named pipe).
 *
 * we use the value of $TMOUT variable to decide whether to wait for input
 * or not. if $TMOUT is set, we wait for input to arrive within $TMOUT secs.
 * if $TMOUT is not set or its value is invalid, we return immediately.
 * $TMOUT is a non-POSIX extension. this function is used by cmdline() when
 * the shell is interactive. it is also used by read and select.
 * 
 * See http://man7.org/linux/man-pages/man2/select.2.html
 */
int ready_to_read(int fd)
{
    /* only wait if stdin is a terminal device */
    if(!isatty(fd) && !isfifo(fd))
    {
        return 1;
    }
    
    struct symtab_entry_s *entry = get_symtab_entry("TMOUT");

    /* $TMOUT not set, no need to wait */
    if(!entry || !entry->val)
    {
        return 1;
    }
    
    /* get the timeout seconds count */
    char *strend;
    long secs = strtol(entry->val, &strend, 10);
    if(strend == entry->val)
    {
        return 1;
    }
    if(secs <= 0)
    {
        return 1;
    }
    
    /* now wait until input is available on stdin */
    fd_set fdset;
    struct timeval tv;
    int retval;

    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    tv.tv_sec = secs;
    tv.tv_usec = 0;
    retval = select(1, &fdset, NULL, NULL, &tv);
    if (retval == -1)       /* error */
    {
        return 0;
    }
    else if (retval)        /* input available */
    {
        return 1;
    }
    else                    /* timeout */
    {
        return 0;
    }
}


/*
 * add a new shell variable with the given name, and assign it the given value.
 * val_len is the length of the value string.. backslash is used as an escape
 * character, unless suppress_esc is non-zero.
 *
 * returns 1 on success, 0 on error.
 */
int read_set_var(char *name, char *val, int val_len, int suppress_esc)
{
    char *tmp = malloc(val_len+1);
    /* TODO: do something better than bailing out here */
    if(!tmp)
    {
        fprintf(stderr, "%s: insufficient memory for field splitting\n", UTILITY);
        return 0;
    }
    strncpy(tmp, val, val_len);
    tmp[val_len] = '\0';

    /* add the variable we will save input to */
    struct symtab_entry_s *entry = get_symtab_entry(name);
    if(!entry)
    {
        entry = add_to_symtab(name);
    }
    if(!entry)
    {
        fprintf(stderr, "%s: failed to add variable: %s\n", UTILITY, name);
        free(tmp);
        return 0;
    }

    /* we can't save input in a readonly variable */
    if(flag_set(entry->flags, FLAG_READONLY))
    {
        fprintf(stderr, "%s: failed to set '%s': readonly variable\n", UTILITY, name);
        free(tmp);
        return 0;
    }

    /* remove escaped chars */
    if(!suppress_esc)
    {
        char *p = tmp;
        /* remove backslashes */
        while(*p)
        {
            if(*p == '\\')
            {
                delete_char_at(p, 0);
            }
            /* beware of a hanging backslash */
            if(*p)
            {
                p++;
            }
        }
    }

    /* save the field's value */
    symtab_entry_setval(entry, tmp);
    free(tmp);
    return 1;
}


/*
 * convert the input of the read builtin utility into separate fields.
 * this function works similar to field_split(), except it doesn't treat
 * quotes in any special way, and treats backslash as an escape character
 * only if the suppress_esc is zero.
 *
 * returns 0 on success, 1 on failure.
 */
int read_field_split(char *str, int var_count, char **var_names, int suppress_esc)
{
    struct symtab_entry_s *entry = get_symtab_entry("IFS");
    char *IFS = entry ? entry->val : NULL;
    /* POSIX says no IFS means: "space/tab/NL" */
    if(!IFS)
    {
        IFS = " \t\n";
    }
    /* POSIX says empty IFS means no field splitting */
    if(IFS[0] == '\0')
    {
        return 1;
    }
    /* get the IFS spaces and delimiters separately */
    char IFS_space[64];
    char IFS_delim[64];
    if(strcmp(IFS, " \t\n") == 0)   /* "standard" IFS */
    {
        IFS_space[0] = ' ' ;
        IFS_space[1] = '\t';
        IFS_space[2] = '\n';
        IFS_space[3] = '\0';
        IFS_delim[0] = '\0';
    }
    else                            /* "custom" IFS */
    {
        char *p  = IFS;
        char *sp = IFS_space;
        char *dp = IFS_delim;
        do
        {
            if(isspace(*p))
            {
                *sp++ = *p++;
            }
            else
            {
                *dp++ = *p++;
            }
        } while(*p);
        *sp = '\0';
        *dp = '\0';
    }

    size_t len = strlen(str);
    int i = 0;
    var_count--;

    /* skip any leading whitespaces in the string */
    skip_IFS_whitespace(&str, IFS_space);

    /* create the fields */
    char *p1 = str, *p2 = str;
    while(*p2)
    {
        /* stop parsing if we reached the penultimate variable */
        if(i == var_count)
        {
            break;
        }

        /* skip escaped chars */
        if(*p2 == '\\')
        {
            p2++;
            /* beware of a hanging backslash */
            if(*p2)
            {
                p2++;
            }
        }

        /*
         * delimit the field if we have an IFS space or delimiter char, or if
         * we reached the end of the input string.
         */
        if(!*p2 || is_IFS_char(*p2, IFS_space) || is_IFS_char(*p2, IFS_delim))
        {
            /* copy the field text */
            size_t len2 = p2-p1;
            if(!read_set_var(var_names[i], p1, len2, suppress_esc))
            {
                return 1;
            }

            /* skip trailing IFS spaces/delimiters */
            while(*p2 && is_IFS_char(*p2, IFS_space))
            {
                p2++;
            }
            while(*p2 && is_IFS_char(*p2, IFS_delim))
            {
                p2++;
            }
            while(*p2 && is_IFS_char(*p2, IFS_space))
            {
                p2++;
            }

            /* prepare for the next field */
            i++;
            p1 = p2;
        }
        else
        {
            p2++;
        }
    }

    /*
     * we have some value remaining from the input string. store it in the
     * next variable.
     */
    if(*p1)
    {
        len -= (p1-str);
        if(!read_set_var(var_names[i++], p1, len, suppress_esc))
        {
            return 1;
        }
    }

    /* set the remaining variables to empty strings */
    for( ; i <= var_count; i++)
    {
        read_set_var(var_names[i], "", 0, suppress_esc);
    }
    return 0;
}


/*
 * the read builtin utility (POSIX).. used to read input and store it into one
 * or more shell variables.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help read` or `read -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int read_builtin(int argc, char **argv)
{
    /*
     * NOTE: POSIX says read must have at least one name argument,
     *       but most shell use the $REPLY variable when no names are
     *       given. we will do the same here, except when running in
     *       the --posix mode where we follow POSIX.
     */
    
    if(option_set('P') && argc == 1)
    {
        fprintf(stderr, "%s: missing argument: variable name\n", UTILITY);
        return 2;
    }
    
    int    v = 1, c, echo_off = 0;
    int    suppress_esc = 0;      /* if set, don't process escape chars */
    int    save_cmd     = 0;      /* if set, save input in the history file */
    int    timeout      = 0;      /* if set, use timeout when reading from terminal or FIFO */
    char  *TMOUT        = NULL;   /* the saved value of $TMOUT */
    char   delim        = '\0';   /* if set, read chars upto delim instead of \n */
    long   total = 0, max = 0;    /* total and max number of chars read */
    FILE  *infile       = stdin;  /* input source */
    int    infd         = 0;      /* optional file descriptor to read from */
    char  *msg          = NULL;   /* optional message to print before reading input */
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvrd:n:N:su:t:op:", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_READ, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* don't process escape chars */
            case 'r':
                suppress_esc = 1;
                break;
                
            /* echo off (useful when reading passwords) */
            case 'o':
                echo_off = 1;
                break;
                
            /* message to print before reading */
            case 'p':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    return 2;
                }
                msg = __optarg;
                break;
                
            /* read upto the 1st char of __optarg, instead of newline */
            case 'd':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    return 2;
                }
                delim = *__optarg;
                break;
                
            /* max. amount to read */
            case 'N':
            case 'n':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    return 2;
                }
                max = atoi(__optarg);
                if(max < 0)
                {
                    max = 0;
                }
                break;
                
            /* store input in history file */
            case 's':
                save_cmd = 1;
                break;
                
            /* alternate input file */
            case 'u':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    return 2;
                }
                infd  = atoi(__optarg);
                if(infd >= 0 && infd  <= 9)
                {
                    infile = fdopen(infd, "r");
                    if(!infile)
                    {
                        infile = stdin;
                    }
                }
                if(infile == stdin)
                {
                    fprintf(stderr, "%s: invalid file descriptor: %s\n", UTILITY, argv[v]);
                    return 2;
                }
                break;
                
            /* timeout in seconds */
            case 't':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    return 2;
                }
                timeout  = atoi(__optarg);
                if(timeout < 0)
                {
                    timeout = 0;
                }
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    
    char *in = NULL;
    long line_max = get_linemax();
    char buf[line_max];

    /* set the time out, if any */
    struct symtab_entry_s *TMOUT_entry = get_symtab_entry("TMOUT");
    if(!TMOUT_entry)
    {
        TMOUT_entry = add_to_symtab("TMOUT");
    }
    if(timeout)
    {
        /* save the old $TMOUT */
        if(TMOUT_entry && TMOUT_entry->val)
        {
            TMOUT = get_malloced_str(TMOUT_entry->val);
        }
        /* set the new $TMOUT */
        sprintf(buf, "%d", timeout);
        symtab_entry_setval(TMOUT_entry, buf);
    }

    /* print the optional message */
    if(msg)
    {
        printf("%s", msg);
        fflush(stdout);
    }

    /* wait until we are ready */
    if(!ready_to_read(infd))
    {
        /* timed out with no input */
        if(timeout)
        {
            symtab_entry_setval(TMOUT_entry, TMOUT);
            if(TMOUT)
            {
                free_malloced_str(TMOUT);
            }
        }
        return 129;     /* bash exit status is > 128 in case of timeout */
    }

    /* turn echo off */
    if(echo_off)
    {
        echooff(infd);
    }

    /* read input */
    while(!feof(infile))
    {
        if(delim)
        {
            /* read upto delim */
            char *b = buf, c;
            char *bend = b+line_max-1;
            while((c = fgetc(infile)) != EOF)
            {
                *b++ = c;
                if(b == bend || c == delim)
                {
                    *b = '\0';
                    break;
                }
            }
        }
        else
        {
            /* end of input */
            if(fgets(buf, line_max, infile) == NULL)
            {
                break;
            }
        }
        
        int read_more = 0;
        if(!suppress_esc)
        {
            char *p = buf;
            /* remove backslashes */
            while(*p)
            {
                if(*p == '\\')
                {
                    if(*++p == '\n')
                    {
                        read_more = 1;
                        /* remove the \\n sequence */
                        p[-1] = '\0';
                    }
                }
                /* beware of a hanging backslash */
                if(*p)
                {
                    p++;
                }
            }
        }

        /* how much did we read? */
        long buflen = strlen(buf);
        if(max && buflen+total >= max)
        {
            read_more = 0;
            buflen = max-total;
            buf[buflen] = '\0';
        }
        total += buflen;

        /* now save input in malloc'd buffer */
        if(!in)
        {
            in = malloc(total+1);
            if(in)
            {
                strcpy(in, buf);
            }
        }
        else
        {
            char *in2 = realloc(in, total+1);
            if(in2)
            {
                strcat(in2, buf);
                in = in2;
            }
            else
            {
                break;
            }
        }
    
        /* print PS2 if we need more input */
        if(read_more)
        {
            if(isatty(infd) && option_set('i'))
            {
                print_prompt2();
            }
        }
        else
        {
            break;
        }
    }
    
    /* no input. bail out */
    if(!in)
    {
        if(timeout)
        {
            symtab_entry_setval(TMOUT_entry, TMOUT);
            if(TMOUT)
            {
                free_malloced_str(TMOUT);
            }
        }
        if(echo_off)
        {
            echoon(infd);
        }
        return 2;
    }

    /* save input */
    if(save_cmd)
    {
        save_to_history(in);
    }

    /* remove the trailing '\n' */
    if(in[total-1] == '\n')
    {
        in[total-1] = '\0';
    }

    int res = 0;
    /* called with no args? set the $REPLY variable and exit */
    if(v >= argc)
    {
        res = read_field_split(in, 1, (char *[]){ "REPLY" }, suppress_esc);
    }
    else
    {
        res = read_field_split(in, argc-v, &argv[v], suppress_esc);
    }

    /* free buffer */
    free(in);
    
    /* restore $TMOUT value */
    if(timeout)
    {
        symtab_entry_setval(TMOUT_entry, TMOUT);
        if(TMOUT)
        {
            free_malloced_str(TMOUT);
        }
    }

    /* turn echo off */
    if(echo_off)
    {
        echoon(infd);
    }

    /* return the result */
    return res;
}
