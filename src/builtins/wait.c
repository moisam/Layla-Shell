/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include "builtins.h"
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY         "wait"

/* defined in jobs.c */
int rip_dead(pid_t pid);
extern struct job_s jobs_table[];


/*
 * If the wait() call is interrupted by a signal, clear the waiting_pid flag,
 * perform any pending traps, and return 128.
 */
int wait_interrupted(void)
{
    /* in tcsh, an interactive shell interrupted by a signal prints the list of jobs */
    if(interactive_shell)
    {
        jobs_builtin(1, (char *[]){ "jobs", NULL });
    }
    
    /* execute any pending traps */
    waiting_pid = 0;
    do_pending_traps();
    
    /* return 128 to indicate we were interrupted by a signal */
    return 128;
}


/*
 * Wait for any child process and return it's exit status, or 0 if no child
 * processes are running. If wait_jobs is non-zero, wait for any job and return
 * it's exit status. If wait_jobs is zero, wait for all child processes and
 * return 0.
 */
int wait_for_any(int wait_jobs, int force)
{
    int    res = 0;
    pid_t  pid = 0;
    int    tty = cur_tty_fd();
    int    i, j;
    struct job_s *job;
    force = force && option_set('m');

    /* if no job control, wait for any child process */
    if(wait_jobs)
    {
        for(job = &jobs_table[0]; job < &jobs_table[MAX_JOBS]; job++)
        {
            if(job->job_num != 0)
            {
                if(WIFEXITED(job->status) || WIFSTOPPED(job->status))
                {
                    continue;
                }
                
                /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
                if(job->tty_attr)
                {
                    set_tty_attr(tty, job->tty_attr);
                }
                
                /* wait for all processes in job to exit */
                j = 1;
                for(i = 0; i < job->proc_count; i++, j <<= 1)
                {
                    if(job->child_exitbits & j)
                    {
                        continue;
                    }
                    pid = job->pids[i];
                    waiting_pid = pid;

                    if(force)
                    {
                        kill(pid, SIGCONT);
                        kill(pid, SIGKILL);
                    }
                
                    if(waitpid(pid, &res, 0) < 0)
                    {
                        if(errno == EINTR)
                        {
                            return wait_interrupted();
                        }
                        res = rip_dead(pid);
                    }

                    waiting_pid = 0;
                    set_pid_exit_status(job, pid, res);
                }
                
                set_job_exit_status(job, job->pgid, job->status);
                set_exit_status(res);

                int saveb = option_set('b');
                set_option('b', 1);
                notice_termination(pid, res, 0);
                set_option('b', saveb);
                
                return job->status;
            }
        }
    }
    else
    {
        while((pid = waitpid(-1, &res, 0)) > 0)
        {
            rip_dead(pid);
        }

    }
    
    return 0;
}


/*
 * The wait builtin utility (POSIX). Used to wait for a process or job to complete and
 * returns the exit status of the completed process or job.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help wait` or `wait -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int wait_builtin(int argc, char **argv)
{
    int    res      = 0;
    char  *arg      = 0;
    pid_t  pid      = 0;
    int    wait_any = 0;
    int    force    = 0;
    struct job_s *job;
    int    v = 1, c;
    int    i, j;
    int    tty = cur_tty_fd();
    
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvnf", &v, FLAG_ARGS_ERREXIT|FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &WAIT_BUILTIN, 0);
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
    if(c == -1)
    {
        return 2;
    }
    
    /* no pid operands -- wait for all children */
    if(v >= argc)
    {
        return wait_for_any(wait_any, force);
    }    

    v--;

    int saveb = option_set('b');
    set_option('b', 1);
    
read_next2:
    
    if(++v >= argc)
    {
        set_option('b', saveb);
        return res;
    }
    arg = argv[v];
    
    /* (a) argument is a job ID number */
    if(*arg == '%')
    {
        /* bash says we can't wait on jobs if job control is not active, which makes sense */
        if(!option_set('m'))
        {
            PRINT_ERROR("%s: can't kill job %s: job control is not active\n", UTILITY, arg);
            return 2;
        }

        pid = get_jobid(arg);
        job = get_job_by_jobid(pid);
        if(pid == 0 || !job)
        {
            PRINT_ERROR("%s: invalid job id: %s\n", UTILITY, arg);
            res = 127;
            goto read_next2;
        }
        
        /* wait for all processes in job to exit */
        j = 1;
        
        /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
        if(job->tty_attr)
        {
            set_tty_attr(tty, job->tty_attr);
        }
        
        if(force)
        {
            kill(-job->pgid, SIGCONT);
            kill(-job->pgid, SIGKILL);
        }
        
        for(i = 0; i < job->proc_count; i++, j <<= 1)
        {
            if(job->child_exitbits & j)
            {
                continue;
            }
            pid = job->pids[i];
            waiting_pid = pid;
            
            if(waitpid(pid, &res, 0) < 0)
            {
                if(errno == EINTR)
                {
                    return wait_interrupted();
                }
                res = rip_dead(pid);
            }

            waiting_pid = 0;
            set_pid_exit_status(job, pid, res);
        }
        set_job_exit_status(job, job->pgid, job->status);
        res = job->status;

        notice_termination(pid, res, 0);
    }
    /* (b) argument is a pid */
    else
    {
        char *strend = NULL;
        pid = strtol(arg, &strend, 10);
        
        /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
        if(*strend || pid == 0)
        {
            PRINT_ERROR("%s: invalid pid: %s\n", UTILITY, arg);
            res = 127;
            goto read_next2;
        }
        
        job = get_job_by_any_pid(pid);
        if(job && job->tty_attr)
        {
            set_tty_attr(tty, job->tty_attr);
        }
        
        if(force)
        {
            kill(pid, SIGCONT);
            kill(pid, SIGKILL);
        }
        
        waiting_pid = pid;
        if(waitpid(pid, &res, 0) < 0)
        {
            /*
             * Error fetching child exit status. of all the possible causes,
             * most probably is the fact that there is no children (in our case).
             * Which probably means that the exit status was collected
             * in the SIGCHLD_handler() function in main.c.
             */
            if(errno == EINTR)
            {
                return wait_interrupted();
            }
            res = rip_dead(pid);
        }
        waiting_pid = 0;
        print_status_message(NULL, pid, res, 1, stderr);
    }

    set_exit_status(res);
    
    if(wait_any)
    {
        return res;
    }
    
    goto read_next2;
}
