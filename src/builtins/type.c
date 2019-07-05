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


int type(int argc, char **argv)
{
    int i;
    int res = 0;
    int print_path       = 0;
    int print_all        = 0;
    //int use_path         = 0;
    int print_word       = 0;
    int search_funcs     = 1;
    for(i = 1; i < argc; i++)
    { 
        if(argv[i][0] == '-')
        {
            char *p = argv[i];
            if(strcmp(p, "-") == 0 || strcmp(p, "--") == 0) { i++; break; }
            p++;
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
                        
                    case 'p':
                        print_path = 1;
                        if(option_set('r'))
                        {
                            /* r-shells can't use this option */
                            fprintf(stderr, "%s: restricted shells can't use the -%c option\r\n", SHELL_NAME, *p);
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
                        fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, argv[i]);
                        return 2;
                }
                p++;
            }
        }
        else break;
    }
    if(i >= argc)
    {
        fprintf(stderr, "%s: missing argument: command name\r\n", UTILITY);
        return 2;
    }
    for( ; i < argc; i++)
    {
        if(strchr (argv[i], '/'))
        {
            printf("%s is %s\r\n", argv[i], argv[i]);
        }
        else
        {
            if(!print_path)
            {
                char *alias = __parse_alias(argv[i]);
                if(alias != argv[i])
                {
                    if(print_word) printf("alias\r\n");
                    else
                    {
                        printf("%s is aliased to ", argv[i]);
                        purge_quoted_val(alias);
                        printf("\r\n");
                    }
                    if(!print_all) continue;
                }
                if(is_keyword (argv[i]) >= 0)
                {
                    if(print_word) printf("keyword\r\n");
                    else
                    {
                        printf("%s is a shell keyword\r\n", argv[i]);
                    }
                    if(!print_all) continue;
                }
                if(is_builtin (argv[i]))
                {
                    if(print_word) printf("builtin\r\n");
                    else
                    {
                        printf("%s is a shell builtin\r\n", argv[i]);
                    }
                    if(!print_all) continue;
                }
                if(search_funcs && is_function(argv[i]))
                {
                    if(print_word) printf("function\r\n");
                    else
                    {
                        printf("%s is a shell function\r\n", argv[i]);
                    }
                    if(!print_all) continue;
                }
            }
            char *path = get_hashed_path(argv[i]);
            char *hpath = path;
            if(!path) path = search_path(argv[i], NULL, 1);
            if(!path)
            {
                if(print_word) printf("\r\n");
                else printf("%s is unknown\r\n", argv[i]);
                res = 3;
                continue;
            }
            else
            {
                if(print_word)
                {
                    if(print_path) printf("%s\n", path);
                    else printf("file\r\n");
                }
                else printf("%s is %s\r\n", argv[i], path);
            }
            if(path != hpath) free_malloced_str(path);
        }
    }

    return res;
}
