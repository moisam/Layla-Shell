/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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
#include <sys/stat.h>
#include "builtins.h"
#include "../cmd.h"
#include "../parser/parser.h"
#include "../debug.h"

#define UTILITY         "type"


static inline void print_type(char *arg, char *type, int print_word, int print_path)
{
    if(print_word)
    {
        printf("%s\n", print_path ? arg : type);
    }
    else
    {
        printf("%s is a shell %s\n", arg, type);
    }
}


/*
 * Print a string describing the type of command cmd. The who parameter 
 * contains the name of the calling utility (type, command, ...), which we
 * use in printing error messages. The PATH parameter is used to search
 * external commands. If PATH is NULL, the $PATH variable is used by
 * default. The flags parameter indicates whether the caller wants us to
 * print a single word describing the command, whether to write the full
 * pathname of external commands, whether to check for functions, write
 * hashed pathnames, or print all possible types of a command.
 */
int print_command_type(char *cmd, char *who, char *PATH, int flags)
{
    int print_path = flag_set(flags, TYPE_FLAG_PRINT_PATH);
    int print_word = flag_set(flags, TYPE_FLAG_PRINT_WORD);
    int print_funcs = flag_set(flags, TYPE_FLAG_PRINT_FUNCS);
    int print_builtins = flag_set(flags, TYPE_FLAG_PRINT_BUILTINS);
    int print_all = flag_set(flags, TYPE_FLAG_PRINT_ALL);
    int print_hashed = flag_set(flags, TYPE_FLAG_PRINT_HASHED);
    
    if(strchr(cmd, '/'))
    {
        /* argument contains slashes. treat as a pathname and print as-is */
        if(/* *who == 'c' && */ print_path)
        {
            printf("%s\n", cmd);
        }
        else
        {
            printf("%s is %s\n", cmd, cmd);
        }
    }
    else
    {
        int matches = 0;
        /* argument has no slashes. process it */
        if(print_builtins)
        {
            /* check if it is a defined alias */
            char *alias = get_alias_val(cmd);
            if(alias && alias != cmd)
            {
                /* 'type -p' prints nothing */
                if(flag_set(flags, TYPE_FLAG_PATH_ONLY))
                {
                    return 0;
                }
                
                if(print_word)
                {
                    if(print_path)
                    {
                        printf("%s\n", cmd);
                    }
                    else
                    {
                        printf("alias\n");
                    }
                }
                else
                {
                    /* print a slightly different message for command with -p */
                    if(print_path)
                    {
                        printf("alias %s=", cmd);
                    }
                    else
                    {
                        printf("%s is aliased to ", cmd);
                    }
                    
                    char *val = quote_val(alias, 1, 0);
                    if(val)
                    {
                        printf("%s\n", val);
                        free(val);
                    }
                    else
                    {
                        printf("\"\"\n");
                    }
                }

                if(!print_all)
                {
                    return 0;
                }
                matches++;
            }

            /* check if it is a keyword */
            if(is_keyword(cmd) >= 0)
            {
                /* 'type -p' prints nothing */
                if(flag_set(flags, TYPE_FLAG_PATH_ONLY))
                {
                    return 0;
                }
                
                print_type(cmd, "keyword", print_word, print_path);
                if(!print_all)
                {
                    return 0;
                }
                matches++;
            }
                
            /* check if it is a defined shell function */
            if(print_funcs && is_function(cmd))
            {
                /* 'type -p' prints nothing */
                if(flag_set(flags, TYPE_FLAG_PATH_ONLY))
                {
                    return 0;
                }
                
                print_type(cmd, "function", print_word, print_path);
                if(!print_all)
                {
                    return 0;
                }
                matches++;
            }

            /* check if it is a builtin utility */
            if(is_enabled_builtin(cmd))
            //if(is_builtin(cmd))
            {
                /* 'type -p' prints nothing */
                if(flag_set(flags, TYPE_FLAG_PATH_ONLY))
                {
                    return 0;
                }
                
                print_type(cmd, "builtin", print_word, print_path);
                if(!print_all)
                {
                    return 0;
                }
                matches++;
            }
        }
        
        /* force printing full message for the 'command' builtin */
        if(*who == 'c')
        {
            print_word = 0;
        }
        
        /* now search for an external command with the given name */
        char *path = NULL;
        int free_path = 0;
        /* use the given path or, if null, use $PATH */
        PATH = PATH ? : get_shell_varp("PATH", NULL);
        
        /* check for hashed pathnames (the type builtin) */
        if(print_hashed)
        {
            if((path = get_hashed_path(cmd)))
            {
                matches++;
            }
        }
        
        do
        {
            if(!path)
            {
                while((path = next_path_entry(&PATH, cmd, 0)))
                {
                    /* check if the file exists */
                    struct stat st;
                    if(stat(path, &st) == 0)
                    {
                        /* not a regular file */
                        if(!S_ISREG(st.st_mode) || access(path, X_OK) != 0)
                        {
                            free(path);
                            continue;
                        }
                        
                        matches++;
                        free_path = 1;
                        break;
                    }
                }
            }
            
            /* nothing found */
            if(!path)
            {
                if(!print_all || matches == 0)
                {
                    if(!print_word)
                    {
                        printf("%s is unknown\n", cmd);
                    }
                    return 3;
                }
                break;
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
                    printf("%s is %s\n", cmd, path);
                }
                
                if(free_path)
                {
                    free(path);
                }
                path = NULL;
            }
        } while(print_all);
    }
    
    return 0;
}


/*
 * The type builtin utility (POSIX). Used to print the type of an argument.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help type` or `type -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int type_builtin(int argc, char **argv)
{
    int i;
    int res = 0;
    int flags = TYPE_FLAG_PRINT_FUNCS | TYPE_FLAG_PRINT_BUILTINS;

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
            
            /* 
             * bash recognizes the obsolete -type, -path, and -all options, as
             * well as their long-option equivalents --type, --path, and --all.
             * here we recognize each option and convert it to it's short-option
             * equivalent.
             */
            if(strcmp(p, "type") == 0 || strcmp(p, "-type") == 0)
            {
                p = "t";
            }
            else if(strcmp(p, "path") == 0 || strcmp(p, "-path") == 0)
            {
                p = "p";
            }
            else if(strcmp(p, "all") == 0 || strcmp(p, "-all") == 0)
            {
                p = "a";
            }
            
            /* process the options string */
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], &TYPE_BUILTIN, 0);
                        return 0;
                        
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                    
                    /* print only one word to describe each argument */
                    case 't':
                        flags |= TYPE_FLAG_PRINT_WORD;
                        flags &= ~(TYPE_FLAG_PRINT_PATH | TYPE_FLAG_PRINT_HASHED);
                        break;
                        
                    case 'P':
                        /* force $PATH search */
                        flags |= (TYPE_FLAG_PRINT_PATH | TYPE_FLAG_PRINT_WORD);
                        flags &= ~(TYPE_FLAG_PRINT_HASHED | TYPE_FLAG_PRINT_BUILTINS);
                        break;
                    
                    /* print the command's path without searching for builtins and functions first */
                    case 'p':
                        flags |= (TYPE_FLAG_PRINT_HASHED | TYPE_FLAG_PRINT_PATH | TYPE_FLAG_PATH_ONLY);
                        flags |= TYPE_FLAG_PRINT_WORD;

                        if(startup_finished && option_set('r'))
                        {
                            /* r-shells can't use this option */
                            PRINT_ERROR("%s: restricted shells can't use the -%c option\n", SOURCE_NAME, *p);
                            return 3;
                        }
                        break;
                    
                    /* print all possible interpretations of each argument */
                    case 'a':
                        flags |= TYPE_FLAG_PRINT_ALL;
                        break;
                    
                    /* don't look in the functions table */
                    case 'f':
                        flags &= ~TYPE_FLAG_PRINT_FUNCS;
                        break;
                        
                    default:                        
                        PRINT_ERROR("%s: unknown option: %s\n", UTILITY, argv[i]);
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
        PRINT_ERROR("%s: missing argument: command name\n", UTILITY);
        return 2;
    }

    /* parse the arguments */
    for( ; i < argc; i++)
    {
        int res2 = print_command_type(argv[i], "type", NULL, flags);
        if(res2)
        {
            res = res2;
        }
    }

    return res;
}
