/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: type.c
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

#define UTILITY         "type"

/* helper macros */
#define stringify(str)      #str

#define print_type(arg, type)                                   \
do {                                                            \
    if(print_word)                                              \
    {                                                           \
        printf(stringify(type) "\n");                           \
    }                                                           \
    else                                                        \
    {                                                           \
        printf("%s is a shell " stringify(type) "\n", (arg));   \
    }                                                           \
} while(0)


/*
 * the type builtin utility (POSIX).. used to print the type of an argument.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help type` or `type -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int type_builtin(int argc, char **argv)
{
    int i;
    int res = 0;
    /* print the command's path without searching for builtins and functions first */
    int print_path       = 0;
    /* print all possible interpretations of each argument */
    int print_all        = 0;
    //int use_path         = 0;
    /* print only one word to describe each argument */
    int print_word       = 0;
    /* look in the functions table */
    int search_funcs     = 1;

    /****************************
     * process the options
     ****************************/
    for(i = 1; i < argc; i++)
    { 
        if(argv[i][0] == '-')
        {
            char *p = argv[i];
            /* the special '-' and '--' options */
            if(strcmp(p, "-") == 0 || strcmp(p, "--") == 0)
            {
                i++;
                break;
            }
            /* skip the '-' */
            p++;
            /* process the options string */
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], REGULAR_BUILTIN_TYPE, 1, 0);
                        return 0;
                        
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                        
                    case 't':
                        print_word = 1;
                        break;
                        
                    case 'P':
                        //use_path = 1;
                        /* fall through */
                        __attribute__((fallthrough));
                        
                    case 'p':
                        print_path = 1;
                        if(startup_finished && option_set('r'))
                        {
                            /* r-shells can't use this option */
                            fprintf(stderr, "%s: restricted shells can't use the -%c option\n", SHELL_NAME, *p);
                            return 3;
                        }
                        break;
                        
                    case 'a':
                        print_all = 1;
                        break;
                        
                    case 'f':
                        search_funcs = 0;
                        break;
                        
                    default:                        
                        fprintf(stderr, "%s: unknown option: %s\n", UTILITY, argv[i]);
                        return 2;
                }
                p++;
            }
        }
        else
        {
            break;
        }
    }

    /* missing arguments */
    if(i >= argc)
    {
        fprintf(stderr, "%s: missing argument: command name\n", UTILITY);
        return 2;
    }

    /* parse the arguments */
    for( ; i < argc; i++)
    {
        if(strchr (argv[i], '/'))
        {
            /* argument contains slashes. treat as a pathname and print as-is */
            printf("%s is %s\n", argv[i], argv[i]);
        }
        else
        {
            /* argument has no slashes. process it */
            if(!print_path)
            {
                /* check if it is a defined alias */
                char *alias = get_alias_val(argv[i]);
                if(alias && alias != argv[i])
                {
                    if(print_word)
                    {
                        printf("alias\n");
                    }
                    else
                    {
                        printf("%s is aliased to ", argv[i]);
                        print_quoted_val(alias);
                        printf("\n");
                    }

                    if(!print_all)
                    {
                        continue;
                    }
                }

                /* check if it is a keyword */
                if(is_keyword (argv[i]) >= 0)
                {
                    print_type(argv[i], keyword);
                    if(!print_all)
                    {
                        continue;
                    }
                }
                /* check if it is a builtin utility */
                if(is_builtin (argv[i]))
                {
                    print_type(argv[i], builtin);
                    if(!print_all)
                    {
                        continue;
                    }
                }
                
                /* check if it is a defined shell function */
                if(search_funcs && is_function(argv[i]))
                {
                    print_type(argv[i], function);
                    if(!print_all)
                    {
                        continue;
                    }
                }
            }
            
            /* now search for an external command with the given name */
            char *path = get_hashed_path(argv[i]);
            char *hpath = path;
            
            /* no hashed path found. search in $PATH */
            if(!path)
            {
                path = search_path(argv[i], NULL, 1);
            }
            
            /* nothing found */
            if(!path)
            {
                if(print_word)
                {
                    printf("\n");
                }
                else
                {
                    printf("%s is unknown\n", argv[i]);
                }
                res = 3;
                continue;
            }
            else
            {
                /* path found */
                if(print_word)
                {
                    if(print_path)
                    {
                        printf("%s\n", path);
                    }
                    else
                    {
                        printf("file\n");
                    }
                }
                else
                {
                    printf("%s is %s\n", argv[i], path);
                }
            }
            
            if(path != hpath)
            {
                free_malloced_str(path);
            }
        }
    }

    return res;
}
