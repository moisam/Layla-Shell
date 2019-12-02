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


/*
 * forget all defined aliases.
 */
void unalias_all(void)
{
    int i = 0;
    for( ; i < MAX_ALIASES; i++)
    {
        if(!aliases[i].name)
        {
            continue;
        }
        free(aliases[i].name);
        aliases[i].name = NULL;
        if(aliases[i].val)
        {
            free(aliases[i].val);
        }
        aliases[i].val  = NULL;
    }
}


/*
 * the unalias builtin utility (POSIX).. used to remove aliases.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help unalias` or `unalias -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int unalias_builtin(int argc, char **argv)
{
    int do_unalias_all = 0;
    int i;
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hva", &v, 1)) > 0)
    {
        if(c == 'h')
        {
            print_help(argv[0], REGULAR_BUILTIN_UNALIAS, 1, 0);
            return 0;
        }
        else if(c == 'v')
        {
            printf("%s", shell_ver);
            return 0;
        }
        else if(c == 'a')
        {
            do_unalias_all = 1;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }

    /* the -a option removes all aliases */
    if(do_unalias_all)
    {
        unalias_all();
        return 0;
    }

    /* process the arguments */
    int res = 0;
    for( ; v < argc; v++)
    {
        for(i = 0; i < MAX_ALIASES; i++)
        {
            if(aliases[i].name)
            {
                if(strcmp(aliases[i].name, argv[v]) == 0)
                {
                    free(aliases[i].name);
                    aliases[i].name = NULL;
                    if(aliases[i].val)
                    {
                        free(aliases[i].val);
                    }
                    aliases[i].val  = NULL;
                    break;
                }
            }
        }
        /* alias not found */
        if(i == MAX_ALIASES)
        {
            fprintf(stderr, "%s: unknown alias: %s\n", UTILITY, argv[v]);
            res = 2;
        }
    }
    return res;
}
