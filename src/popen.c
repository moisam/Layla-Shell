/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "cmd.h"
#include "builtins/setx.h"
#include "backend/backend.h"
#include "debug.h"


/*
 * initialize a subshell's environment.
 */
void init_subshell()
{
    /* make sure we have the shell's PGID */
    setpgid(0, tty_pid);

    /* reset the -dumpast option if set */
    set_option('d', 0);

    /* turn off job control */
    set_option('m', 0);

    /* turn off the interactive mode */
    set_option('i', 0);

    /* reset signals, traps and stdin */
    asynchronous_prologue();

    /* forget about all aliases (they are an interactive feature, anyway) */
    unset_all_aliases();

    /* indicate we are in a subshell */
    inc_subshell_var();

    /* export environment variables and functions */
    do_export_vars();

    /*
     * reset the DEBUG trap if -o functrace (-T) is not set, and the ERR trap
     * if -o errtrace (-E) is not set. traced functions inherit both traps
     * from the calling shell (bash).
     */
    if(!option_set('T'))
    {
        save_trap("DEBUG" );
        save_trap("RETURN");
    }
    if(!option_set('E'))
    {
        save_trap("ERR");
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
 * increment the value of the $SUBSHELL variable when we're running a subshell.
 */
void inc_subshell_var()
{
    subshell_level++;
    char buf[8];
    //sprintf(buf, "%d", subshell);
    sprintf(buf, "%d", subshell_level);
    struct symtab_entry_s *entry = get_symtab_entry("SUBSHELL");
    if(!entry)
    {
        entry = add_to_symtab("SUBSHELL");
    }
    entry->flags |= FLAG_EXPORT;
    symtab_entry_setval(entry, buf);
}


/*
 * set the value of $SHLVL to that of $SUBSHELL.
 */
void set_shlvl_var()
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
    if(!entry)
    {
        return;
    }

    char buf[8];
    sprintf(buf, "%d", subshell_level);
    symtab_entry_setval(entry, buf);

    /* bash doesn't mark $SHLVL as readonly. but better safe than sorry */
    entry->flags |= FLAG_READONLY;
    entry->flags |= FLAG_EXPORT;
}


/*
 * this call is similar to popen(), except it sets the environment in the subshell
 * by exporting variables and function definitions. the 'r' suffix is because we
 * open the pipe for reading, equivalent to calling pipe(cmd, "r").
 */

FILE *popenr(char *cmd)
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
        char *cmd2 = word_expand_to_str(cmd);
        if(cmd2)
        {
            cmd = cmd2;
        }
        
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
         * execute the command. we imitate POSIX's definition of popen(), where the call to popen()
         * results in behaviour akin to:
         * 
         *      execl(shell path, "sh", "-c", command, (char *)0);
         * 
         * see this link: https://pubs.opengroup.org/onlinepubs/009695399/functions/popen.html
         * 
         * we first try executing our shell, using our argv[0]. if failed, we try /bin/sh.
         */
        errno = 0;
        execl(shell_argv[0], "sh", "-c", cmd, NULL);

        /* returning from execl means error */
        execl("/bin/sh", "sh", "-c", cmd, NULL);

        /* returning from execl means error */
        if(errno)
        {
            fprintf(stderr, "%s: failed to exec '%s': %s\n", SHELL_NAME, "/bin/sh", strerror(errno));
        }
        /* exit in error */
        if(errno == ENOEXEC)
        {
            exit(EXIT_ERROR_NOEXEC);
        }
        else if(errno == ENOENT)
        {
            exit(EXIT_ERROR_NOENT);
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
    else if(pid < 0)            /* error */
    {
        fprintf(stderr, "%s: failed to fork subshell: %s\n", SHELL_NAME, strerror(errno));
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
        return fdopen(filedes[0], "r");
    }
}
