/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 *
 *    file: echo.c
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

#include <wchar.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "setx.h"
#include "../include/debug.h"

#define UTILITY             "echo"


/*
 * Process the options supplied to echo and glob. The 'opts' argument contains 
 * the recognized options ('eEn' for echo and 'eE' for glob). If -e is supplied,
 * 'allow_escaped' is set to 1, if -E is supplied, it is set to 0. If -n is
 * supplied (for echo, not glob), 'supp_nl' is set to 1.
 * 
 * Returns the index of the first non-option argument to be printed by the
 * calling utility.
 */
int process_echo_options(int argc, char **argv, char *opts, int *allow_escaped, int *supp_nl)
{
    char *p, *p2;
    int v = 1;
    int recognize_n = (strchr(opts, 'n')) ? 1 : 0;
    
    /* process options and arguments */
    for( ; v < argc; v++)
    {
        /* options start with '-' */
        if(argv[v][0] == '-')
        {
            p = argv[v]+1;
            int op = (*p != '\0');  /* so that `echo -` prints '-' */

            /*
             * Check the validity of the options string. We only accept
             * three options: e, n and E (for echo), or two options: e and E
             * (for glob). If the string contains any other letter, we treat 
             * it as an argument to be printed, not as an option.
             */
            while(*p)
            {
                for(p2 = opts; *p2; p2++)
                {
                    if(*p2 == *p)
                    {
                        break;
                    }
                }
                
                if(!*p2)
                {
                    op = 0;
                    break;
                }
                p++;
            }

            if(!op)
            {
                break;
            }
            
            /* now process the options */
            p = argv[v]+1;
            while(*p)
            {
                switch(*p)
                {
                    /* -e: allow escape characters (see the manpage for the details) */
                    case 'e':
                        (*allow_escaped) = 1;
                        break;

                    /* -E: don't allow escape characters (see the manpage for the details) */
                    case 'E':
                        (*allow_escaped) = 0;
                        break;

                    /* -n: don't print a newline after the arguments */
                    case 'n':
                        if(recognize_n)
                        {
                            (*supp_nl) = 1;
                        }
                        break;
                }
                p++;
            }
        }
        else
        {
            break;
        }
    }
    
    return v;
}


/*
 * The echo builtin utility (non-POSIX). Prints back the arguments passed to it,
 * followed by an optional newline.
 *
 * Returns 0 invariably.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help echo` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int echo_builtin(int argc, char **argv)
{
    /*
     * In bash, shopt option 'xpg_echo' is used to indicate whether escape
     * sequences are enabled by echo by default. This behavior can be overriden
     * by use of the -e and -E options (see below).
     * We enable escape sequences in --posix mode and if xpg-echo is set.
     */
    int supp_nl = 0;
    int allow_escaped = option_set('P') || optionx_set(OPTION_XPG_ECHO);

    /* process the options */
    int v = process_echo_options(argc, argv, "eEn", &allow_escaped, &supp_nl);
    
    /* print the arguments */
    int flags = allow_escaped ? FLAG_ECHO_ALLOW_ESCAPED : 0;
    if(!supp_nl)
    {
        flags |= FLAG_ECHO_PRINT_NL;
    }

    do_echo(v, argc, argv, flags);

    return 0;
}


/*
 * argv is processed and printed, starting from arg number v.
 * The flags parameter fine tunes the process, like allowing the use of escape
 * chars, for example.
 */
void do_echo(int v, int argc, char **argv, int flags)
{
    int  allow_escaped = flag_set(flags, FLAG_ECHO_ALLOW_ESCAPED);
    char separator = flag_set(flags, FLAG_ECHO_NULL_TERM) ? '\0' : ' ';
    int  print_nl = flag_set(flags, FLAG_ECHO_PRINT_NL);
    int  endme = 0, first = 1;

    clearerr(stdout);
    
    /* process the arguments */
    for( ; v < argc; v++)
    {
        /*
         * we print the separator char before the second and subsequent args,
         * but not before the first arg.
         */
        if(first)
        {
            first = 0;
        }
        else
        {
            printf("%c", separator);
        }

        /* are escape sequences allowed? */
        if(allow_escaped)
        {
            char *str = ansic_expand(argv[v]);
            if(str)
            {
                printf("%s", str);
                free(str);
                /*
                 * we have no way of knowing if ansic_expand() encountered the \c
                 * sequence or not, so we use this dirty hack to check if the
                 * original string contains it, because we need to stop parsing the
                 * rest of the arguments if \c is encountered.
                 */
                if(strstr(argv[v], "\\c"))
                {
                    endme = 1;
                }
            }
            else
            {
                printf("%s", argv[v]);
            }
        }
        /* print the arg as-is, without processing for possible escape sequences */
        else
        {
            printf("%s", argv[v]);
        }

        /* the '\c' escape sequence terminates processing */
        if(endme)
        {
            /* no more output, not even '\n' */
            print_nl = 0;
            break;
        }
    }
    
    if(print_nl)
    {
        putchar('\n');
    }
    
    /*
     * glob utility works similar to echo, except that it outputs its arguments
     * separated by null chars '\0' instead of spaces. We make sure we terminate
     * the last argument printed by glob with a null char.
     */
    if(separator == '\0')
    {
        putchar(separator);
    }
    
    fflush(stdout);
}
