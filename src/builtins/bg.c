/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: bg.c
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

#include <wait.h>
#include <signal.h>
#include <linux/input.h>
#include <linux/kd.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../include/debug.h"
#include "../symtab/symtab.h"
#include "../include/sig.h"

/* Defined in jobs.c */
extern int cur_job ;
extern int prev_job;


/*
 * Helper function to send the job indicated by the passed struct job_s to
 * the background. It prints a status message to stdout, then sends
 * SIGCONT to the job processes and sets $! to the PGID of the job.
 * 
 * Return 1 if the job is started, 0 on error.
 */
int do_bg(struct job_s *job)
{
    if(!flag_set(job->flags, JOB_FLAG_JOB_CONTROL))
    {
        PRINT_ERROR("bg", "job started without job control");
        return 0;
    }
    

    if(RUNNING(job->status))
    {
        PRINT_ERROR("bg", "job %d is already running in the background", 
                    job->job_num);
        /* Not an error (bash) */
        return 1;
    }

    job->flags &= ~(JOB_FLAG_FORGROUND|JOB_FLAG_NOTIFIED);
    
    /*
     * POSIX defines bg's output as:
     *     "[%d] %s\n", <job-number>, <command>
     * 
     */
    char current = ' ';
    char *cmd = job->commandstr;
    
    if(!option_set('P'))
    {
        current = (job->job_num == cur_job ) ? '+' :
                  (job->job_num == prev_job) ? '-' : ' ';
    }
    

    printf("[%d]%c %s\n", job->job_num, current, cmd);
    do_kill(-(job->pgid), SIGCONT, job);
    
    /* Set the $! special parameter */
    set_shell_vari("!", job->pgid);


    /*
     * Save the current job in the previous job, then set the last started job
     * as the current job.
     */
    reset_cur_job();
    
#if 0
    prev_job = cur_job;
    cur_job  = job->job_num;
#endif
    
    return 1;
}
