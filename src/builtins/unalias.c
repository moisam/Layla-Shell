/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "builtins.h"
#include "../include/cmd.h"

#define UTILITY         "unalias"


/*
 * Forget all defined aliases.
 */
void unalias_all(void)
{
    int i = 0;
    for( ; i < MAX_ALIASES; i++)
    {
        if(aliases[i].name)
        {
            free(aliases[i].name);
            aliases[i].name = NULL;
        
            if(aliases[i].val)
            {
                free(aliases[i].val);
                aliases[i].val = NULL;
            }
        }
    }
}


/*
 * The unalias builtin utility (POSIX). Used to remove aliases.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help unalias` or `unalias -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int unalias_builtin(int argc, char **argv)
{
    int do_unalias_all = 0;
    int i;
    int v = 1, c;
    int res = 0;

    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "a" : "ahv";

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &UNALIAS_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'a':
                do_unalias_all = 1;
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* the -a option removes all aliases */
    if(do_unalias_all)
    {
        unalias_all();
        return 0;
    }

    /* process the arguments */
    for( ; v < argc; v++)
    {
        i = alias_list_index(argv[v]);
        if(i >= 0)
        {
            free(aliases[i].name);
            aliases[i].name = NULL;
            
            if(aliases[i].val)
            {
                free(aliases[i].val);
                aliases[i].val  = NULL;
            }
        }
        else
        {
            PRINT_ERROR(UTILITY, "unknown alias: %s", argv[v]);
            res = 2;
        }
    }

    return res;
}
