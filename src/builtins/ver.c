/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: ver.c
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

#include "../cmd.h"
#include "../symtab/symtab.h"

#define UTILITY             "ver"


/*
 * the ver builtin utility (non-POSIX).. prints the shell version.
 *
 * returns 0 on success, non-zero if an unknown option is given.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help ver` or `ver -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int ver_builtin(int argc, char **argv)
{
    int v = 1;
    for( ; v < argc; v++)
    { 
        char *arg = argv[v];
        if(arg[0] == '-')
        {
            char *p = argv[v];
            while(*++p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], REGULAR_BUILTIN_VER, 1, 0);
                        return 0;
                        
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                        
                    default:
                        fprintf(stderr, "%s: unknown option: %s\n", UTILITY, argv[v]);
                        return 2;
                }
            }
        }
    }
    printf("%s", shell_ver);
    return 0;
}
