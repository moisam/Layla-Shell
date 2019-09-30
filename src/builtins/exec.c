/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "setx.h"

#define UTILITY             "exec"


/*
 * the exec builtin utility (POSIX).. used to execute commands in the current shell
 * execution environment.. this utility should never return.. if it doesn, it means
 * the command was not executed.. the return status will be 127 if the command
 * wasn't found, or 126 if it wasn't executable, or 1 otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help exec` or `exec -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int exec(int argc, char **argv)
{
    int  v = 1, c;
    int  cenv = 0, login = 0;
    char *arg0 = NULL;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /*
     * process the options.
     *
     * POSIX exec does not accept options, so we check if we are running in the
     * --posix mode and, if so, we skip checking for options.
     */
    if(!option_set('P'))
    {
        while((c = parse_args(argc, argv, "hvca:l", &v, 1)) > 0)
        {
            switch(c)
            {
                /* print help */
                case 'h':
                    print_help(argv[0], SPECIAL_BUILTIN_EXEC, 0, 0);
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
                    arg0 = __optarg;
                    if(!arg0 || arg0 == INVALID_OPTARG)
                    {
                        fprintf(stderr, "%s: missing argument to option -%c\n", UTILITY, c);
                        return 2;
                    }
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
        fprintf(stderr, "%s: can't execute command: restricted shell\n", UTILITY);
        return 2;
    }
    /*
     * with the -c option, we clear the environment before applying variable assignments
     * for this command.. local variable assignments can be found in the local symbol
     * table that the backend pushed on the stack before it called us.
     */
    if(cenv)
    {
        clearenv();
        struct symtab_s *symtab = get_local_symtab();
        do_export_table(symtab, EXPORT_VARS_EXPORTED_ONLY);
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
            fprintf(stderr, "%s: failed to exec '%s': insufficient memory\n", SHELL_NAME, argv[v]);
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
    execvp(cmd, &argv[v]);

    /* NOTE: we should NEVER come back here, unless there is an error, of course! */
    int err = errno;
    fprintf(stderr, "%s: failed to exec '%s': %s\n", SHELL_NAME, argv[v], strerror(errno));
    if(cmd)
    {
        free_malloced_str(cmd);
    }
    
    /* in bash, subshells exit unconditionally on exec() failure */
    if(getpid() != tty_pid)
    {
        exit_gracefully(EXIT_FAILURE, NULL);
    }
    
    /* in bash, a non-interactive shell exits here if the execfail shopt option is not set */
    if(!option_set('i') && !optionx_set(OPTION_EXEC_FAIL))
    {
        exit_gracefully(EXIT_FAILURE, NULL);
    }
    
    /*
     * if the environment was cleared, try to restore it by re-exporting all the
     * export variables.
     */
    if(cenv)
    {
        do_export_vars(EXPORT_VARS_EXPORTED_ONLY);
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
