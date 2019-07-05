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

/* defined in export.c */
void do_export_table(struct symtab_s *symtab);


int exec(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int  v = 1, c;
    int  cenv = 0, login = 0;
    char *arg0 = NULL;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvca:l", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_EXEC, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'c':
                cenv = 1;
                break;
                
            case 'l':
                login = 1;
                break;
                
            case 'a':
                arg0 = __optarg;
                if(!arg0 || arg0 == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: missing argument to option -%c\r\n", UTILITY, c);
                    return 2;
                }
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc) return 0;

    /* is this shell restricted? */
    if(option_set('r'))
    {
        /* bash says r-shells can't use exec to replace the shell */
        fprintf(stderr, "%s: can't execute command: restricted shell\r\n", UTILITY);
        return 2;
    }
    
    /*
     * with -c option, clear the environment before applying variable assignments of this
     * command. local variable assignments can be found in the local symbol table that
     * the backend pushed on the stack before calling us.
     */
    if(cenv)
    {
        clearenv();
        struct symtab_s *symtab = get_local_symtab();
        do_export_table(symtab);
    }
    
    /* now exec */
    char *cmd = get_malloced_str(argv[v]);
    if(arg0)
    {
        free_malloced_str(argv[v]);
        argv[v] = get_malloced_str(arg0);
    }
    if(login)       /* place a dash in front of argv[0] */
    {
        char *l = malloc(strlen(argv[v])+2);
        if(!l)
        {
            fprintf(stderr, "%s: failed to exec '%s': insufficient memory\r\n", SHELL_NAME, argv[v]);
            if(cmd) free_malloced_str(cmd);
            return 1;
        }
        sprintf(l, "-%s", argv[v]);
        free_malloced_str(argv[v]);
        argv[v] = get_malloced_str(l);
        free(l);
    }
    execvp(cmd, &argv[v]);
    /* NOTE: we should NEVER come back here, unless there is an error, of course! */
    int err = errno;
    fprintf(stderr, "%s: failed to exec '%s': %s\r\n", SHELL_NAME, argv[v], strerror(errno));
    if(cmd) free_malloced_str(cmd);
    
    /* in bash, subshells exit unconditionally on exec() failure */
    if(getpid() != tty_pid)
        exit_gracefully(EXIT_FAILURE, NULL);
    
    /* in bash, a non-interactive shell exits here if execfail is not set */
    if(!option_set('i') && !optionx_set(OPTION_EXEC_FAIL))
        exit_gracefully(EXIT_FAILURE, NULL);
    
    /*
     * if the environment was cleared, try to restore it by re-exporting all the
     * export variables.
     */
    if(cenv)
    {
        do_export_vars();
    }
    
    /* return failure */
    if(err == ENOEXEC) return EXIT_ERROR_NOEXEC;
    if(err == ENOENT ) return EXIT_ERROR_NOENT ;    
    return 0;
}
