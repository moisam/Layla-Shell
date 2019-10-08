/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: coproc.c
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

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "../cmd.h"
#include "setx.h"
#include "../backend/backend.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY             "coproc"

/*
 * the coprocess uses two pipes: one for reading by the coprocess (written to
 * by the shell), the other for writing by the coprocess (read by the shell).
 * we save each pipe's file descriptors in the following global arrays.
 */
int rfiledes[2] = { -1, -1 };
int wfiledes[2] = { -1, -1 };


/*
 * the coproc builtin utility (non-POSIX).. used to fork a subshell (coprocess)
 * which we can assign tasks to. the coprocess runs in the background and we interact
 * with it using two pipes: one for reading, the other for writing. this utility is
 * special among the other builtin utilities in that we let it handle its own I/O
 * redirections, so that the redirections affect the coprocess only.
 * returns 0 if the coprocess was started successfully, 0 otherwise.
 * 
 * see the manpage, or run: `help coproc` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int coproc(int argc, char **argv, struct io_file_s *io_files)
{
    /* should have at least one argument */
    if(argc == 1)
    {
        fprintf(stderr, "%s: missing arguments\n", UTILITY);
        return 2;
    }
    
    char *pname = "COPROC";
    char  buf[16];
    pid_t pid;

    /* close old pipes */
    if(rfiledes[1] != -1)
    {
        close(rfiledes[1]);
        rfiledes[1] = -1;
    }
    if(wfiledes[0] != -1)
    {
        close(wfiledes[0]);
        wfiledes[0] = -1;
    }
    
    /* create two pipes */
    pipe(rfiledes);
    pipe(wfiledes);
    
    /* then start the coprocess */
    if((pid = fork_child()) == 0)     /* child process */
    {
        asynchronous_prologue();
        close(0);   /* stdin */
        dup  (rfiledes[0]);     /* read from the reading pipe */
        close(1);   /* stdout */
        dup  (wfiledes[1]);     /* write to the writing pipe */
        /* close unused file descriptors */
        close(rfiledes[0]);
        close(rfiledes[1]);
        close(wfiledes[0]);
        close(wfiledes[1]);
        /* perform any I/O redirections */
        __redirect_do(io_files, 0);
        errno = 0;
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
        /* increment the $SUBSHELL variable so we know we're in a subshell */
        inc_subshell_var();
        /* execute the command */
        int res = search_and_exec(NULL, argc-1, &argv[1], NULL, SEARCH_AND_EXEC_DOFUNC);
        /* if we returned, the command was not executed */
        if(errno)
        {
            fprintf(stderr, "%s: failed to exec '%s': %s\n", UTILITY, argv[1], strerror(errno));
        }
        /* exit with an appropriate exit code */
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
            exit(res);
        }
    }
    else if(pid < 0)            /* error */
    {
        fprintf(stderr, "%s: failed to fork: %s\n", UTILITY, strerror(errno));
        return 1;
    }
    else                        /* parent process */
    {
        /*
         * save the file descriptors to the symtab.
         * TODO: these file descriptors should not be available in subshells.
         */
        /* $COPROC1 - command input, shell output. similar to bash's $COPROC[1] */
        sprintf(buf, "%s1", pname);
        struct symtab_entry_s *entry = add_to_symtab(buf);
        sprintf(buf, "%d", rfiledes[1]);
        symtab_entry_setval(entry, buf);
        close(rfiledes[0]);                 /* close the other end, we will not use it */
        /*
         * set the close-on-exec flag. we could've used pipe2() to set this flag when we
         * created the pipe, but this would have caused the coprocess to fail after fork,
         * when it calls search_and_exec(), which calls do_exec_cmd(), which eventually 
         * calls exec().
         */
        fcntl(rfiledes[1], F_SETFD, FD_CLOEXEC);

        /* $COPROC0 - command output, shell input. similar to bash's $COPROC[0] */
        sprintf(buf, "%s0", pname);
        entry = add_to_symtab(buf);
        sprintf(buf, "%d", wfiledes[0]);
        symtab_entry_setval(entry, buf);
        close(wfiledes[1]);                 /* close the other end, we will not use it */
        /* same as above */
        fcntl(wfiledes[0], F_SETFD, FD_CLOEXEC);

        /* set the $COPROC_PID variable */
        sprintf(buf, "%s_PID", pname);
        entry = add_to_symtab(buf);
        sprintf(buf, "%d", pid);
        symtab_entry_setval(entry, buf);

        /* add as background job */
        char *cmdstr = list_to_str(&argv[1], 0);
        /* $! will be set in add_job() */
        struct job *job = add_job(pid, (pid_t[]){ pid }, 1, cmdstr, 1);
        set_cur_job(job);
        if(cmdstr)
        {
            free(cmdstr);
        }
        return 0;
    }
}
