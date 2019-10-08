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

/* default path to use if $PATH is NULL or undefined */
char *default_path = "/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin";


/*
 * follow POSIX's command search and execution, by checking first if the
 * command to be executed is a special builtin, a function, a regular builtin,
 * or an external command (in that order).. the first match is executed.
 * if the command name contains a slash '/', we treat it as the pathname of
 * the external command we should execute.
 */
int search_and_exec(struct source_s *src, int cargc, char **cargv, char *PATH, int flags)
{
    int dofork = flag_set(flags, SEARCH_AND_EXEC_DOFORK);
    int dofunc = flag_set(flags, SEARCH_AND_EXEC_DOFUNC);
    /* POSIX Command Search and Execution Algorithm:      */
    /* STEP 1: The command has no slash(es) in its name   */
    if(!strchr(cargv[0], '/'))
    {
        /* STEP 1-A: check for special builtin utilities      */
        if(do_special_builtin(cargc, cargv))
        {
            //free_symtab(symtab_stack_pop());
            return 0;
        }

        /* STEP 1-B: check for internal functions             */
        /* NOTE: Step 1-B is suppressed under 'command' invocation */
        if(dofunc)
        {
            if(do_function_definition(src, cargc, cargv))
            {
                //free_symtab(symtab_stack_pop());
                return 0;
            }
        }

        /* STEP 1-C: check for regular builtin utilities      */
        if(do_regular_builtin(cargc, cargv))
        {
            //free_symtab(symtab_stack_pop());
            return 0;
        }
        /* STEP 1-D: checked for in exec_cmd()                */
    }

    /* fork a new child process, if the caller asked for it */
    if(dofork)
    {
        return fork_command(cargc, cargv, PATH, UTILITY, 0, 0);
    }
    else
    {
        return  do_exec_cmd(cargc, cargv, PATH, NULL);
    }
}


/*
 * the command builtin utility (POSIX).. used to execute a builtin or external command,
 * ignoring shell functions declared with the same name.. used also to print information
 * about the type (and path) of commands, functions and utilities.. returns 0 if the
 * command was found and executed, non-zero otherwise.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help command` or `command -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int command(int argc, char **argv)
{
    int i;
    int print_path       = 0;
    int print_interp     = 0;
    int use_default_path = 0;

    /* loop on the arguments and parse the options, if any */
    for(i = 1; i < argc; i++)
    { 
        /* options start with '-' */
        if(argv[i][0] == '-')
        {
            char *p = argv[i];
            /* stop patsing options if we hit special option '-' or '--' */
            if(strcmp(p, "-") == 0 || strcmp(p, "--") == 0)
            {
                i++;
                break;
            }
            /* skip the '-' and parse the options string */
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
                            fprintf(stderr, "%s: restricted shells can't use the -p option\n", SHELL_NAME);
                            return 3;
                        }
                        use_default_path = 1;
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
            /* first argument, stop paring options */
            break;
        }
    }
    
    /* missing argument(s) */
    if(i >= argc)
    {
        fprintf(stderr, "%s: missing argument: command name\n", UTILITY);
        return 2;
    }
    
    /* if the caller provided a path, use it. otherwise use the default path */
    char *PATH = use_default_path ? default_path : NULL;
    
    /* the -v option was used: print the command's path */
    if(print_path)
    {
        /* if the command name contains '/', print it as-is */
        if(strchr(argv[i], '/'))
        {
            printf("%s\n", argv[i]);
            return 0;
        }
        /* check if the command name is a defined alias */
        char *alias = parse_alias(argv[i]);
        if(alias != argv[i])
        {
            printf("alias %s='%s'\n", argv[i], alias);
            return 0;
        }
        /* check if the command name is a shell keyword, function, or builtin */
        if(is_keyword (argv[i]) >= 0 || is_builtin (argv[i]) || is_function(argv[i]))
        {
            printf("%s\n", argv[i]);
            return 0;
        }
        /* search the command name in the executable path */
        char *path = search_path(argv[i], PATH, 1);
        if(!path)
        {
            /* we don't know what this command is */
            printf("%s is unknown\n", argv[i]);
            return 3;
        }
        /* print the external command's path */
        printf("%s\n", path);
        free_malloced_str(path);
        return 0;
    }
    
    /* the -V option was used: print our interpretation of the command */
    if(print_interp)
    {
        /* if the command name contains '/', print it as-is */
        if(strchr (argv[i], '/'))
        {
            printf("%s is %s\n", argv[i], argv[i]);
            return 0;
        }
        /* check if the command name is a defined alias */
        char *alias = parse_alias(argv[i]);
        if(alias != argv[i])
        {
            printf("%s is aliased to '%s'\n", argv[i], alias);
            return 0;
        }
        /* check if the command name is a shell keyword */
        if(is_keyword (argv[i]) >= 0)
        {
            printf("%s is a shell keyword\n", argv[i]);
            return 0;
        }
        /* check if the command name is a shell builtin */
        if(is_builtin (argv[i]) )
        {
            printf("%s is a shell builtin\n", argv[i]);
            return 0;
        }
        /* check if the command name is a shell function */
        if(is_function(argv[i]) )
        {
            printf("%s is a shell function\n", argv[i]);
            return 0;
        }
        /* search the command name in the executable path */
        char *path = search_path(argv[i], PATH, 1);
        if(!path)
        {
            /* we don't know what this command is */
            printf("%s is unknown\n", argv[i]);
            return 3;
        }
        /* print the external command's path */
        printf("%s is %s\n", argv[i], path);
        free_malloced_str(path);
        return 0;
    }
    
    /*
     * neither -v nor -V was supplied. run the command using POSIX search and
     * execution Algorithm.
     */
    int    cargc = argc-i;
    char **cargv = &argv[i];
    return search_and_exec(NULL, cargc, cargv, PATH, SEARCH_AND_EXEC_DOFORK);
}
