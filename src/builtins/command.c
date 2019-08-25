/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: command.c
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
#include <errno.h>
#include <wait.h>
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY         "command"
char *default_path = "/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin";


int search_and_exec(int cargc, char **cargv, char *PATH, int flags)
{
    int dofork = flag_set(flags, SEARCH_AND_EXEC_DOFORK);
    int dofunc = flag_set(flags, SEARCH_AND_EXEC_DOFUNC);
    /* POSIX Command Search and Execution Algorithm:      */
    /* STEP 1: The command has no slash(es) in its name   */
    if(!strchr(cargv[0], '/'))
    {
        symtab_stack_push();
        /* STEP 1-A: check for special builtin utilities      */
        if(do_special_builtin(cargc, cargv))
        {
            free_symtab(symtab_stack_pop());
            return 0;
        }
        /* STEP 1-B: check for internal functions             */
        /* NOTE: Step 1-B is suppressed under 'command' invocation */
        if(dofunc)
        {
            if(do_function_definition(cargc, cargv))
            {
                free_symtab(symtab_stack_pop());
                return 0;
            }
        }

        /* STEP 1-C: check for regular builtin utilities      */
        if(do_regular_builtin(cargc, cargv))
        {
            free_symtab(symtab_stack_pop());
            return 0;
        }
        /* STEP 1-D: checked for in exec_cmd()                */
    }

    if(dofork)
        return fork_command(cargc, cargv, PATH, UTILITY, 0, 0);
    else
        return  do_exec_cmd(cargc, cargv, PATH, NULL);
}


int command(int argc, char **argv)
{
    int i;
    int print_path       = 0;
    int print_interp     = 0;
    int use_default_path = 0;
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
                        print_help(argv[0], REGULAR_BUILTIN_COMMAND, 1, 0);
                        return 0;
                        
                    case 'v':
                        print_path       = 1;
                        break;
                        
                    case 'V':
                        print_interp     = 1;
                        break;
                        
                    case 'p':
                        if(startup_finished && option_set('r'))
                        {
                            /* r-shells can't use this option */
                            fprintf(stderr, "%s: restricted shells can't use the -p option\r\n", SHELL_NAME);
                            return 3;
                        }
                        use_default_path = 1;
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
    
    char *PATH = use_default_path ? default_path : NULL;
    
    if(print_path)
    {
        if(strchr (argv[i], '/'))  { printf("%s\r\n", argv[i]); return 0; }
        char *alias = __parse_alias(argv[i]);
        if(alias != argv[i])
        {
            printf("alias %s='%s'\r\n", argv[i], alias);
            return 0;
        }
        if(is_keyword (argv[i]) >= 0 || is_builtin (argv[i]) || is_function(argv[i]))
        {
            printf("%s\r\n", argv[i]);
            return 0;
        }
        char *path = search_path(argv[i], PATH, 1);
        if(!path)
        {
            printf("%s is unknown\r\n", argv[i]);
            return 3;
        }
        printf("%s\r\n", path);
        free_malloced_str(path);
        return 0;
    }
    
    if(print_interp)
    {
        if(strchr (argv[i], '/'))
        {
            printf("%s is %s\r\n", argv[i], argv[i]);
            return 0;
        }
        char *alias = __parse_alias(argv[i]);
        if(alias != argv[i])
        {
            printf("%s is aliased to '%s'\r\n", argv[i], alias);
            return 0;
        }
        if(is_keyword (argv[i]) >= 0)
        {
            printf("%s is a shell keyword\r\n", argv[i]);
            return 0;
        }
        if(is_builtin (argv[i]) )
        {
            printf("%s is a shell builtin\r\n", argv[i]);
            return 0;
        }
        if(is_function(argv[i]) )
        {
            printf("%s is a shell function\r\n", argv[i]);
            return 0;
        }
        char *path = search_path(argv[i], PATH, 1);
        if(!path)
        {
            printf("%s is unknown\r\n", argv[i]);
            return 3;
        }
        printf("%s is %s\r\n", argv[i], path);
        free_malloced_str(path);
        return 0;
    }
    

    int    cargc = argc-i;
    char **cargv = &argv[i];
    return search_and_exec(cargc, cargv, PATH, SEARCH_AND_EXEC_DOFORK);
}
