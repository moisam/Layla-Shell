/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: printenv.c
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

extern char **environ;

#define UTILITY             "printenv"


/*
 * the printenv utility is a tcsh non-POSIX extension. bash doesn't have it,
 * as it is part of the GNU coreutils package, not the shell itself.
 */

int printenv(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    char separator = '\n';
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hv0", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_PRINTENV, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case '0':
                separator = '\0';
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        char **p2 = environ;
        while(*p2)
        {
            printf("%s%c", *p2, separator);
            p2++;
        }
        return 0;
    }
    
    c = 0;
    for( ; v < argc; v++)
    {
        char *p = getenv(argv[v]);
        if(p)
        {
            printf("%s%c", p, separator);
            c = 0;
        }
        else c = 1;
    }
    if(c) putchar(separator);
    return 0;
}
