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
#include <sys/types.h>
#include "../cmd.h"
#include "../debug.h"

/* defined in jobs.c */
extern int cur_job ;
extern int prev_job;

#define UTILITY         "fg"

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
    int   status;
    printf("%s\r\n", job->commandstr);
    tcsetpgrp(0, job->pgid);
    /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
    if(job->tty_attr && isatty(0)) tcsetattr(0, TCSANOW, job->tty_attr);
    kill(-(job->pgid), SIGCONT);
    //resid = waitpid(job->pgid, &status, WAIT_FLAG);
    waitpid(job->pgid, &status, WAIT_FLAG);
    if(WIFSTOPPED(status) && option_set('m'))
    {
        if(cur_job != job->job_num) set_cur_job(job);
        notice_termination(job->pgid, status);
    }
    else
    {
        set_exit_status(status, 1);
        kill_job(job);
    }
    tcsetpgrp(0, tty_pid);
}


int fg(int argc, char **argv)
{
    if(!option_set('m'))
    {
        fprintf(stderr, "%s: job control is not enabled\r\n", UTILITY);
        return 2;
    }
    
    struct job *job;
    if(argc == 1)
    {
        job = get_job_by_jobid(get_jobid("%%"));
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %%%%\r\n", UTILITY);
            return 3;
        }
        __fg(job);
        return 0;
    }

    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hv", &v, 0)) > 0)
    {
        if     (c == 'h') { print_help(argv[0], REGULAR_BUILTIN_FG, 1, 0); }
        else if(c == 'v') { printf("%s", shell_ver)     ; }
    }
    /* unknown option */
    if(c == -1) return 1;
    if(v >= argc) return 0;
    
    for( ; v < argc; v++)
    {
        job = get_job_by_jobid(get_jobid(argv[v]));
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %s\r\n", UTILITY, argv[v]);
            return 3;
        }
        __fg(job);
    }
    return 0;
}
