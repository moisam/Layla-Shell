/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020, 2024 (c)
 * 
 *    file: exec.c
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

#define _DEFAULT_SOURCE         /* clearenv() */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../include/sig.h"
#include "setx.h"

#define UTILITY             "exec"

/* defined in kbdevent2.c */
extern struct termios tty_attr_old;


/*
 * The exec builtin utility (POSIX). Used to execute commands in the current shell
 * execution environment. This utility should never return. If it doesn, it means
 * the command was not executed. The return status will be 127 if the command
 * wasn't found, or 126 if it wasn't executable, or 1 otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help exec` or `exec -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int exec_builtin(int argc, char **argv)
{
    int  v = 1, c;
    int  cenv = 0, login = 0;
    char *arg0 = NULL;

    /*
     * Process the options.
     *
     * POSIX exec does not accept options, so we check if we are running in the
     * --posix mode and, if so, we skip checking for options.
     */
    if(!option_set('P'))
    {
        while((c = parse_args(argc, argv, "hvca:l", &v, FLAG_ARGS_PRINTERR)) > 0)
        {
            switch(c)
            {
                /* print help */
                case 'h':
                    print_help(argv[0], &EXEC_BUILTIN, 0);
                    return 0;

                    /* print shell version */
                case 'v':
                    printf("%s", shell_ver);
                    return 0;

                    /* clear the new command's environment */
                case 'c':
                    cenv = 1;
                    break;

                    /*
                     *  precede argv[0] with a '-', so the command, presumably a shell,
                     *  thinks it is a login shell.
                     */
                case 'l':
                    login = 1;
                    break;

                    /* specify the argument to pass as arg[0] to the command */
                case 'a':
                    if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                    {
                        OPTION_REQUIRES_ARG_ERROR(UTILITY, c);
                        return 2;
                    }
                    arg0 = internal_optarg;
                    break;
            }
        }

        /* unknown option */
        if(c == -1)
        {
            return 2;
        }
    }
    
    /* no arguments */
    if(v >= argc)
    {
        return 0;
    }
    
    /* is this shell restricted? */
    if(startup_finished && option_set('r'))
    {
        /* bash & zsh say r-shells can't use exec to replace the shell */
        PRINT_ERROR(UTILITY, "can't execute command: restricted shell");
        return 2;
    }
    
    /* get the command name */
    char *cmd = get_malloced_str(argv[v]);

    /* replace arg[0] with the argument we were given with the -a option */
    if(arg0)
    {
        free_malloced_str(argv[v]);
        argv[v] = get_malloced_str(arg0);
    }

    /* place a dash in front of argv[0] */
    if(login)
    {
        char *l = malloc(strlen(argv[v])+2);
        if(!l)
        {
            PRINT_ERROR(UTILITY, "failed to exec `%s`: insufficient memory", argv[v]);
            if(cmd)
            {
                free_malloced_str(cmd);
            }
            return 1;
        }
        
        sprintf(l, "-%s", argv[v]);
        free_malloced_str(argv[v]);
        argv[v] = get_malloced_str(l);
        free(l);
    }

    /* now exec the command */
    char *path = cmd;
    if(!strchr(cmd, '/'))
    {
        /* no slashes. search $PATH */
        path = search_path(cmd, NULL, 1);
    }

    int tried_exec = 0;
    if(path)
    {
        struct stat st;
        if(stat(path, &st) == 0)
        {
            if(S_ISREG(st.st_mode))
            {
                /* restore signals to their inherited values */
                restore_signals();
                
                /* stop job control and kill stopped jobs (bash) */
                if(option_set('m'))
                {
                    kill_all_jobs(SIGHUP, 0);
                }
                
                /* retore the terminal attributes */
                if(orig_tty_pgid)
                {
                    int tty = cur_tty_fd();
                    tcsetpgrp(tty, orig_tty_pgid);
                    setpgid(0, orig_tty_pgid);
                    tcsetattr(tty, TCSAFLUSH, &tty_attr_old);
                }

                /*
                 * With the -c option, we clear the environment before applying variable assignments
                 * for this command. Local variable assignments can be found in the local symbol
                 * table that the backend pushed on the stack before it called us.
                 */
                if(cenv)
                {
                    clearenv();
                    /*
                     * NOTE: bash doesn't export anything after clearing the environment,
                     *       (neither does zsh - according to the manpage).
                     * TODO: Should we export variables defined as part of the commandline,
                     *       such as x in the command `x=abc exec sh`?
                     */
                }
                else
                {
                    /*
                     * decrement $SHLVL so if the exec'ed command is another shell, it 
                     * will start with a correct value of $SHLVL (bash).
                     */
                    inc_shlvl_var(-1);

                    /* export the variables marked for export */
                    do_export_vars(EXPORT_VARS_FORCE_ALL);
                }

                /* execute the command */
                tried_exec = 1;
                execvp(path, &argv[v]);
            }
        }

        if(path != cmd)
        {
            free(path);
        }
    }

    /* NOTE: we should NEVER come back here, unless there is an error, of course! */
    int err = errno;
    PRINT_ERROR(UTILITY, "failed to exec `%s`: %s", 
                cmd ? cmd : argv[v], strerror(errno));
    
    /*
     * in bash, subshells exit unconditionally, and non-interactive shells exit
     * on exec() failure if the execfail shopt option is not set.
     */
    if(executing_subshell || (!interactive_shell && !optionx_set(OPTION_EXEC_FAIL)))
    {
        exit_gracefully(EXIT_FAILURE, NULL);
    }
    
    if(tried_exec)
    {
        if(cmd)
        {
            free_malloced_str(cmd);
        }

        /* initialize the terminal */
        if(read_stdin && interactive_shell)
        {
            init_tty();
        }

        /* restart signals */
        init_signals();
    
        /*
         * if the environment was cleared, try to restore it by re-exporting all the
         * export variables.
         */
        if(cenv)
        {
            do_export_vars(EXPORT_VARS_EXPORTED_ONLY);
        }
        else
        {
            /* environment wasn't cleared. just reset $SHLVL */
            inc_shlvl_var(1);
        }
    }
    
    /* return appropriate failure result */
    if(err == ENOEXEC)
    {
        return EXIT_ERROR_NOEXEC;
    }
    if(err == ENOENT )
    {
        return EXIT_ERROR_NOENT ;
    }
    return 1;
}
