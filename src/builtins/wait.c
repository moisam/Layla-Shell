/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: wait.c
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

#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include "../cmd.h"

#define UTILITY         "wait"

/* defined in jobs.c */
int rip_dead(pid_t pid);
extern struct job __jobs[];


int __wait(int argc, char **argv)
{
    int    res      = 0;
    char  *arg      = 0;
    pid_t  pid      = 0;
    int    wait_any = 0;
    int    force    = 0;
    struct job *job;
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvnf", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_WAIT, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'n':
                wait_any = 1;
                break;
                
            case 'f':
                force = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 1;
    
    int i, j;
    /* no pid perands -- wait for all children */
    if(v >= argc)
    {
        /*
         * bash says we can't kill jobs if job control is not active, which makes sense.
         * of course, wait should wait for shell's child processes to exit, but we don't
         * keep a list of our child process, the thing which we should fix!
         */
        if(!option_set('m'))
        {
            fprintf(stderr, "%s: can't wait on jobs: job control is not active\r\n", UTILITY);
            return 2;
        }
        
        for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
        {
            if(job->job_num != 0)
            {
                if(WIFEXITED(job->status) || WIFSTOPPED(job->status)) continue;
                if(force)
                {
                    kill(-job->pgid, SIGCONT);
                    kill(-job->pgid, SIGKILL);
                }
                /* wait for all processes in job to exit */
                j = 1;
                for(i = 0; i < job->proc_count; i++, j <<= 1)
                {
                    if(job->child_exitbits & j) continue;
                    pid = job->pids[i];
                    if(waitpid(pid, &res, 0) < 0)
                    {
                        if(errno == EINTR)
                        {
                            /* in tcsh, an interactive shell interrupted by a signal prints the list of jobs */
                            if(option_set('i')) jobs(1, (char *[]){ "jobs", NULL });
                            /* return 128 to indicate we were interrupted by a signal */
                            return 128;
                        }
                        res = rip_dead(pid);
                    }
                    set_pid_exit_status(job, pid, res);
                }
                set_job_exit_status(job, job->pgid, job->status);
                set_exit_status(res, 1);
                notice_termination(pid, res);
                if(wait_any) return job->status;
            }
        }
        return 0;
    }    
    v--;
    
read_next2:
    if(++v >= argc) return res;
    arg = argv[v];
    /* (a) argument is a job ID number */
    if(*arg == '%')
    {
        /* bash says we can't wait on jobs if job control is not active, which makes sense */
        if(!option_set('m'))
        {
            fprintf(stderr, "%s: can't kill job %s: job control is not active\r\n", UTILITY, arg);
            return 2;
        }

        pid = get_jobid(arg);
        job = get_job_by_jobid(pid);
        if(pid == 0 || !job)
        {
            fprintf(stderr, "%s: invalid job id: %s\r\n", UTILITY, arg);
            res = 127;
            goto read_next2;
        }
        /* wait for all processes in job to exit */
        j = 1;
        if(force)
        {
            kill(-job->pgid, SIGCONT);
            kill(-job->pgid, SIGKILL);
        }
        for(i = 0; i < job->proc_count; i++, j <<= 1)
        {
            if(job->child_exitbits & j) continue;
            pid = job->pids[i];
            if(waitpid(pid, &res, 0) < 0)
            {
                if(errno == EINTR)
                {
                    /* in tcsh, an interactive shell interrupted by a signal prints the list of jobs */
                    if(option_set('i')) jobs(1, (char *[]){ "jobs", NULL });
                    /* return 128 to indicate we were interrupted by a signal */
                    return 128;
                }
                res = rip_dead(pid);
            }
            set_pid_exit_status(job, pid, res);
        }
        set_job_exit_status(job, job->pgid, job->status);
        res = job->status;
    }
    /* (b) argument is a pid */
    else
    {
        pid = atoi(arg);
        if(force)
        {
            kill(pid, SIGCONT);
            kill(pid, SIGKILL);
        }
         /*
          * error fetching child exit status. of all the possible causes,
          * most probably is the fact that there is no children (in our case).
          * which probably means that the exit status was collected
          * in the SIGCHLD_handler() function in main.c.
          */
        if(waitpid(pid, &res, 0) < 0)
        {
            if(errno == EINTR)
            {
                /* in tcsh, an interactive shell interrupted by a signal prints the list of jobs */
                if(option_set('i')) jobs(1, (char *[]){ "jobs", NULL });
                /* return 128 to indicate we were interrupted by a signal */
                return 128;
            }
            res = rip_dead(pid);
        }
    }
    set_exit_status(res, 1);
    notice_termination(pid, res);
    if(wait_any) return res;
    goto read_next2;
}
