/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: unalias.c
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

#include <stdlib.h>
#include "../cmd.h"

#define UTILITY         "unalias"

void unalias_all()
{
    int i;
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(!__aliases[i].name) continue;
        free(__aliases[i].name);
        __aliases[i].name = NULL;
        if(__aliases[i].val) free(__aliases[i].val);
        __aliases[i].val  = NULL;
    }
}


int unalias(int argc, char **argv)
{
    int do_unalias_all = 0;
    int i;
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hva", &v, 1)) > 0)
    {
        if     (c == 'h') { print_help(argv[0], REGULAR_BUILTIN_UNALIAS, 1, 0); }
        else if(c == 'v') { printf("%s", shell_ver); }
        else if(c == 'a') { do_unalias_all = 1     ; }
    }
    /* unknown option */
    if(c == -1) return 1;

    if(do_unalias_all)
    {
        unalias_all();
        return 0;
    }

    int res = 0;
    for( ; v < argc; v++)
    {
        size_t len = strlen(argv[v]);
        for(i = 0; i < MAX_ALIASES; i++)
        {
            if(__aliases[i].name)
            {
                size_t len2 = strlen(__aliases[i].name);
                if(len2 != len) continue;
                if(strcmp(__aliases[i].name, argv[v]) == 0)
                {
                    free(__aliases[i].name);
                    __aliases[i].name = NULL;
                    if(__aliases[i].val) free(__aliases[i].val);
                    __aliases[i].val  = NULL;
                    break;
                }
            }
        }
        if(i == MAX_ALIASES)
        {
            fprintf(stderr, "%s: unknown alias: %s\r\n", UTILITY, argv[v]);
            res = 2;
        }
    }
    return res;
}
