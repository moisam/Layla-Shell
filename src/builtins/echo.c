/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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
#include "../cmd.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY             "echo"


/*
 * the echo builtin utility (non-POSIX).. prints back the arguments passed to it,
 * followed by an optional newline.
 *
 * returns 0 invariably.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help echo` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int echo_builtin(int argc, char **argv)
{
    /*
     * in bash, shopt option 'xpg_echo' is used to indicate whether escape
     * sequences are enabled by echo by default.. this behavior can be overriden
     * by use of the -e and -E options (see below).
     */
    int v = 1, supp_nl = 0, allow_escaped = optionx_set(OPTION_XPG_ECHO);
    char *p;
    /* process options and arguments */
    for( ; v < argc; v++)
    {
        /* options start with '-' */
        if(argv[v][0] == '-')
        {
            p = argv[v]+1;
            int op = 1;
            /*
             * check the validity of the options string.. we only accept
             * three options: e, n and E.. if the string contains any other
             * letter, we treat it as an argument to be printed, not as an option.
             */
            while(*p)
            {
                if(*p != 'e' && *p != 'n' && *p != 'E')
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
                        allow_escaped = 1;
                        break;

                    /* -E: don't allow escape characters (see the manpage for the details) */
                    case 'E':
                        allow_escaped = 0;
                        break;

                    /* -n: don't print a newline after the arguments */
                    case 'n':
                        supp_nl = 1;
                        break;
                }
                p++;
            }
        }
        else break;
    }
    do_echo(v, argc, argv, allow_escaped ? FLAG_ECHO_ALLOW_ESCAPED : 0);
    if(!supp_nl)
    {
        putchar('\n');
    }
    return 0;
}

/*
 * argv is processed and printed, starting from arg number v..
 * the flags parameter fine tunes the process, like allowing the use of escape
 * chars, for example.
 */
void do_echo(int v, int argc, char **argv, int flags)
{
    int  allow_escaped = flag_set(flags, FLAG_ECHO_ALLOW_ESCAPED);
    char separator = flag_set(flags, FLAG_ECHO_NULL_TERM) ? '\0' : ' ';
    int  endme = 0, i, first = 1;
    char *p, *p2;
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
            int k;
            wchar_t j;
            p = argv[v];

            /* process the argument char by char */
            while(*p)
            {
                /* the backslash introduces an escaped char */
                if(*p == '\\')
                {
                    p++;
                    /* process the escaped char */
                    switch(*p)
                    {
                        case 'a':       /* \a - alert or beep */
                            beep();
                            break;

                        case 'b':       /* \b - backspace */
                            putchar('\b');
                            break;

                        case 'c':       /* \c - end of processing */
                            endme = 1;
                            break;

                        case 'e':       /* \e - ESC */
                        case 'E':       /* the same */
                            putchar('\e');
                            break;

                        case 'f':       /* \f - form feed */
                            putchar('\f');
                            break;

                        case 'n':       /* \n - newline */
                            putchar('\n');
                            break;

                        case 'r':       /* \r - carriage return */
                            putchar('\r');
                            break;

                        case 't':       /* \t - TAB */
                            putchar('\t');
                            break;

                        case 'v':       /* \v - vertical tab */
                            putchar('\v');
                            break;

                        case '\\':       /* \\ - backslash */
                            putchar('\\');
                            break;

                        case '0':       /* \0nnn - octal ASCI char code */
                            p++;
                            if(isdigit(*p))
                            {
                                /* get 1st digit */
                                i = (*p)-'0';
                                p2 = p+1;
                                if(isdigit(*p2))
                                {
                                    /* get 2nd digit */
                                    i = i*8 + (*p2)-'0';
                                    p2++;
                                }
                                if(isdigit(*p2))
                                {
                                    /* get 3rd digit */
                                    i = i*8 + (*p2)-'0';
                                    p2++;
                                }
                                p = p2-1;
                                printf("%d", i);
                            }
                            else
                            {
                                /* no octal digits. bail out */
                                p--;
                                printf("\\0");
                            }
                            break;

                        case 'x':       /* \xNN - hexadecimal ASCII char code */
                            i = 0;
                            p2 = p+1;
                            /* get 1st digit */
                            if(get_ndigit(*p2, 16, &i))
                            {
                                p2++;
                                if(get_ndigit(*p2, 16, &k))
                                {
                                    /* get 2nd digit */
                                    i = i*16 + k;
                                    p2++;
                                }
                            }
                            /* print the char */
                            if(p2 == p+1)
                            {
                                printf("\\x");
                            }
                            else
                            {
                                /* no hex digits. bail out */
                                p = p2-1;
                                printf("%u", i);
                            }
                            break;

                        /*
                         * we use UTF-8. see https://en.wikipedia.org/wiki/UTF-8
                         */
                        case 'U':           /* unicode char \UHHHHHHHH */
                        case 'u':           /* unicode char \uHHHH */
                            /* \U accepts upto 8 digits, while \u accepts upto 4 */
                            if(*p == 'u')
                            {
                                k = 4;
                            }
                            else
                            {
                                k = 8;
                            }
                            j = 0;
                            /* get the Unicode code point */
                            for(i = 1; i <= k; i++)
                            {
                                int l = 0;
                                if(get_ndigit(p[i], 16, &l))
                                {
                                    j = j*16 + l;
                                }
                                else
                                {
                                    break;
                                }
                            }
                            /* if we didn't move forward, means we didn't find any digits */
                            if(i == 1)
                            {
                                printf("\\u");
                                break;
                            }
                            /* print the Unicode char */
                            printf("%lc", j);
                            /* move forward */
                            p += (i-1);
                            break;

                        default:
                            /* escaped char that is none of the above */
                            putchar('\\');
                            putchar(*p);
                            break;
                    }
                }
                else
                {
                    /* print all other chars */
                    putchar(*p);
                }
                /* the '\c' escape sequence terminates processing */
                if(endme)
                {
                    break;
                }
                p++;
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
            break;
        }
    }
    /*
     * glob utility works similar to echo, except that it outputs its arguments
     * separated by null chars '\0' instead of spaces.. we make sure we terminate
     * the last argument printed by glob with a null char.
     */
    if(separator == '\0')
    {
        putchar(separator);
    }
}
