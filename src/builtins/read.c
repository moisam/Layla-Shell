/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY             "read"


#define CHECK_OPTION_HAS_ARG(c)                             \
if(!internal_optarg || internal_optarg == INVALID_OPTARG)   \
{                                                           \
    PRINT_ERROR("%s: missing argument for option: -%c\n",   \
                UTILITY, c);                                \
    return 2;                                               \
}

struct stat stats;


/*
 * check if the given file descriptor fd is a FIFO (named pipe).
 */
int is_fifo(int fd)
{
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
int ready_to_read(int fd, struct timeval *timeout)
{
    /* only wait if stdin is a terminal device */
    if(!isatty(fd) && !is_fifo(fd))
    {
        return 1;
    }
    
    /* timeout not set, no need to wait */
    if(timeout->tv_sec == 0 && timeout->tv_usec == 0)
    {
        return 1;
    }
    
    /* timeout invalid, return */
    if(timeout->tv_sec < 0 || timeout->tv_usec < 0)
    {
        return 1;
    }
    
    /* now wait until input is available on stdin */
    fd_set fdset;
    struct timeval tv;
    int retval;

    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    tv.tv_sec = timeout->tv_sec;
    tv.tv_usec = timeout->tv_usec;
    retval = select(1, &fdset, NULL, NULL, &tv);

    /* return 0 if error or timeout, 1 otherwise */
    return !(retval <= 0);
}


/*
 * add a new shell variable with the given name, and assign it the given value.
 * val_len is the length of the value string.. backslash is used as an escape
 * character, unless suppress_esc is non-zero.
 *
 * returns 1 on success, 0 on error.
 */
int read_set_var(char *name, char *val, int val_len /* , int suppress_esc */)
{
    char *tmp = malloc(val_len+1);
    /* TODO: do something better than bailing out here */
    if(!tmp)
    {
        PRINT_ERROR("%s: insufficient memory for field splitting\n", UTILITY);
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

    /* we can't save input in a readonly variable */
    if(flag_set(entry->flags, FLAG_READONLY))
    {
        READONLY_ASSIGN_ERROR(UTILITY, name);
        free(tmp);
        return 0;
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
int read_field_split(char *str, int var_count, char **var_names /* , int suppress_esc */)
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

        /*
         * delimit the field if we have an IFS space or delimiter char, or if
         * we reached the end of the input string.
         */
        if(!*p2 || is_IFS_char(*p2, IFS_space) || is_IFS_char(*p2, IFS_delim))
        {
            /* copy the field text */
            size_t len2 = p2-p1;
            if(!read_set_var(var_names[i], p1, len2 /* , suppress_esc */))
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
        if(!read_set_var(var_names[i++], p1, len /* , suppress_esc */))
        {
            return 1;
        }
    }

    /* set the remaining variables to empty strings */
    for( ; i <= var_count; i++)
    {
        read_set_var(var_names[i], "", 0 /* , suppress_esc */);
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
        PRINT_ERROR("%s: missing argument: variable name\n", UTILITY);
        return 2;
    }
    
    int v = 1, c, echo_off = 0;
    /* if set, don't process escape chars */
    int suppress_esc = 0;
    /* if set, save input in the history file */
    int save_cmd = 0;
    /* if set, use timeout when reading from terminal or FIFO */
    struct timeval timeout = { 0, 0 };
    int has_timeout = 0;
    /* if set, read chars upto delim instead of \n */
    char delim = '\n';
    int ignore_delim = 0;
    /* total and max number of chars read */
    long max = 0;
    /* file descriptor to read from */
    int infd = 0;
    /* if set, we're reading input from a terminal */
    int reading_tty = 0;
    /* optional message to print before reading input */
    char *msg = NULL;
    char *strend = NULL;
    /* current terminal attributes */
    struct termios attr;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvrd:n:N:su:t:op:", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &READ_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* don't process escape chars */
            case 'r':
                suppress_esc = 1;
                break;
                
            /*
             * echo off (bash uses -s, but we used -s to store input in the
             * history file as ksh does -- see below).
             */
            case 'o':
                echo_off = 1;
                break;
                
            /* message to print before reading */
            case 'p':
                CHECK_OPTION_HAS_ARG(c);
                msg = internal_optarg;
                break;
                
            /* read upto the 1st char of internal_optarg, instead of newline */
            case 'd':
                CHECK_OPTION_HAS_ARG(c);
                delim = *internal_optarg;
                break;
                
            /* max. amount to read */
            case 'N':
                ignore_delim = 1;
                __attribute__((fallthrough));
                
            case 'n':
                CHECK_OPTION_HAS_ARG(c);
                max = strtol(internal_optarg, &strend, 10);
                if(*strend || max < 0)
                {
                    PRINT_ERROR("%s: invalid count: %s\n", UTILITY, internal_optarg);
                    return 2;
                }
                break;
                
            /* store input in the history file (ksh, bash doesn't have this option) */
            case 's':
                save_cmd = 1;
                break;
                
            /* alternate input file */
            case 'u':
                CHECK_OPTION_HAS_ARG(c);
                infd = strtol(internal_optarg, &strend, 10);
                if(*strend || fcntl(infd, F_GETFD, 0) == -1)
                {
                    PRINT_ERROR("%s: invalid file descriptor: %s\n", UTILITY, internal_optarg);
                    return 2;
                }
                break;
                
            /* timeout in seconds */
            case 't':
                CHECK_OPTION_HAS_ARG(c);
                if(!get_secs_usecs(internal_optarg, &timeout))
                {
                    PRINT_ERROR("%s: invalid timeout: %s\n", UTILITY, internal_optarg);
                    return 2;
                }
                
                if(timeout.tv_sec < 0 || timeout.tv_usec < 0)
                {
                    timeout.tv_sec = 0;
                    timeout.tv_usec = 0;
                }
                else
                {
                    has_timeout = 1;
                }
                break;
        }
    }
    
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    
    /* if no timeout given, check $TMOUT */
    if(!has_timeout && get_secs_usecs(get_shell_varp("TMOUT", NULL), &timeout))
    {
        if(timeout.tv_sec < 0 || timeout.tv_usec < 0)
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
        }
        else
        {
            has_timeout = 1;
        }
    }
    
    /* turn on some flags if not reading from tty (bash) */
    reading_tty = isatty(infd);
    if(!reading_tty && (msg || echo_off))
    {
        msg = NULL;
        echo_off = 0;
    }
    
    /* turn off timeout if reading from a regular file (bash) */
    if(has_timeout)
    {
        if(fstat(infd, &stats) < 0 || S_ISREG(stats.st_mode))
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            has_timeout = 0;
        }
    }

    /* wait until we are ready */
    if(!ready_to_read(infd, &timeout))
    {
        /* bash exit status is > 128 in case of timeout */
        return 129;
    }

    if(reading_tty)
    {
        /* get terminal attribs */
        if(tcgetattr(infd, &attr) == -1)
        {
            PRINT_ERROR("%s: failed to get terminal attributes: %s\n", 
                        UTILITY, strerror(errno));
            return 1;
        }
    
        /* make the terminal return 1 char at a time */
        struct termios attr2 = attr;

        attr2.c_lflag &= ~ICANON;
        attr2.c_lflag |= (ISIG | IEXTEN);
        attr2.c_iflag &= ~INLCR;
        attr2.c_iflag |= ICRNL;
        attr2.c_oflag &= ~(OCRNL | ONOCR | ONLRET);
        attr2.c_oflag |= (OPOST | ONLCR);

        if(has_timeout)
        {
            /* we should set the timer in terms of 1/10th of a second */
            attr2.c_cc[VMIN ] = 0;
            attr2.c_cc[VTIME] = (timeout.tv_sec*10)+(timeout.tv_usec/100000);
        }
        else
        {
            attr2.c_cc[VMIN ] = 1;
            attr2.c_cc[VTIME] = 0;
        }

        /* turn echo off */
        if(echo_off)
        {
            attr2.c_lflag &= ~ECHO;
        }
    
        set_tty_attr(infd, &attr2);

        /* read input */
        if(infd == 0)
        {
            clearerr(stdin);
        }
    }

    /* print the optional message (bash) */
    if(msg)
    {
        fprintf(stderr, "%s", msg);
        fflush(stderr);
    }
    
    int buf_size = 32;
    char *buf = alloc_string_buf(buf_size);
    if(!buf)
    {
        PRINT_ERROR("%s: failed to allocate buffer: %s\n", UTILITY, strerror(errno));
        return 1;
    }

    int count = 0, skip_next = 0;
    char *b = buf;
    char *bend = b+buf_size-1;

    while((c = read(infd, b, 1)) == 1)
    {
        /* EOF */
        if(*b == 0x04)
        {
            break;
        }
        
        /* if buffer is full, extend it */
        if(!may_extend_string_buf(&buf, &bend, &b, &buf_size))
        {
            PRINT_ERROR("%s: failed to allocate buffer: %s\n", 
                        UTILITY, strerror(errno));
            buf = NULL;
            b = NULL;
            break;
        }

        /* last char was a backslash and we don't have the -r option */
        if(skip_next)
        {
            skip_next = 0;
            /* discard the \\n combination */
            if(*b == '\n' && interactive_shell && reading_tty)
            {
                print_prompt2();
                continue;
            }
            else
            {
                count++;
                if(max && count >= max)
                {
                    b++;
                    break;
                }
            }
        }
        
        /* we have a backslash char, which we'll overwrite with the next char */
        if(!suppress_esc && *b == '\\')
        {
            skip_next = 1;
            continue;
        }
        
        /* stop if we reached the delimiter char */
        if(!ignore_delim && *b == delim)
        {
            b++;
            break;
        }
        
        b++;
        count++;
        if(max && count >= max)
        {
            break;
        }
    }
    
    if(b)
    {
        *b = '\0';
    }
    
    /* no input. bail out */
    if(c < 0 || !buf || !*buf)
    {
        if(reading_tty)
        {
            set_tty_attr(infd, &attr);
        }
        
        if(c < 0 && errno != EINTR)
        {
            PRINT_ERROR("%s: failed to read: %s\n", UTILITY, strerror(errno));
        }

        for( ; v < argc; v++)
        {
            struct symtab_entry_s *entry = get_symtab_entry(argv[v]);
            if(entry)
            {
                /* we can't set a readonly variable */
                if(flag_set(entry->flags, FLAG_READONLY))
                {
                    READONLY_ASSIGN_ERROR(UTILITY, argv[v]);
                }
                else
                {
                    symtab_entry_setval(entry, NULL);
                }
            }
        }

        return 2;
    }

    /* save input */
    if(save_cmd)
    {
        save_to_history(buf);
    }

    /* remove the trailing '\n' */
    if(b[-1] == '\n')
    {
        b[-1] = '\0';
    }

    int res = 0;
    /* called with no args? set the $REPLY variable and exit */
    if(v >= argc)
    {
        res = read_field_split(buf, 1, (char *[]){ "REPLY" } /* , suppress_esc */);
    }
    else
    {
        res = read_field_split(buf, argc-v, &argv[v] /* , suppress_esc */);
    }

    /* free buffer */
    free(buf);

    /* restore terminal attribs */
    if(reading_tty)
    {
        set_tty_attr(infd, &attr);
    }

    /* return the result */
    return res;
}
