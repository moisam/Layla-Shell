/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "builtins.h"
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY            "command"

/* default path to use if $PATH is NULL or undefined */
// char *COMMAND_DEFAULT_PATH = "/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin";
char *COMMAND_DEFAULT_PATH = "/bin:/usr/bin";


/*
 * Follow POSIX's command search and execution, by checking first if the
 * command to be executed is a special builtin, a function, a regular builtin,
 * or an external command (in that order). The first match is executed.
 * If the command name contains a slash '/', we treat it as the pathname of
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
        if(do_builtin(cargc, cargv, 1))
        {
            //free_symtab(symtab_stack_pop());
            return 0;
        }

        /* STEP 1-B: check for internal functions             */
        /* NOTE: Step 1-B is suppressed under 'command' invocation */
        if(dofunc)
        {
            struct symtab_entry_s *func = get_func(cargv[0]);
            if(func)
            {
                return !do_function_body(src, cargc, cargv);
            }
        }

        /* STEP 1-C: check for regular builtin utilities      */
        if(do_builtin(cargc, cargv, 0))
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
 * The command builtin utility (POSIX). Used to execute a builtin or external command,
 * ignoring shell functions declared with the same name. Used also to print information
 * about the type (and path) of commands, functions and utilities.
 * 
 * Returns 0 if the* command was found and executed, non-zero otherwise.
 * 
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help command` or `command -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int command_builtin(int argc, char **argv)
{
    int i;
    int flags = TYPE_FLAG_PRINT_FUNCS | TYPE_FLAG_PRINT_BUILTINS;
    int dont_run = 0;
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
                        print_help(argv[0], &COMMAND_BUILTIN, 0);
                        return 0;
                        
                    case 'v':
                        flags |= TYPE_FLAG_PRINT_PATH;
                        flags |= TYPE_FLAG_PRINT_WORD;
                        dont_run = 1;
                        break;
                        
                    case 'V':
                        flags &= ~TYPE_FLAG_PRINT_PATH;
                        flags &= ~TYPE_FLAG_PRINT_WORD;
                        dont_run = 1;
                        break;
                        
                    case 'p':
                        if(startup_finished && option_set('r'))
                        {
                            /* r-shells can't use this option */
                            PRINT_ERROR("%s: restricted shells cannot use the -p option\n",
                                        SOURCE_NAME);
                            return 3;
                        }
                        use_default_path = 1;
                        break;
                        
                    default:
                        PRINT_ERROR("%s: unknown option: -%c\n", UTILITY, *p);
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
        PRINT_ERROR("%s: missing argument: command name\n", UTILITY);
        return 2;
    }
    
    /*
     * If the caller provided a path, use it. Otherwise use the default path.
     * we try to get the default path by using POSIX's confstr() function, if
     * defined. Otherwise we use our own default value.
     */
    char *PATH = NULL;
    int res = 0;
    
    if(use_default_path)
    {
        PATH = get_default_path();
    }

    /* One of the following two options was supplied:
     * - the -v option prints the command path
     * - the -V option prints our interpretation of the command 
     */
    if(dont_run)
    {
        res = print_command_type(argv[i], "command", PATH, flags);
    }
    else
    {
        /*
         * Neither -v nor -V was supplied. Run the command using POSIX search and
         * execution Algorithm.
         */
        int    cargc = argc-i;
        char **cargv = &argv[i];
        res = search_and_exec(NULL, cargc, cargv, PATH, SEARCH_AND_EXEC_DOFORK) ?
                    exit_status : 1;
    }
    
    if(PATH && PATH != COMMAND_DEFAULT_PATH)
    {
        free(PATH);
    }
    return res;
}
