/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: fg.c
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

#include <unistd.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/input.h>
#include <linux/kd.h>
#include "builtins.h"
#include "../cmd.h"
#include "../sig.h"
#include "../backend/backend.h"
#include "../debug.h"

/* defined in jobs.c */
extern int cur_job ;
extern int prev_job;

#define UTILITY         "fg"


/*
 * bring a job to the foreground.
 */
int do_fg(struct job_s *job)
{
    if(!flag_set(job->flags, JOB_FLAG_JOB_CONTROL))
    {
        PRINT_ERROR("%s: job started without job control\n", UTILITY);
        return 0;
    }
    
    sigset_t sigset;
    SIGNAL_BLOCK(SIGCHLD, sigset);

    job->flags |= JOB_FLAG_FORGROUND;
    job->flags &= ~JOB_FLAG_NOTIFIED;
    
    /*
     * in tcsh, special alias jobcmd is run before running commands and when jobs
     * change state, or a job is brought to the foreground.
     */
    run_alias_cmd("jobcmd");
    
    /*
     * no need to check for option_set('m') here because it must be set,
     * otherwise this function would have never been called.
     */
    int tty = cur_tty_fd();
    printf("%s\n", job->commandstr);

    /* save the terminal's attributes (bash, zsh) */
    struct termios *attr = save_tty_attr();
    
    /* restore the terminal attributes to what it was when the job was suspended (zsh) */
    if(job->tty_attr)
    {
        if(!set_tty_attr(tty, job->tty_attr))
        {
            PRINT_ERROR("%s: failed to restore terminal attributes\n", SOURCE_NAME);
        }
    }

    /* tell the terminal about the new foreground pgid */
    tcsetpgrp(tty, job->pgid);

    /* continue the job and wait for it */
    kill(-(job->pgid), SIGCONT);
    SIGNAL_UNBLOCK(sigset);
    wait_on_child(job->pgid, NULL, job);
    
    /* restore our foreground pgid */
    tcsetpgrp(tty, shell_pid);

    /* restore the terminal's attributes */
    set_tty_attr(tty, attr);
    
    return 1;
}


/*
 * The fg builtin utility (POSIX). Used to bring a job to the foreground.
 * If more than one job is specified, brings the jobs, one at a time, to the
 * foreground, waiting for each to finish execution before resuming the next.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help fg` or `fg -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int fg_builtin(int argc, char **argv)
{
    struct job_s *job;

    /* fg only works if job control is enabled (the monitor '-m' option is set) */
    if(!option_set('m'))
    {
        PRINT_ERROR("%s: job control is not active\n", UTILITY);
        return 1;
    }
    
    /* we have no job argument.. use the current job */
    if(argc == 1)
    {
        job = get_job_by_jobid(get_jobid("%%"));
        if(!job)
        {
            PRINT_ERROR("%s: unknown job: %%%%\n", UTILITY);
            return 3;
        }
        return !do_fg(job);
    }

    /****************************
     * process the options
     ****************************/
    int v = 1, res = 0, c;
    while((c = parse_args(argc, argv, "hv", &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &FG_BUILTIN, 0);
                return 0;

            case 'v':
                printf("%s", shell_ver);
                return 0;
        }
    }
    
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* no job arguments */
    if(v >= argc)
    {
        return 0;
    }
    
    /* process the job arguments */
    for( ; v < argc; v++)
    {
        job = get_job_by_jobid(get_jobid(argv[v]));
        if(!job)
        {
            PRINT_ERROR("%s: unknown job: %s\n", UTILITY, argv[v]);
            return 3;
        }
        res = !do_fg(job);
    }
    return res;
}
