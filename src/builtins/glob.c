/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: glob.c
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

#define UTILITY             "glob"


int __glob(int argc, char **argv)
{
    int v, allow_escaped = optionx_set(OPTION_XPG_ECHO);
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
                }
                p++;
            }
        }
        else break;
    }
    int flags = allow_escaped ? FLAG_ECHO_ALLOW_ESCAPED : 0;
    flags |= FLAG_ECHO_NULL_TERM;
    do_echo(v, argc, argv, flags);
    return 0;
}
