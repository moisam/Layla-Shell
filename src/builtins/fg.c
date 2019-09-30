/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../debug.h"

/* defined in jobs.c */
extern int cur_job ;
extern int prev_job;

/* defined in ../backend/backend.c */
int wait_on_child(pid_t pid, struct node_s *cmd, struct job *job);

#define UTILITY         "fg"


/*
 * bring a job to the foreground.
 */
void __fg(struct job *job)
{
    /*
     * in tcsh, special alias jobcmd is run before running commands and when jobs
     * change state, or a job is brought to the foreground.
     */
    run_alias_cmd("jobcmd");
    
    /*
     * no need to check for option_set('m') here because it must be set,
     * otherwise this function would have never been called.
     */
    int   tty = isatty(0);
    printf("%s\n", job->commandstr);
    if(tty)
    {
        tcsetpgrp(0, job->pgid);
        /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
        if(job->tty_attr)
        {
            if(tcsetattr(0, TCSAFLUSH, job->tty_attr) == -1)
            {
                fprintf(stderr, "%s: failed to restore terminal attributes\n", SHELL_NAME);
            }
        }
    }
    /* continue the job and wait for it */
    kill(-(job->pgid), SIGCONT);
    wait_on_child(job->pgid, NULL, job);
    /* restore our foreground pgid */
    if(tty)
    {
        tcsetpgrp(0, tty_pid);
    }
}


/*
 * the fg builtin utility (POSIX).. used to bring a job to the foreground.
 * if more than one job is specified, brings the jobs, one at a time, to the
 * foreground, waiting for each to finish execution before resuming the next.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help fg` or `fg -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int fg(int argc, char **argv)
{
    /* job control must be on */
    if(!option_set('m'))
    {
        fprintf(stderr, "%s: job control is not enabled\n", UTILITY);
        return 2;
    }
    
    struct job *job;
    /* we have no job argument.. use the current job */
    if(argc == 1)
    {
        job = get_job_by_jobid(get_jobid("%%"));
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %%%%\n", UTILITY);
            return 3;
        }
        __fg(job);
        return 0;
    }

    /****************************
     * process the options
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    while((c = parse_args(argc, argv, "hv", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_FG, 1, 0);
                return 0;

            case 'v':
                printf("%s", shell_ver);
                return 0;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
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
            fprintf(stderr, "%s: unknown job: %s\n", UTILITY, argv[v]);
            return 3;
        }
        __fg(job);
    }
    return 0;
}
