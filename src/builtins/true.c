/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: true.c
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

#define UTILITY             "true"

int true(int argc, char **argv)
{
    /*
     * NOTE: ksh, bash and other major shells implement a true builtin
     *       that accepts no options, specially the '--' option.
     */
#if 0
    int v;
    for(v = 1; v < argc; v++)
    { 
        if(argv[v][0] == '-')
        {
            if(strcmp(argv[v], "-h") == 0) { print_help(argv[0], REGULAR_BUILTIN_TRUE, 1); continue; }
            if(strcmp(argv[v], "-v") == 0) { printf("%s", shell_ver); continue; }
            fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, argv[v]);
            return 2;
        }
        break;
    }
#endif
    return 0;
}

