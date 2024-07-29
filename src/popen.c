/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020, 2024 (c)
 * 
 *    file: popen.c
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

#define _XOPEN_SOURCE   500     /* fdopen() */

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "include/cmd.h"
#include "include/sig.h"
#include "builtins/builtins.h"
#include "builtins/setx.h"
#include "backend/backend.h"
#include "include/debug.h"


/*
 * Initialize a subshell's environment.
 */
void init_subshell(void)
{
    /* make sure we have the shell's PGID */
    setpgid(0, shell_pid);


    if(!option_set('m'))
    {
        set_signal_handler(SIGINT , SIG_IGN);
        set_signal_handler(SIGQUIT, SIG_IGN);
    }

    /* reset the -dumpast option if set */
    set_option('d', 0);

    /* turn off job control */
    set_option('m', 0);

    /* turn off the interactive mode */
    interactive_shell = 0;

    /* forget about all aliases (they are an interactive feature, anyway) */
    unset_all_aliases();

    /* indicate we are in a subshell */
    inc_subshell_var();

    /* export environment variables and functions */
    do_export_vars(EXPORT_VARS_FORCE_ALL);

    /*
     * Reset the DEBUG trap if -o functrace (-T) is not set, and the ERR trap
     * if -o errtrace (-E) is not set. Traced functions inherit both traps
     * from the calling shell (bash).
     */
    if(!option_set('T'))
    {
        reset_trap("DEBUG");
        reset_trap("RETURN");
    }

    if(!option_set('E'))
    {
        reset_trap("ERR");
    }
    
    /*
     * the -e (errexit) option is reset in subshells if inherit_errexit
     * is not set (bash).
     */
    if(!optionx_set(OPTION_INHERIT_ERREXIT))
    {
        set_option('e', 0);
    }
}


/*
 * Increment the value of the $SUBSHELL variable when we're running a subshell.
 */
void inc_subshell_var(void)
{
    struct symtab_entry_s *entry = get_symtab_entry("SUBSHELL");
    if(!entry)
    {
        entry = add_to_symtab("SUBSHELL");
    }

    char buf[16];
    sprintf(buf, "%d", ++executing_subshell);
    symtab_entry_setval(entry, buf);

    /* bash doesn't mark $BASH_SUBSHELL as readonly. but better safe than sorry, right? */
    entry->flags |= (FLAG_READONLY | FLAG_EXPORT);
}


/*
 * Increment the value of $SHLVL by the given amount, typically 1 if the shell
 * is starting anew, or -1 if we're executing an exec command.
 */
void inc_shlvl_var(int amount)
{
    /*
     * increment the shell nesting level (bash and tcsh).
     * tcsh resets it to 1 in login shells.
     */
    struct symtab_entry_s *entry = get_symtab_entry("SHLVL");
    if(!entry)
    {
        entry = add_to_symtab("SHLVL");
    }

    char buf[8];
    shell_level += amount;
    sprintf(buf, "%d", shell_level);
    symtab_entry_setval(entry, buf);

    /* bash doesn't mark $SHLVL as readonly. but better safe than sorry, right? */
    entry->flags |= (FLAG_READONLY | FLAG_EXPORT);
}


/*
 * This function is similar to popen(), except it sets the environment in the 
 * subshell by exporting variables and function definitions. The 'r' suffix is 
 * because we open the pipe for reading, equivalent to calling pipe(cmd, "r").
 * The process id of the subshell is stored in procid if that is not NULL.
 */
FILE *popenr(char *cmd, pid_t *procid)
{
    if(!cmd)
    {
        return NULL;
    }

    int filedes[2] = { -1, -1 };
    pid_t pid;
    
    /* create new pipe */
    pipe(filedes);
    
    if((pid = fork_child()) == 0)     /* child process */
    {
        
        /* init our subshell environment */
        init_subshell();

        /* modify our standard streams */
        close(0);   /* stdin */
        open("/dev/null", O_RDONLY);
        close(1);   /* stdout */
        dup  (filedes[1]);
        close(filedes[0]);
        close(filedes[1]);

        /*
         * Execute the command. We imitate POSIX's definition of popen(), where the call to popen()
         * results in behaviour akin to:
         * 
         *      execl(shell path, "sh", "-c", command, (char *)0);
         * 
         * See this link: https://pubs.opengroup.org/onlinepubs/009695399/functions/popen.html
         */
        struct source_s src;
        src.buffer   = cmd;
        src.bufsize  = strlen(src.buffer);
        src.srctype  = SOURCE_CMDSTR;
        src.srcname  = NULL;
        src.curpos   = INIT_SRC_POS;

        parse_and_execute(&src);
        
        /* execute the EXIT trap (if any) */
        trap_handler(0);
        
        exit(exit_status);
    }
    else if(pid < 0)            /* error */
    {
        PRINT_ERROR(SHELL_NAME, "failed to fork subshell: %s", strerror(errno));
        return NULL;
    }
    else
    {
        close(filedes[1]);                 /* close the other end, we will not use it */
        
        /*
         * set the close-on-exec flag. we could've used pipe2() to set this flag when we
         * created the pipe, but this would have caused the coprocess to fail after fork,
         * when it calls calls execl().
         */
        fcntl(filedes[0], F_SETFD, FD_CLOEXEC);
        (*procid) = pid;

        return fdopen(filedes[0], "r");
    }
}
