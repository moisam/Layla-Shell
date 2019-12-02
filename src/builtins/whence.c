/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: whence.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../cmd.h"
#include "../parser/parser.h"

#define UTILITY         "whence"


/*
 * the whence builtin utility (non-POSIX).. it is a type/command-like ksh extension
 * with a slightly different set of options than both commands.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help whence` or `whence -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int whence_builtin(int argc, char **argv)
{
    int v = 1, c;
    int res = 0, /* verbose = 0, */ skipfunc = 0, print_path = 0, print_all = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "afhpv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_WHENCE, 1, 0);
                return 0;
                
            case 'a':
                print_all  = 1;
                break;
                
            case 'f':
                skipfunc   = 1;
                break;
                
            case 'p':
                print_path = 1;
                break;
                
            case 'v':
                /* ignored. we always print verbose output */
                //verbose    = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* missing arguments */
    if(v >= argc)
    {
        fprintf(stderr, "%s: missing argument: command name\n", UTILITY);
        return 2;
    }
    
    /* parse the arguments */
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        if(strchr (arg, '/'))
        {
            /* argument contains slashes. treat as a pathname and print as-is */
            printf("%s is %s\n", arg, arg);
        }
        else
        {
            /* argument has no slashes. process it */
            if(!print_path)
            {
                /* check if it is a defined alias */
                char *alias = get_alias_val(arg);
                if(alias && alias != arg)
                {
                    printf("%s is aliased to ", arg);
                    purge_quoted_val(alias);
                    printf("\n");
                    if(!print_all) continue;
                }
                /* check if it is a keyword */
                if(is_keyword (arg) >= 0)
                {
                    printf("%s is a shell keyword\n", arg);
                    if(!print_all) continue;
                }
                /* check if it is a builtin utility */
                if(is_builtin (arg))
                {
                    printf("%s is a shell builtin\n", arg);
                    if(!print_all) continue;
                }
                /* check if it is a defined shell function */
                if(!skipfunc && is_function(arg))
                {
                    printf("%s is a shell function\n", arg);
                    if(!print_all) continue;
                }
            }
            /* now search for an external command with the given name */
            char *path = search_path(arg, NULL, 1);
            if(!path)
            {
                /* path not found */
                printf("%s is unknown\n", arg);
                res = 3;
                continue;
            }
            /* path found */
            printf("%s is %s\n", arg, path);
            free_malloced_str(path);
        }
    }

    return res;
}
