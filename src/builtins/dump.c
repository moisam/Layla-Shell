/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: dump.c
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

#define UTILITY             "dump"

int dump(int argc, char **argv)
{
    int v;
    for(v = 1; v < argc; v++)
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
                        print_help(argv[0], REGULAR_BUILTIN_DUMP, 1, 0);
                        continue;
                        
                    case 'v':
                        printf("%s", shell_ver);
                        continue;
                        
                    default:
                        fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, argv[v]);
                        return 2;
                }
            }
        }
        else if(strcmp(arg, "symtab") == 0)
        {
            dump_local_symtab();
        }
        else if(strcmp(arg, "vars"  ) == 0)
        {
            purge_vars(0);
        }
    }
    return 1;
}