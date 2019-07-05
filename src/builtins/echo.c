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

/* defined in ../cmd_args.c */
char get_xdigit(char c);


int echo(int argc, char **argv)
{
    int v, supp_nl = 0, allow_escaped = optionx_set(OPTION_XPG_ECHO);
    char *p;
    
    for(v = 1; v < argc; v++)
    { 
        if(argv[v][0] == '-')
        {
            p = argv[v]+1;
            int op = 1;
            while(*p)
            {
                if(*p != 'e' && *p != 'n' && *p != 'E')
                {
                    op = 0;
                    break;
                }
                p++;
            }
            if(!op) break;
            
            p = argv[v]+1;
            while(*p)
            {
                switch(*p)
                {
                    case 'e':
                        allow_escaped = 1;
                        break;
                        
                    case 'E':
                        allow_escaped = 0;
                        break;
                        
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
    if(!supp_nl) putchar('\n');
    return 0;
}

/*
 * argv is processed and printed, starting from arg number v.
 * flags fine-tune the process, like allowing escaped chars.
 */
void do_echo(int v, int argc, char **argv, int flags)
{
    int  allow_escaped = flag_set(flags, FLAG_ECHO_ALLOW_ESCAPED);
    char separator = flag_set(flags, FLAG_ECHO_NULL_TERM) ? '\0' : ' ';
    int endme = 0, i, first = 1;
    char *p, *p2;
    for( ; v < argc; v++)
    {
        if(first) first = 0;
        else      printf("%c", separator);
        
        if(allow_escaped)
        {
            int  k;
            wchar_t j;
            char c;
            p = argv[v];
            while(*p)
            {
                if(*p == '\\')
                {
                    p++;
                    switch(*p)
                    {
                        case 'a': beep()       ; break;
                        case 'b': putchar('\b'); break;
                        case 'c': endme = 1    ; break;
                        case 'e':
                        case 'E': putchar('\e'); break;
                        case 'f': putchar('\f'); break;
                        case 'n': putchar('\n'); break;
                        case 'r': putchar('\r'); break;
                        case 't': putchar('\t'); break;
                        case 'v': putchar('\v'); break;
                        case '\\': putchar('\\'); break;
                        
                        case '0':
                            p++;
                            if(isdigit(*p))
                            {
                                i = (*p)-'0';
                                p2 = p+1;
                                if(isdigit(*p2)) i = i*8 + (*p2)-'0', p2++;
                                if(isdigit(*p2)) i = i*8 + (*p2)-'0', p2++;
                                p = p2-1;
                                printf("%d", i);
                            }
                            else p--, printf("\\0");
                            break;
                            
                        case 'x':
                            i = 0;
                            p2 = p+1;
                            if(isxdigit(*p2)) i = get_xdigit(*p2), p2++;
                            if(isxdigit(*p2)) i = i*16 + get_xdigit(*p2), p2++;
                            
                            if(p2 == p+1) printf("\\x");
                            else p = p2-1, printf("%u", i);
                            break;
                            
                        /*
                         * we use UTF-8. see https://en.wikipedia.org/wiki/UTF-8
                         */
                        case 'U':           /* unicode char \UHHHHHHHH */
                            k = 8;
                    
                        case 'u':           /* unicode char \uHHHH */
                            if(*p == 'u') k = 4;
                            j = 0;
                            for(i = 1; i <= k; i++)
                            {
                                if(isxdigit(p[i]))
                                {
                                    c = get_xdigit(p[i]);
                                    j = j*16 + c;
                                }
                                else break;
                            }
                            if(i == 1)
                            {
                                printf("\\u");
                                break;
                            }
                            printf("%lc", j);
                            p += (i-1);
                            break;
                        
                        default:
                            putchar('\\');
                            putchar(*p);
                            break;
                    }
                }
                else putchar(*p);
                if(endme) break;
                p++;
            }
        }
        else
        {
            printf("%s", argv[v]);
        }
        if(endme) break;
    }
    if(separator == '\0') putchar(separator);
}
