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

int isfifo(int fd)
{
    struct stat stats;
    fstat(fd, &stats);
    return (S_ISFIFO(stats.st_mode)) ? 1 : 0;
}

/*
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
    if(!isatty(fd) && !isfifo(fd)) return 1;
    
    struct symtab_entry_s *entry = get_symtab_entry("TMOUT");
    /* $TMOUT not set, no need to wait */
    if(!entry || !entry->val) return 1;
    
    /* get the timeout seconds count */
    char *strend;
    long secs = strtol(entry->val, &strend, 10);
    if(strend == entry->val) return 1;
    if(secs <= 0) return 1;
    
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
        return 0;
    else if (retval)        /* input available */
        return 1;
    else                    /* timeout */
        return 0;
}


int __read(int argc, char **argv)
{
    /*
     * NOTE: POSIX says read must have at least one name argument,
     *       but most shell use the $REPLY variable when no names are
     *       given. we will do the same here.
     */
    
    /*
    if(argc == 1)
    {
        fprintf(stderr, "%s: missing argument: variable name\r\n", UTILITY);
        return 2;
    }
    */
    
    /****************************
     * process the arguments
     ****************************/
    int    v = 1, c, echo_off = 0;
    int    suppress_esc = 0;
    int    save_cmd = 0;
    int    timeout = 0;
    char  *TMOUT = NULL;   /* the saved value of $TMOUT */
    char   delim = '\0';
    size_t total = 0, max = 0;
    FILE  *infile = stdin;
    int    infd   = 0;
    char  *msg    = NULL;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
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
                
            case 'r':
                suppress_esc = 1;
                break;
                
            case 'o':
                echo_off = 1;
                break;
                
            case 'p':
                if(!__optarg || __optarg == INVALID_OPTARG) return 2;
                msg = __optarg;
                break;
                
            /* read to the 1st char of __optarg, instead of newline */
            case 'd':
                if(!__optarg || __optarg == INVALID_OPTARG) return 2;
                delim = *__optarg;
                break;
                
            /* max. amount to read */
            case 'N':
            case 'n':
                if(!__optarg || __optarg == INVALID_OPTARG) return 2;
                max = atoi(__optarg);
                if(max < 0) max = 0;
                break;
                
            /* store input in history file */
            case 's':
                save_cmd = 1;
                break;
                
            /* alternate input file */
            case 'u':
                if(!__optarg || __optarg == INVALID_OPTARG) return 2;
                infd  = atoi(__optarg);
                if(infd >= 0 && infd  <= 9)
                {
                    infile = fdopen(infd, "r");
                    if(!infile) infile = stdin;
                }
                if(infile == stdin)
                {
                    fprintf(stderr, "%s: invalid file descriptor: %s\r\n", UTILITY, argv[v]);
                    return 2;
                }
                break;
                
            /* timeout in seconds */
            case 't':
                if(!__optarg || __optarg == INVALID_OPTARG) return 2;
                timeout  = atoi(__optarg);
                if(timeout < 0) timeout = 0;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 1;
    
    char *in = NULL;
    long line_max = sysconf(_SC_LINE_MAX);
    if(line_max <= 0) line_max = DEFAULT_LINE_MAX;
    char buf[line_max];

    /* set the time out, if any */
    struct symtab_entry_s *TMOUT_entry = get_symtab_entry("TMOUT");
    if(!TMOUT_entry) TMOUT_entry = add_to_symtab("TMOUT");
    if(timeout)
    {
        if(TMOUT_entry && TMOUT_entry->val) TMOUT = get_malloced_str(TMOUT_entry->val);
        sprintf(buf, "%d", timeout);
        symtab_entry_setval(TMOUT_entry, buf);
    }

    if(msg)
    {
        printf("%s", msg);
        fflush(stdout);
    }

    /* wait until we are ready */
    if(!ready_to_read(infd))
    {
        if(timeout)
        {
            symtab_entry_setval(TMOUT_entry, TMOUT);
            if(TMOUT) free_malloced_str(TMOUT);
        }
        return 129;     /* bash exit status is > 128 in case of timeout */
    }

    if(echo_off) echooff(infd);
    while(!feof(infile))
    {
        if(delim)
        {
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
            if(fgets(buf, line_max, infile) == NULL) break;
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
                    delete_char_at(p, 0);
                    if(*p == '\n')
                    {
                        read_more = 1;
                        delete_char_at(p, 0);
                    }
                }
                /* beware of a hanging backslash */
                if(*p) p++;
            }
        }

        /* how much did we read? */
        size_t buflen = strlen(buf);
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
            in = (char *)malloc(buflen+1);
            if(in) strcpy(in, buf);
        }
        else
        {
            in = (char *)realloc(in, buflen+strlen(in)+1);
            if(in) strcat(in, buf);
        }
    
        if(read_more)
        {
            if(isatty(0) && option_set('i')) print_prompt2();
        }
        else break;
    }
    
    /* no input. bail out */
    if(!in)
    {
        if(timeout)
        {
            symtab_entry_setval(TMOUT_entry, TMOUT);
            if(TMOUT) free_malloced_str(TMOUT);
        }
        if(echo_off) echoon(infd);
        return 2;
    }
    if(save_cmd) save_to_history(in);
    struct cmd_token *first = __make_fields(in);
    if(!first)
    {
        /* no field splitting done. remove '\n' and save. */
        int len = strlen(in);
        if(in[len-1] == '\n') in[len-1] = '\0';
        first = make_cmd_token(in);
        //free(in);
        //return 2;
    }
    
    struct cmd_token *t = first;
    struct symtab_entry_s *entry;
    int res = 0;
    
    /* called with no args? set the $REPLY variable and exit */
    if(v >= argc)
    {
        entry = add_to_symtab("REPLY");
        char *str = __tok_to_str(first);
        symtab_entry_setval(entry, str);
        free(str);
        goto end;
    }
    
loop2:
    entry = add_to_symtab(argv[v]);
    if(!entry)
    {
        fprintf(stderr, "%s: failed to add variable: %s\r\n", UTILITY, argv[v]);
        res = 2; goto end;
    }
    if(flag_set(entry->flags, FLAG_READONLY))
    {
        fprintf(stderr, "%s: failed to assign to variable '%s': readonly variable\r\n", UTILITY, argv[v]);
        res = 2; goto end;
    }
    symtab_entry_setval(entry, t->data);
    v++;
    struct cmd_token *next = t->next;
    /* fields are less than vars */
    if(!next)
    {
        for( ; v < argc; v++)
        {
            entry = add_to_symtab(argv[v]);
            if(!entry)
            {
                fprintf(stderr, "%s: failed to add variable: %s\r\n", UTILITY, argv[v]);
                res = 2; goto end;
            }
            if(flag_set(entry->flags, FLAG_READONLY))
            {
                fprintf(stderr, "%s: failed to assign to variable '%s': readonly variable\r\n", UTILITY, argv[v]);
                res = 2; goto end;
            }
            symtab_entry_setval(entry, "");
        }
    }
    /* vars are less than fields */
    else if(v >= argc)
    {
        /*
         * NOTE: we are taking the easy road here. POSIX says we should 
         *       concatenate the remaining fields with proper delimiters,
         *       and ignore trailing IFS whitespace. we just glue the
         *       fields using spaces!.
         * TODO: fix this to proper behaviour!.
         */
        char *str = __tok_to_str(t);
        symtab_entry_setval(entry, str);
        free(str);
    }
    else
    {
        t = next;
        if(t) goto loop2;
    }
    
end:
    free_all_tokens(first);
    free(in);
    if(timeout)
    {
        symtab_entry_setval(TMOUT_entry, TMOUT);
        if(TMOUT) free_malloced_str(TMOUT);
    }
    if(echo_off) echoon(infd);
    return res;
}
