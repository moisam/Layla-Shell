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
 * whence is a type/command-like ksh extension with a slightly different
 * set of options than both commands.
 */
int whence(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    int res = 0, /* verbose = 0, */ skipfunc = 0, print_path = 0, print_all = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
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
    if(c == -1) return 2;

    if(v >= argc)
    {
        fprintf(stderr, "%s: missing argument: command name\r\n", UTILITY);
        return 2;
    }
    
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        if(strchr (arg, '/'))
        {
            printf("%s is %s\r\n", arg, arg);
        }
        else
        {
            if(!print_path)
            {
                char *alias = __parse_alias(arg);
                if(alias != arg)
                {
                    printf("%s is aliased to ", arg);
                    purge_quoted_val(alias);
                    printf("\r\n");
                    if(!print_all) continue;
                }
                if(is_keyword (arg) >= 0)
                {
                    printf("%s is a shell keyword\r\n", arg);
                    if(!print_all) continue;
                }
                if(is_builtin (arg))
                {
                    printf("%s is a shell builtin\r\n", arg);
                    if(!print_all) continue;
                }
                if(!skipfunc && is_function(arg))
                {
                    printf("%s is a shell function\r\n", arg);
                    if(!print_all) continue;
                }
            }
            char *path = search_path(arg, NULL, 1);
            if(!path)
            {
                printf("%s is unknown\r\n", arg);
                res = 3;
                continue;
            }
            printf("%s is %s\r\n", arg, path);
            free_malloced_str(path);
        }
    }

    return res;
}
