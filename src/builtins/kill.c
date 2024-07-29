/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: kill.c
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
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <wait.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../include/sig.h"
#include "../include/debug.h"

#define UTILITY     "kill"


/*
 * Return the signal number for the given signal name.
 *
 * Returns the signal number, or -1 for invalid signal names.
 */
int get_signum(char *signame)
{
    int i;
    char signame2[16];
    char *strend;
    
    if(isalpha(*signame))
    {
        if(strncmp(signame, "SIG", 3) == 0)
        {
            /* signal name has the SIG prefix */
            strcpy(signame2, signame);
        }
        else
        {
            /* signal name with no SIG prefix. add the prefix */
            sprintf(signame2, "SIG%s", signame);
        }
        
        /* get the signal number */
        for(i = 0; i < SIGNAL_COUNT; i++)
        {
            if(strcasecmp(signames[i], signame2) == 0)
            {
                return i;
            }
        }

        /* invalid signal name */
        return -1;
    }
    else if(isdigit(*signame))
    {
        i = strtol(signame, &strend, 10);

        if(*strend /* || i < 0 || i > SIGNAL_COUNT */ )
        {
            return -1;
        }
        
        return i;
    }
    
    return -1;
}


/*
 * Send a signal to the given pid. If pid is negative, the signal is sent to
 * all processes in the process group whose pgid equals the given pid.
 * 
 * Returns 0 on success, -1 on failure.
 */
int do_kill(pid_t pid, int signum, struct job_s *job)
{
    int res = 0;

    /* Negative pid is actually a pgid */
    if(pid < 0)
    {
        sigset_t sigset;
        SIGNAL_BLOCK(SIGCHLD, sigset);
        
        if(job)
        {
            job->flags &= ~JOB_FLAG_NOTIFIED;
            
            /* 
             * Jobs whose pgid == the shell's pgid are usually started in the 
             * background. We can't simply send the signal to the pgid, as this
             * will affect the shell too, so we need to manually send the signal
             * to all processes in the job.
             */
            if(job->pgid == shell_pid)
            {
                int i, j = 1;
                for(i = 0; i < job->proc_count; i++, j <<= 1)
                {
                    if(job->child_exitbits & j)
                    {
                        continue;
                    }
                    
                    /* 
                     * we need to make sure the target process is running, so that
                     * it will receive our signal (tcsh, bash).
                     */
                    if(signum == SIGTERM || signum == SIGHUP)
                    {
                        kill(pid, SIGCONT);
                    }
                    
                    res = kill(job->pids[i], signum);
                }
            }
            else
            {
                /* 
                 * we need to make sure the target process is running, so that
                 * it will receive our signal (tcsh, bash).
                 */
                pid = -(job->pgid);
                
                if(signum == SIGTERM || signum == SIGHUP)
                {
                    kill(pid, SIGCONT);
                }
                
                res = kill(pid, signum);
                
                /*
                 * Act as if the job was resumed using bg (bash).
                 * bash also sets the job to the running state here.
                 */
                if(STOPPED(job->status) && signum == SIGCONT)
                {
                    job->flags &= ~(JOB_FLAG_FORGROUND|JOB_FLAG_NOTIFIED);
                }
            }
        }
        else
        {
            /* No job. Just kill the process group */
            res = kill(pid, signum);
        }
        
        SIGNAL_UNBLOCK(sigset);
    }
    else
    {
        /* Kill the process with the given pid */
        res = kill(pid, signum);
    }
    
    return res;
}


/*
 * The kill builtin utility (non-POSIX). Used to send a signal to a job, process,
 * or process group.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help kill` or `kill -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int kill_builtin(int argc, char **argv)
{
    /* we should have at least one option/argument */
    if(argc == 1)
    {
        PRINT_ERROR(UTILITY, "missing operand(s)");
        return 1;
    }

    int i, index = 0, res = 0;
    int signum = -1;
    pid_t pid = 0;
    struct job_s *job = NULL;
    char *arg, *strend = NULL;

    /* process the options and arguments */
    while(++index < argc)
    {
        arg = argv[index];
        /* options start with '-' */
        if(*arg == '-')
        {
            if(arg[1] == '\0')      /* - or end of options */
            {
                index++;
                break;
            }
            
            if(strcmp(arg, "-h") == 0)
            {
                print_help(argv[0], &KILL_BUILTIN, 0);
                return 0;
            }
            else if(strcmp(arg, "-v") == 0)
            {
                printf("%s", shell_ver);
                return 0;
            }
            else if(strcmp(arg, "--") == 0)
            {
                break;
            }
            /* get the signal number or name */
            else if(strcmp(arg, "-n") == 0 || strcmp(arg, "-s") == 0)
            {
                if(++index >= argc)
                {
                    OPTION_REQUIRES_ARG_ERROR(UTILITY, arg[1]);
                    return 1;
                }

                arg = argv[index];
                signum = get_signum(arg);
                
                /* check the validity of the signal name */
                if(signum < 0 || signum > SIGNAL_COUNT)
                {
                    goto invalid_signame;
                }
            }
            /* list all signal names, or the number of the given name */
            else if(strcmp(arg, "-l") == 0 || strcmp(arg, "-L") == 0)
            {
                /* No option-argument. List all signals */
                arg = argv[++index];
                if(!arg || *arg == '\0')
                {
                    /* list all signal names */
                    for(i = 0; i < SIGNAL_COUNT; i++)
                    {
                        printf("%2d) %s%*s", i, signames[i], (int)(10-strlen(signames[i])), " ");
                        if((i % 4) == 0)
                        {
                            printf("\n");
                        }
                    }
                    
                    if((i-1) % 4)
                    {
                        printf("\n");
                    }

                    return 0;
                }
                
                /* We have an option-argument, which is a signal number (or name) */
                signum = get_signum(arg);
                
                if(signum >= 0 && signum < SIGNAL_COUNT)
                {
                    if(isalpha(*arg))
                    {
                        /* print signal number */
                        printf("%d\n", signum);
                    }
                    else
                    {
                        /* print without the SIG prefix */
                        printf("%s\n", signum ? &signames[signum][3] : "NULL");
                    }

                    return 0;
                }
                
                /* we have an exit status of a process terminated by a signal */
                if(WIFSIGNALED(signum))
                {
                    int sig = WTERMSIG(signum);

                    if(sig < 0 || sig >= SIGNAL_COUNT)
                    {
                        printf("%d\n", sig);
                    }
                    else
                    {
                        printf("%s\n", signames[sig]);
                    }

                    return 0;
                }

                goto invalid_signame;
            }
            else
            {
                /* try parsing as a -signal_name or -signal_number option */
                signum = get_signum(++arg);

                if(signum < 0 || signum > SIGNAL_COUNT)
                {
                    goto invalid_signame;
                }
            }
        }
        else
        {
            /* no more options */
            break;
        }
    }

    /* end of options and beginning of arguments */
    if(index >= argc)
    {
        if(signum != -1)
        {
            PRINT_ERROR(UTILITY, "missing argument (run `kill -h` to see usage)");
        }

        return res;
    }
    
    if(signum == -1)
    {
        signum = SIGTERM;
    }

    /* process the arguments */
    do
    {
        job = NULL;
        arg = argv[index];

        /* (a) argument is a job ID number */
        if(*arg == '%')
        {
            sigset_t sigset;
            SIGNAL_BLOCK(SIGCHLD, sigset);

            pid = get_jobid(arg);
            job = get_job_by_jobid(pid);

            SIGNAL_UNBLOCK(sigset);
            
            if(pid == 0 || !job)
            {
                INVALID_JOB_ERROR(UTILITY, arg);
                res = 2;
                continue;
            }

            pid = -(job->pgid);
        }
        /* (b) argument is a pid */
        else
        {
            /* empty arg or invalid number */
            pid = strtol(arg, &strend, 10);
            
            if(!*arg || *strend)
            {
                INVALID_JOB_ERROR(UTILITY, arg);
                res = 2;
                continue;
            }
        }

        /* send the signal */
        if(do_kill(pid, signum, job) != 0)
        {
            res = 1;
        }

    } while(++index < argc);

    return res;
    
invalid_signame:
    PRINT_ERROR(UTILITY, "invalid signal name: %s", arg);
    return 2;
}
