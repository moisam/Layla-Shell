/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <wait.h>
#include "../cmd.h"
#include "../signames.h"
#include "../debug.h"

#define UTILITY     "kill"

char *str_toupper(char *lower)
{
    char *upper = get_malloced_str(lower);
    char *u     = upper;
    while(*u)
    {
        if(*u >= 'a' && *u <= 'z')
            *u = *u-('a'-'A');
        u++;
    }
    return upper;
}

int get_signum(char *__signame)
{
    int i;
    char *signame = str_toupper(__signame);
    char signame2[10];
    if(strncmp(signame, "SIG", 3) == 0) strcpy(signame2, signame);
    else
    {
        strcpy(signame2, "SIG"  );
        strcat(signame2, signame);
    }
    free_malloced_str(signame);
    for(i = 0; i < total_signames; i++)
        if(strcmp(signames[i], signame2) == 0)
            return i;
    return -1;
}


int __kill(int argc, char *argv[])
{
    if(argc == 1)
    {
        fprintf(stderr, "%s: missing operand(s)\r\n", UTILITY);
        return 1;
    }
    int    index  = 0;
    int    res    = 0;
    int    i;
    int    signum = SIGTERM;
    pid_t  pid    = 0;
    struct job *job = NULL;
    char  *arg;

    while(++index < argc)
    {
        arg = argv[index];
        if(*arg == '-')
        {
            if     (strcmp(arg, "-h") == 0) { print_help(argv[0], REGULAR_BUILTIN_KILL, 1, 0); continue; }
            else if(strcmp(arg, "-v") == 0) { printf("%s", shell_ver); continue; }
            else if(strcmp(arg, "--") == 0) break;
            else if(strcmp(arg, "-n") == 0)
            {
                if(++index >= argc)
                {
                    fprintf(stderr, "%s: missing argument: signal number\r\n", UTILITY);
                    return 1;
                }
                arg = argv[index];
                if(isdigit(*arg))
                {
                    signum = atoi(arg);
                    if(signum == 0 || signum > total_signames) goto invalid_signame;
                    continue;
                }
                else goto invalid_signame;
            }
            else if(strcmp(arg, "-s") == 0)
            {
                if(++index >= argc)
                {
                    fprintf(stderr, "%s: missing argument: signal name\r\n", UTILITY);
                    return 1;
                }
                arg = argv[index];
                if(strcmp(arg, "0") == 0) signum = 0;
                else signum = get_signum(arg);
                if(signum == -1)
                {
                    goto invalid_signame;
                }
                continue;
            }
            else if(strcmp(arg, "-l") == 0 || strcmp(arg, "-L") == 0)
            {
                /* no option-argument */
                if(++index >= argc)
                {
                    /* list all signal names */
                    for(i = 0; i < total_signames; i++)
                    {
                        printf("%2d) %.10s ", i, signames[i]);
                        if((i % 4) == 0) printf("\r\n");
                    }
                    if(total_signames % 4) printf("\r\n");
                    return 0;
                }
                /* we have an option-argument, which is a singal number */
                arg = argv[index];
                int j = atoi(arg);
                if(isdigit(*arg) && j >= 0 && j < total_signames)
                {
                    if(j == 0) printf("NULL\r\n");
                    else       printf("%s\r\n", &signames[j][3]);   /* without SIG prefix */
                    return 0;
                }
                /* we have an exit status of a process terminated by a signal */
                if(WIFSIGNALED(j))
                {
                    int sig = WTERMSIG(j);
                    if(sig < 0 || sig >= total_signames) printf("%d\r\n", sig);
                    else printf("%s\r\n", signames[sig]);
                    return 0;
                }
                fprintf(stderr, "%s: invalid signal specification: %s\r\n", UTILITY, arg);
                return 4;
            }
            /* try a -signal_name or -signal_number options */
            arg++;
            if(isalpha(*arg))
            {
                if(strcmp(arg, "0") == 0) signum = 0;
                else signum = get_signum(arg);
                if(signum == -1) goto invalid_signame;
                continue;
            }
            else if(isdigit(*arg))
            {
                signum = atoi(arg);
                if(signum == 0 || signum > total_signames) goto invalid_signame;
                continue;
            }
            fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, arg);
            return 3;
        }
        break;
    }
    /* end of options. start of arguments */
    if(index >= argc) return res;

    do
    {
        arg = argv[index];
        /* (a) argument is a job ID number */
        if(*arg == '%')
        {
            /* bash says we can't kill jobs if job control is not active, which makes sense */
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
                res = 3;
                continue;
            }
            pid = -(job->pgid);
        }
        /* (b) argument is a pid */
        else
        {
            pid = atoi(arg);
        }

        /* 
         * we need to make sure the target process is running, so that
         * it will receive our signal (tcsh does this).
         */
        if(signum == SIGTERM || signum == SIGHUP)
            kill(pid, SIGCONT);
        if(kill(pid, signum) != 0)
        {
            if(signum < 0 || signum >= total_signames)
                fprintf(stderr, "%s: failed to send signal %d to process %d: %s\r\n",
                        UTILITY, signum, pid, strerror(errno));
            else
                fprintf(stderr, "%s: failed to send signal %s to process %d: %s\r\n", 
                        UTILITY, signames[signum], pid, strerror(errno));
            res = 3;
        }
        else if(job)
        {
            if(signum == SIGTERM || signum == SIGKILL || signum == SIGHUP)
            {
                kill_job(job);
                job = NULL;
            }
        }
    } while(++index < argc);
    return res;
    
invalid_signame:
    fprintf(stderr, "%s: invalid signal name: %s\r\n", UTILITY, arg);
    return 2;
}
