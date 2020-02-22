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
#include "../cmd.h"
#include "../sig.h"
#include "../debug.h"

#define UTILITY     "kill"


/*
 * return the signal number for the given signal name.
 *
 * returns the signal number, or -1 for invalid signal names.
 */
int get_signum(char *signame)
{
    int i;
    char signame2[16];
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


/*
 * the kill builtin utility (non-POSIX).. used to send a signal to a job, process,
 * or process group.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help kill` or `kill -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int kill_builtin(int argc, char **argv)
{
    /* we should have at least one option/argument */
    if(argc == 1)
    {
        PRINT_ERROR("%s: missing operand(s)\n", UTILITY);
        return 1;
    }

    int i, index = 0, res = 0;
    int signum = SIGTERM;
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
            /* get the signal number */
            else if(strcmp(arg, "-n") == 0)
            {
                if(++index >= argc)
                {
                    PRINT_ERROR("%s: missing argument: signal number\n", UTILITY);
                    return 1;
                }
                
                arg = argv[index];
                if(isdigit(*arg))
                {
                    signum = strtol(arg, &strend, 10);
                    if(*strend || signum == 0 || signum > SIGNAL_COUNT)
                    {
                        goto invalid_signame;
                    }
                    continue;
                }
                else
                {
                    goto invalid_signame;
                }
            }
            /* get the symbolic signal name */
            else if(strcmp(arg, "-s") == 0)
            {
                if(++index >= argc)
                {
                    PRINT_ERROR("%s: missing argument: signal name\n", UTILITY);
                    return 1;
                }
                
                arg = argv[index];
                if(strcmp(arg, "0") == 0)
                {
                    signum = 0;
                }
                else
                {
                    signum = get_signum(arg);
                }
                
                /* check the validity of the signal name */
                if(signum == -1)
                {
                    goto invalid_signame;
                }
                continue;
            }
            /* list all signal names, or the number of the given name */
            else if(strcmp(arg, "-l") == 0 || strcmp(arg, "-L") == 0)
            {
                /* no option-argument. list all signals */
                arg = argv[++index];
                if(index >= argc || !arg || *arg == '\0')
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
                
                /* we have an option-argument, which is a signal number (or name) */
                int j;
                
                if(isalpha(*arg))
                {
                    j = get_signum(arg);
                    if(j == -1)
                    {
                        PRINT_ERROR("%s: invalid signal specification: %s\n", UTILITY, arg);
                        return 2;
                    }
                }
                else
                {
                    j = strtol(arg, &strend, 10);
                    if(*strend)
                    {
                        PRINT_ERROR("%s: invalid signal specification: %s\n", UTILITY, arg);
                        return 2;
                    }
                }
                
                if(j >= 0 && j < SIGNAL_COUNT)
                {
                    if(isalpha(*arg))
                    {
                        /* print signal number */
                        printf("%d\n", j);
                    }
                    else
                    {
                        /* print without the SIG prefix */
                        printf("%s\n", j ? &signames[j][3] : "NULL");
                    }
                    return 0;
                }
                
                /* we have an exit status of a process terminated by a signal */
                if(WIFSIGNALED(j))
                {
                    int sig = WTERMSIG(j);
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
                PRINT_ERROR("%s: invalid signal specification: %s\n", UTILITY, arg);
                return 2;
            }
            
            /* try parsing as a -signal_name or -signal_number option */
            arg++;
            if(isalpha(*arg))
            {
                if(strcmp(arg, "0") == 0)
                {
                    signum = 0;
                }
                else
                {
                    signum = get_signum(arg);
                }

                /* check the validity of the signal name */
                if(signum == -1)
                {
                    goto invalid_signame;
                }
                continue;
            }
            else if(isdigit(*arg))
            {
                signum = strtol(arg, &strend, 10);
                if(*strend || signum == 0 || signum > SIGNAL_COUNT)
                {
                    goto invalid_signame;
                }
                continue;
            }

            PRINT_ERROR("%s: unknown option: %s\n", UTILITY, arg);
            return 2;
        }
        break;
    }

    /* end of options and beginning of arguments */
    if(index >= argc)
    {
        return res;
    }

    /* process the arguments */
    do
    {
        arg = argv[index];

        /* (a) argument is a job ID number */
        if(*arg == '%')
        {
            /* bash says we can't kill jobs if job control is not active, which makes sense */
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
                res = 2;
                continue;
            }
            pid = -(job->pgid);
        }
        /* (b) argument is a pid */
        else
        {
            pid = strtol(arg, &strend, 10);
            if(*strend)
            {
                PRINT_ERROR("%s: invalid job id: %s\n", UTILITY, arg);
                res = 2;
                continue;
            }
        }

        /* 
         * we need to make sure the target process is running, so that
         * it will receive our signal (tcsh does this).
         */
        if(signum == SIGTERM || signum == SIGHUP)
        {
            kill(pid, SIGCONT);
        }

        /* send the signal */
        if(kill(pid, signum) != 0)
        {
            if(signum < 0 || signum >= SIGNAL_COUNT)
            {
                PRINT_ERROR("%s: failed to send signal %d to process %d: %s\n",
                        UTILITY, signum, pid, strerror(errno));
            }
            else
            {
                PRINT_ERROR("%s: failed to send signal %s to process %d: %s\n", 
                        UTILITY, signames[signum], pid, strerror(errno));
            }
            res = 2;
        }
        else if(job)
        {
            if(signum == SIGTERM || signum == SIGKILL || signum == SIGHUP)
            {
                remove_job(job);
                job = NULL;
            }
        }
    } while(++index < argc);
    return res;
    
invalid_signame:
    PRINT_ERROR("%s: invalid signal name: %s\n", UTILITY, arg);
    return 2;
}
