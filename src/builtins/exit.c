/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: exit.c
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

/* macro definitions needed to use sig*() */
#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include "builtins.h"
#include "../cmd.h"
#include "../kbdevent.h"
#include "../symtab/symtab.h"
#include "setx.h"
#include "../symtab/string_hash.h"
#include "../debug.h"

/*
 * Flag to let us know if the user has already tried to exit before. We use this
 * when the user tries to exit while having running jobs. In this case we print an
 * alert message and return (without exiting). If the user re-runs `exit` immediately,
 * we kill all the jobs and continue with the exit. If the user doesn't run `exit`
 * immediately, but runs any other command, the flag is cleared, so that when they run
 * `exit` again, the cycle repeats.
 */
int tried_exit = 0;

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;


/*
 * The exit builtin utility (POSIX). Exits the shell, flusing command history to the
 * history file, and freeing used buffers. Doesn't return on success (the shell exits).
 * if passed an argument, it is regarded as the numeric exit status code we will pass
 * back to our parent process.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help exit` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int exit_builtin(int argc, char **argv)
{
    int i;

    /* more than 2 args is an error */
    if(argc > 2)
    {
        PRINT_ERROR("%s: too many arguments\n", argv[0]);
        return 1;
    }

    /*
     *  If given 2 args, get the exit status code passed as argv[1] (we only need
     *  the lower 8 bits of it). Otherwise, we will use the exit status of the
     *  last command executed (as per POSIX).
     */
    if(argc == 2)
    {
        char *strend = NULL;
        i = strtol(argv[1], &strend, 10);
        if(*strend)
        {
            PRINT_ERROR("%s: invalid exit status: %s\n", argv[0], argv[1]);
            return 2;
        }
        else
        {
            exit_status = (i & 0xff);
        }
    }

    /*
     * Similar to bash and ksh, alert the user for the presence of running/suspended
     * jobs. If the user insists on exiting, don't alert them the 2nd time.
     * If we're running this utility as a foreground job in the shell's process,
     * we pass the current job structure to pending_jobs(), so that it won't count
     * this job in the returned tally number.
     */
    if(interactive_shell && !tried_exit /* && getpid() == shell_pid && 
       tcgetpgrp(cur_tty_fd()) == shell_pid */ )
    {
        if((i = pending_jobs()))
        {
            /* interactive login shell will kill all jobs in exit_gracefully() below */
            char *noun = (i == 1) ? "job" : "jobs";
            fprintf(stderr, "You have %d pending %s.\n", i, noun);
            
            /* list the pending jobs (bash extension) */
            if(optionx_set(OPTION_CHECK_JOBS))
            {
                jobs_builtin(1, (char *[]){ "jobs" });
            }

            tried_exit = 1;
            return 1;
        }
    }

    exit_gracefully(exit_status, NULL);
    __builtin_unreachable();
}


/*
 * The last step in exiting the shell.
 *
 * The optional *errmsg argument is an error message to be output
 * before exiting.
 */
void exit_gracefully(int stat, char *errmsg)
{
    /* execute the EXIT trap (if any) */
    trap_handler(0);
    
    /*
     *  flush history list to the history file if the shell is interactive and
     *  the save_hist extended option is set.
     */
    if(interactive_shell && optionx_set(OPTION_SAVE_HIST))
    {
        flush_history();
    }
    
    /* perform logout scripts on login shell's exit */
    if(option_set('L') && !executing_subshell)
    {
        /* 
         * Don't interrupt us while we perform logout actions. This is
         * what tcsh does on logout.
         */
        struct source_s src;
        sigset_t intmask;
        sigemptyset(&intmask);
        sigaddset(&intmask, SIGCHLD);
        sigaddset(&intmask, SIGINT );
        sigaddset(&intmask, SIGQUIT);
        sigaddset(&intmask, SIGTERM);
        sigprocmask(SIG_BLOCK, &intmask, NULL);
    
        /* local logout scripts */
        if(read_file("~/.lshlogout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }
        else if(read_file("~/.logout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }

        /* global logout scripts */
        if(read_file("/etc/lshlogout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }
        else if(read_file("/etc/logout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }

        sigprocmask(SIG_UNBLOCK, &intmask, NULL);

        /*
         * save the dirstack if login shell (OPTION_SAVE_DIRS must be set to 
         * save the dirstack).
         */
        if(optionx_set(OPTION_SAVE_DIRS))
        {
            save_dirstack(NULL);
        }
    }

    /* interactive login shells send SIGHUP to jobs on exit (bash) */
    if(interactive_shell && option_set('L') && optionx_set(OPTION_HUP_ON_EXIT))
    {
        kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
    }

    /* check if we have an error message and if so, print it. */
    if(errmsg)
    {
        PRINT_ERROR("%s: %s\n", SOURCE_NAME, errmsg);
    }
    
    /* flush any hanging messages in the output streams */
    fflush(stdout);
    fflush(stderr);

    /* restore the terminal's canonical mode (if we're reading from it) */
    if(read_stdin && interactive_shell)
    {
        set_tty_attr(cur_tty_fd(), &tty_attr_old);
    }
    
    /* say bye bye! */
    exit(stat);
}
