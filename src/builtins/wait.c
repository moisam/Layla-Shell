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

/* required macro definition for sig*() functions */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../include/sig.h"
#include "../backend/backend.h"
#include "../include/debug.h"

#define UTILITY         "wait"

/* special flag for wait_for_pid() */
#define WAIT_ANY        -1

/* defined in jobs.c */
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
 * Wait for the child process with the given pid until it changes state. If pid
 * is WAIT_ANY, waits for any child process. In this case, job should be NULL.
 * If force is non-zero, SIGCONT is sent to the process to wake it up before waiting.
 * 
 * Returns 0 in case of success, 1 in error. The exit status of the child process 
 * waited for is stored in the global exit_status variable.
 */
int wait_for_pid(struct job_s *job, pid_t pid, int force)
{
    int res, res2;
    int mark_notified = 1;
    
    if(force)
    {
        kill(pid, SIGCONT);
        kill(pid, SIGKILL);
    }
    
    waiting_pid = pid;
    debug ("pid = %d\n", pid);
    
wait:
    if((res2 = waitpid(pid, &res, 0)) < 0)
    {
        debug (NULL);
        if(errno == ECHILD)
        {
            /*
             * ECHILD means pid is not our child (or we have no children). 
             * Maybe pid was our child but it exited and we've collected 
             * its exit status. Check with rip_dead() and get the status if 
             * that is the case.
             */
            if(pid == WAIT_ANY)
            {
                PRINT_ERROR(UTILITY, "no children to wait for");
                set_internal_exit_status(127);
                return 1;
            }
            
            if((res = rip_dead(pid)) == -1)
            {
                PRINT_ERROR(UTILITY, "process %d is not a child of this shell", pid);
                set_internal_exit_status(127);
                return 1;
            }
        }
        else if(errno == EINTR)
        {
            if(signal_received && signal_received != SIGCHLD)
            {
                set_internal_exit_status(wait_interrupted());
                return 1;
            }
            
            /*
             * We were interrupted by the death of a child, but not the one we
             * want. Continue waiting.
             */
            if((res = rip_dead(pid)) == -1)
            {
                goto wait;
            }
        }
    }
    else if(pid == WAIT_ANY)
    {
        debug ("res2 = %d\n", res2);
        job = get_job_by_any_pid(res2);
        pid = res2;
        /* Don't mark the job as notified, wait_for_any() should do this */
        mark_notified = 0;
    }
    debug ("res = %d, pid = %d\n", res, pid);
    
    waiting_pid = 0;
    
    if(job)
    {
        set_pid_exit_status(job, pid, res);
        set_job_exit_status(job, pid, res);
        res = job->status;
        
        if(mark_notified)
        {
            job->flags |= JOB_FLAG_NOTIFIED;
        }
    }
    
    set_exit_status(res);
    
    remove_dead_jobs();
    
    return 0;
}


int wait_for_job(struct job_s *job, int force, int tty)
{
    /* save the terminal's attributes (bash, zsh) */
    struct termios *attr = save_tty_attr();

    /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
    if(job->tty_attr)
    {
        set_tty_attr(tty, job->tty_attr);
    }
    
    /* wait for all processes in job to exit */
    int i, j = 1;
    int res;
    pid_t pid;
    
    for(i = 0; i < job->proc_count; i++, j <<= 1)
    {
        if(job->child_exitbits & j)
        {
            continue;
        }

        pid = job->pids[i];
        res = wait_for_pid(job, pid, force);
        
        /* wait interrupted or an error occurred */
        if(res != 0)
        {
            /* restore the terminal's attributes */
            set_tty_attr(tty, attr);
            return res;
        }
    }
    
    res = job->status;
    set_exit_status(res);
    debug ("res = %d\n", res);
    
    /* restore the terminal's attributes */
    if(job->tty_attr)
    {
        set_tty_attr(tty, attr);
    }
    
#if 0
    int saveb = option_set('b');
    set_option('b', 1);
    notice_termination(pid, res, 0);
    set_option('b', saveb);
#endif
    
    return res;
}


/*
 * Wait for any child process and return it's exit status, or 0 if no child
 * processes are running. If wait_jobs is non-zero, wait for any job and return
 * it's exit status. If wait_jobs is zero, wait for all child processes and
 * return 0.
 */
int wait_for_any(int force)
{
    int    res = 0;
    struct job_s *job;
    sigset_t sigset;
    
    force = force && option_set('m');
    SIGNAL_BLOCK(SIGCHLD, sigset);
    
    /* Check for unreported dead jobs */
    for(job = &jobs_table[0]; job < &jobs_table[MAX_JOBS]; job++)
    {
        if(job->job_num != 0 && job->child_exits == job->proc_count)
        {
            if(!NOTIFIED_JOB(job))
            {
                goto fin;
            }
        }
    }
    
    SIGNAL_UNBLOCK(sigset);
    
    /* Wait for any child process */
    for( ; ; )
    {
        SIGNAL_BLOCK(SIGCHLD, sigset);
        
        /* Check there is a background job to wait for */
        for(job = &jobs_table[0]; job < &jobs_table[MAX_JOBS]; job++)
        {
            if(job->job_num != 0 && job->child_exits != job->proc_count && !FOREGROUND_JOB(job))
            {
                break;
            }
        }
        
        /* No job available to wait for */
        if(job == &jobs_table[MAX_JOBS])
        {
            SIGNAL_UNBLOCK(sigset);
            return 127;
        }
        
        SIGNAL_UNBLOCK(sigset);
        
        /* Wait for any child process to terminate */
        errno = 0;
        res = wait_for_pid(NULL, WAIT_ANY, force);
        
        if(errno == ECHILD)
        {
            return res;
        }
        
#if 0
        errno = 0;
        if((pid = waitpid(-1, &res, 0)) < 0)
        {
            if(errno == EINTR)
            {
                if(signal_received != SIGCHLD)
                {
                    return wait_interrupted();
                }
            }
        }
        else
        {
            if((job = get_job_by_jobid(pid)))
            {
                set_pid_exit_status(job, pid, res);
                set_job_exit_status(job, pid, res);
            }
        }
#endif
        
        SIGNAL_BLOCK(SIGCHLD, sigset);
        
        /* Check if there is any finished job */
        for(job = &jobs_table[0]; job < &jobs_table[MAX_JOBS]; job++)
        {
            debug ("job->child_exits = %d, job->proc_count = %d\n", job->child_exits, job->proc_count);
            if(job->job_num != 0 && job->child_exits == job->proc_count)
            {
                goto fin;
            }
        }
        
        SIGNAL_UNBLOCK(sigset);
    }
    
fin:
    res = job->status;
    set_exit_status(res);
    remove_job(job);
    SIGNAL_UNBLOCK(sigset);
    return res;
}


void wait_for_bg(int force)
{
    int    tty = cur_tty_fd();
    struct job_s *job;
    sigset_t sigset;
    
    force = force && option_set('m');
    
    /* Wait for any child process */
    for( ; ; )
    {
        SIGNAL_BLOCK(SIGCHLD, sigset);
        
        /* Check there is a background job to wait for */
        for(job = &jobs_table[0]; job < &jobs_table[MAX_JOBS]; job++)
        {
            if(job->job_num != 0 && job->child_exits != job->proc_count && !FOREGROUND_JOB(job))
            {
                break;
            }
        }
        
        /* No job available to wait for */
        if(job == &jobs_table[MAX_JOBS])
        {
            SIGNAL_UNBLOCK(sigset);
            break;
        }
        
        SIGNAL_UNBLOCK(sigset);
        
        /* Wait for this job to finish */
        wait_for_job(job, force, tty);
    }
    
    /* Mark all dead jobs as notified */
    pid_t last_async_job = get_shell_varl("!", 0);
    
    SIGNAL_BLOCK(SIGCHLD, sigset);
    
    for(job = &jobs_table[0]; job < &jobs_table[MAX_JOBS]; job++)
    {
        if(job->job_num != 0 && job->child_exits == job->proc_count && 
            (interactive_shell || job->pgid != last_async_job))
        {
            job->flags |= JOB_FLAG_NOTIFIED;
        }
    }
    
    SIGNAL_UNBLOCK(sigset);
    
    remove_dead_jobs();
    clear_deadlist();
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
    int    tty = cur_tty_fd();
    sigset_t sigset;
    
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvnf", &v, FLAG_ARGS_PRINTERR)) > 0)
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
    
    /* First make sure we do receive SIGCHLD in our signal handler */
    struct sigaction sigact, old_sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = SIGCHLD_handler;
    sigaction(SIGCHLD, &sigact, &old_sigact);

    /* The -n flag is used. Wait for any job */
    if(wait_any)
    {
        wait_for_any(force);
        sigaction(SIGCHLD, &old_sigact, NULL);
        return exit_status;
    }
    
    /* No pid operands. Wait for all children */
    if(v >= argc)
    {
        wait_for_bg(force);
        sigaction(SIGCHLD, &old_sigact, NULL);
        return 0;
    }

    /* Wait for the given pids and/or job specs */
    for( ; v < argc; v++)
    {
        arg = argv[v];
        
        /* (a) argument is a job ID number */
        if(*arg == '%')
        {
            SIGNAL_BLOCK(SIGCHLD, sigset);

            pid = get_jobid(arg);
            job = get_job_by_jobid(pid);
            
            SIGNAL_UNBLOCK(sigset);

            if(pid == 0 || !job)
            {
                INVALID_JOB_ERROR(UTILITY, arg);
                res = 127;
                continue;
            }
            
            /* wait for all processes in job to exit */
            debug ("pid = %d\n", pid);
            wait_for_job(job, force, tty);
            res = exit_status;
            debug ("exit_status = %d\n", exit_status);
        }
        /* (b) argument is a pid */
        else if(isdigit(*arg))
        {
            /* Save the terminal's attributes */
            struct termios *attr = NULL;
            char *strend = NULL;
            pid = strtol(arg, &strend, 10);
            
            if(*strend || pid == 0)
            {
                /* bash returns immediately in this case */
                PRINT_ERROR(UTILITY, "invalid pid: %s", arg);
                res = 127;
                continue;
            }
            
            job = get_job_by_any_pid(pid);

            /* restore the terminal attributes to what it was when the job was suspended, as zsh does */
            if(job && job->tty_attr)
            {
                attr = save_tty_attr();
                set_tty_attr(tty, job->tty_attr);
            }
            
            res = wait_for_pid(job, pid, force);
            res = exit_status;
            
            if(attr)
            {
                set_tty_attr(tty, attr);
            }
        }
        else
        {
            PRINT_ERROR(UTILITY, "invalid pid: %s", arg);
            res = 1;
        }
    }

    /* Restore SIGCHLD signal handler to what it was before */
    sigaction(SIGCHLD, &old_sigact, NULL);
    return res;
}
