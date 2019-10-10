/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: jobs.c
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include "cmd.h"
#include "symtab/symtab.h"
#include "sig.h"
#include "error/error.h"
#include "debug.h"

#define UTILITY         "jobs"

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;

/* jobs table for all the jobs running under this shell */
struct job __jobs[MAX_JOBS];

/* jobs count */
int total_jobs   = 0;

/* the current job number */
int cur_job      = 0;

/* the previous job number */
int prev_job     = 0;

/* flags for the output_status() function */
#define OUTPUT_STATUS_RUN_ONLY      (1 << 0)    /* output the status of running jobs only */
#define OUTPUT_STATUS_STOP_ONLY     (1 << 1)    /* output the status of stopped jobs only */
#define OUTPUT_STATUS_NEW_ONLY      (1 << 2)    /* output the status of unnotified jobs only */
#define OUTPUT_STATUS_PIDS_ONLY     (1 << 3)    /* output only the job process ids */
#define OUTPUT_STATUS_VERBOSE       (1 << 4)    /* output verbose info about the job */

/* 
 * struct to hold the list of dead process whose status hasn't been added to
 * the jobs table yet.
 */
struct
{
    pid_t pid;
    int   status;
} deadlist[32];

int listindex = 0;


/*
 * update the job table entry with the exit status of the process with the
 * given pid.
 */
void set_pid_exit_status(struct job *job, pid_t pid, int status)
{
    int i;
    if(!job || !job->pids || !job->exit_codes)
    {
        return;
    }
    /* search the job's pid list to find the given pid */
    for(i = 0; i < job->proc_count; i++)
    {
        if(job->pids[i] == pid)
        {
            /* update the exit status bitmap */
            job->exit_codes[i] = status;
            /* process exited normally or was terminated by a signal */
            if(WIFEXITED(status) || WIFSIGNALED(status))
            {
                job->child_exitbits |=  (1 << i);
            }
            else
            {
                job->child_exitbits &= ~(1 << i);
            }
            break;
        }
    }
    /* count the number of children that exited */
    job->child_exits = 0;
    int j = 1;
    for(i = 0; i < job->proc_count; i++, j <<= 1)
    {
        if((job->child_exitbits & j) == j)
        {
            job->child_exits++;
        }
    }
}


/*
 * set the job's exit status according to the exit status of its member processes.
 * if the pipefail '-l' option is set, the job's exit status is that of the last
 * process to exit with non-zero exit status, otherwise its that of the process
 * whose pid == pgid of the job.
 */
void set_job_exit_status(struct job *job, pid_t pid, int status)
{
    if(!job)
    {
        /* if job control is not active, set $? anyway */
        set_exit_status(status);
        return;
    }
    if(option_set('l'))    /* the pipefail option */
    {
        int i;
        int res = 0;
        if(job->proc_count && job->exit_codes)
        {
            for(i = 0; i < job->proc_count; i++)
            {
                if(job->exit_codes[i])
                {
                    res = job->exit_codes[i];
                    break;
                }
            }
        }
        if(res != job->status)
        {
            job->flags &= ~JOB_FLAG_NOTIFIED;
        }
        job->status = res;
    }
    else
    {
        /* normal, everyday pipe (with no pipefail option) */
        if(job->pgid == pid)
        {
            job->status = status;
        }
    }
    /* now get the $! variable and set $? if needed */
    struct symtab_entry_s *entry = get_symtab_entry("!");
    if(!entry || !entry->val)
    {
        return;
    }
    char *endptr;
    long n = strtol(entry->val, &endptr, 10);
    if(endptr == entry->val)
    {
        return;
    }
    if(n == job->pgid)
    {
        entry = get_symtab_entry("?");
        if(!entry)
        {
            return;
        }
        char buf[16];
        if(WIFEXITED(job->status))
        {
            status = WEXITSTATUS(job->status);
        }
        else if(WIFSIGNALED(job->status))
        {
            status = WTERMSIG(job->status) + 128;
        }
        else if(WIFSTOPPED(job->status))
        {
            status = WSTOPSIG(job->status) + 128;
        }
        else
        {
            status = job->status;
        }
        sprintf(buf, "%d", status);
        symtab_entry_setval(entry, buf);
    }
}


/*
 * update the job's exit status by updating the exit status of the job's member 
 * processes and setting the job's exit status accordingly.
 */
void update_job_exit_status(struct job *job)
{
    if(!job)
    {
        return;
    }
    /* update the exit status of each process in the job's pid list */
    int i, status;
    pid_t pid;
    for(i = 0; i < job->proc_count; i++)
    {
        pid = job->pids[i];
        if(waitpid(pid, &status, WNOHANG) == pid)
        {
            set_pid_exit_status(job, pid, status);
            set_job_exit_status(job, pid, status);
        }
    }
}


/*
 * check for POSIX list terminators: ';', '\n', and '&'.
 * 
 * returns 1 if the first char of *c is a list terminator, 0 otherwise.
 */
inline int is_list_terminator(char *c)
{
    if(*c == ';' || *c   == '\n')
    {
        return 1;
    }
    if(*c == '&' && c[1] !=  '&')
    {
        return 1;
    }
    return 0;
}


/*
 * print a notification message telling the user about the status of the job to
 * which process pid (with the given exit status) belongs.. the output_pid
 * parameter indicates whether we should print the pid as part of the message or not..
 * if the rip_dead parameter is non-zero, we will remove the job from the jobs table.
 */
void __output_status(pid_t pid, int status, int output_pid, FILE *out, int rip_dead)
{
    struct job *job = get_job_by_any_pid(pid);
    /* 
     * no job with this pid? probably it was a subshell or some command
     * substitution task. bail out.
     */
    if(!job)
    {
        return;
    }
    char statstr[32];
    int sig;
    if(WIFSTOPPED(status))
    {
        sig = WSTOPSIG(status);
        if(sig == SIGTSTP)
        {
            strcpy(statstr, "Stopped          ");
        }
        else if(sig == SIGSTOP)
        {
            strcpy(statstr, "Stopped (SIGSTOP)");
        }
        else if(sig == SIGTTIN)
        {
            strcpy(statstr, "Stopped (SIGTTIN)");
        }
        else if(sig == SIGTTOU)
        {
            strcpy(statstr, "Stopped (SIGTTOU)");
        }
    }
    else if(WIFCONTINUED(status))
    {
        return;
    }
    else if(WIFSIGNALED (status))
    {
        int sig = WTERMSIG(status);
        if(sig < 0 || sig >= total_signames)
        {
             sprintf(statstr, "Signaled(%3d)     ", sig);
        }
        else
        {
            sprintf(statstr, "Signaled(%.9s)    ", signames[sig]);
        }
    }
    else if(job->child_exits && job->child_exits == job->proc_count)
    //else if(WIFEXITED   (status))
    {
        if(WEXITSTATUS(status) == 0)
        {
            strcpy(statstr, "Done             ");
        }
        else
        {
            sprintf(statstr, "Done(%3u)      ", WEXITSTATUS(status));
        }
    }
    else
    {
        strcpy(statstr, "Running          ");
    }
    /* print the notification message */
    char current = (job->job_num == cur_job ) ? '+' :
                   (job->job_num == prev_job) ? '-' : ' ';
    if(output_pid)
    {
         fprintf(out, "[%d]%c %u %s  %s\n",
                 job->job_num, current, job->pgid, statstr, job->commandstr);
    }
    else
    {
        fprintf(out, "[%d]%c %s     %s\n",
                 job->job_num, current, statstr, job->commandstr);
    }
    /* mark the job as notified */
    job->flags |= JOB_FLAG_NOTIFIED;
    /* remove the job from the jobs table if it exited normally or by receiving a signal */
    if(rip_dead && (WIFSIGNALED(status) || WIFEXITED(status)))
    {
        kill_job(job);
    }
}

/*
 * POSIX Job Control Job ID Formats:
 * 
 * Job Control Job ID       Meaning
 * ==================       ====================================
 * %%                       Current job.
 * %+                       Current job.
 * %-                       Previous job.
 * %n                       Job number n.
 * %string                  Job whose command begins with string.
 * %?string                 Job whose command contains string.
 */

/*
 * get the job id for the given job string (should be one of the entries in
 * the table above).
 * 
 * returns the job id number, or 0 if the job is not found.
 */
int get_jobid(char *jobid_str)
{
    if(!jobid_str || !*jobid_str)
    {
        return 0;
    }
    if(*jobid_str != '%')
    {
        return 0;
    }
    if(strcmp(jobid_str, "%%") == 0)
    {
        return cur_job ;
    }
    if(strcmp(jobid_str, "%+") == 0)
    {
        return cur_job ;
    }
    if(strcmp(jobid_str, "%-") == 0)
    {
        return prev_job;
    }
    if(!*++jobid_str)
    {
        return 0;
    }
    if(*jobid_str >= '0' && *jobid_str <= '9')
    {
        return atoi(jobid_str);
    }
    struct job *job;
    if(*jobid_str == '?')
    {
        /* search for a job whose command contains the given string */
        jobid_str++;
        for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
        {
            if(job->job_num == 0)
            {
                continue;
            }
            if(strstr(job->commandstr, jobid_str))
            {
                return job->job_num;
            }
        }
    }
    else
    {
        /* search for a job whose command starts with the given string */
        size_t len = strlen(jobid_str);
        for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
        {
            if(job->job_num == 0)
            {
                continue;
            }
            if(strncmp(job->commandstr, jobid_str, len) == 0)
            {
                return job->job_num;
            }
        }
    }
    return 0;
}


/*
 * get the number of current jobs.
 */
int pending_jobs()
{
    int count = 0;
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num != 0)
        {
            if(!job->child_exits || job->child_exits != job->proc_count)
            {
                count++;
            }
        }
    }
    return count;
}


/*
 * kill all the pending jobs.
 * 
 * called by exit() and others to kill all child processes.
 * the flag field tells us if we want to exclude specific
 * jobs from receiving the signal, e.g. all jobs except
 * the disowned ones (this is what a login terminal does
 * when it receives a SIGHUP signal).
 */
void kill_all_jobs(int signum, int flag)
{
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num != 0)
        {
            if(flag && flag_set(job->flags, flag))
            {
                continue;
            }
            pid_t pid = -(job->pgid);
            kill(pid, SIGCONT);
            kill(pid, signum);
        }
    }
}


/*
 * replace all jobspec occurences in the form of '%n' in the given argv, starting
 * with argv[startat], then call the command builtin utility, passing it the
 * arguments to execute them.
 * 
 * returns 0 if the command is found and executed, non-zero otherwise.
 */
int replace_and_run(int startat, int argc, char **argv)
{
    char buf[32];
    int i = startat;
    /* can't start past argc count */
    if(i >= argc)
    {
        return 2;
    }
    /* loop on the argv array, replacing all jobspecs */
    for( ; i < argc; i++)
    {
        int modified = 0;
        char *perc, *p;
        char __arg[strlen(argv[i])*2];
        char *arg = __arg;
        strcpy(arg, argv[i]);
        /* search the argument for % */
        while((perc = strchr(arg, '%')))
        {
            char c = perc[1];
            /* %%, %+, %-, and % jobspecs */
            if(c == '%' || c == '+' || c == '-' || c == '\0')
            {
                buf[0] = '%';
                if(c)
                {
                    buf[1] = c;
                }
                else    /* add another % to make % become %% */
                {
                    buf[1] = '%';
                }
                buf[2] = '\0';
            }
            /* %n jobspecs */
            else if(isdigit(c))
            {
                buf[0] = '%';
                p = buf+1;
                while(isdigit((c = *++perc)))
                {
                    *p++ = c;
                }
                *p = '\0';
            }
            /* %str and %?str jobspecs */
            else if(isalpha(c) || c == '?')
            {
                buf[0] = '%';
                p = buf+1;
                if(c == '?')
                {
                    *p++ = c, c = *++perc;
                }
                while(isalnum((c = *++perc)))
                {
                    *p++ = c;
                }
                *p = '\0';
            }
            else
            {
                arg = perc+1;
                continue;
            }
            int len = strlen(buf);
            /* get the pgid of the job given the jobspec */
            struct job *job = get_job_by_jobid(get_jobid(buf));
            if(!job)
            {
                fprintf(stderr, "%s: unknown job: %s\n", UTILITY, buf);
                return 1;
            }
            sprintf(buf, "%d", job->pgid);
            /* make some room in the buffer */
            int len2 = strlen(buf);
            if(len2 > len)
            {
                int j = len2-len;
                char *p1 = arg+strlen(arg);
                char *p2 = p1+j;
                while(p1 > arg)
                {
                    *p2-- = *p1--;
                }
            }
            strncpy(arg, buf, len2);
            arg = perc+len2;
            modified = 1;
        }
        /* has we substituted any jobspecs in this arg? */
        if(modified)
        {
            p = get_malloced_str(__arg);
            if(!p)
            {
                continue;
            }
            /* free the old arg and store the modified one in its place */
            free_malloced_str(argv[i]);
            argv[i] = p;
        }
    }
    /* now call command */
    int    cargc = argc-2;
    char **cargv = &argv[2];
    /* push a local symbol table on top of the stack */
    symtab_stack_push();
    int res = search_and_exec(NULL, cargc, cargv, NULL, SEARCH_AND_EXEC_DOFORK|SEARCH_AND_EXEC_DOFUNC);
    /* free the local symbol table */
    free_symtab(symtab_stack_pop());
    /* return the result */
    return res;
}


/*
 * output the status of a job.. called by the jobs builtin utility (see below).
 */
void output_status(struct job *job, int flags)
{
    if(!job)
    {
        return;
    }
    /* update job with the exit status codes of its child processes */
    update_job_exit_status(job);
    /* check if we ought to list running jobs but the job is not running */
    if(flag_set(flags, OUTPUT_STATUS_RUN_ONLY) && NOT_RUNNING(job->status))
    {
        return;
    }
    /* check if we ought to list stopped jobs but the job is not stopped */
    if(flag_set(flags, OUTPUT_STATUS_STOP_ONLY) && !WIFSTOPPED(job->status))
    {
        return;
    }
    /* check if we ought to list job pids only */
    if(flag_set(flags, OUTPUT_STATUS_PIDS_ONLY))
    {
        printf("%d\n", job->pgid);
    }
    else
    {
        /* don't list notified jobs */
        if(!flag_set(flags, OUTPUT_STATUS_NEW_ONLY) || !flag_set(job->flags, JOB_FLAG_NOTIFIED))
        {
            /* don't list disowned jobs */
            if(!flag_set(job->flags, JOB_FLAG_DISOWNED))
            {
                __output_status(job->pgid, job->status, flag_set(flags, OUTPUT_STATUS_VERBOSE), stdout, 0);
            }
        }
    }
}


/*
 * the jobs builtin utility (POSIX).. used to list the status of running/stopped jobs.
 * returns 0, unless an unknown option or jobspec was supplied.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help jobs` or `jobs -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int jobs(int argc, char **argv)
{
    int flags        = 0;
    int i;
    int finish       = 0;
    struct job *job;
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    while((c = parse_args(argc, argv, "hvlpnrsx", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_JOBS, 1, 0);
                break;
                
            case 'v':
                printf("%s", shell_ver);
                break;
                
            case 'l':
                flags |= OUTPUT_STATUS_VERBOSE;
                break;
                
            case 'p':
                flags |= OUTPUT_STATUS_PIDS_ONLY;
                break;
                
            case 'n':
                flags |= OUTPUT_STATUS_NEW_ONLY;
                break;
                
            case 'r':
                flags |= OUTPUT_STATUS_RUN_ONLY;
                flags &= ~OUTPUT_STATUS_STOP_ONLY;
                break;
                
            case 's':
                flags |= OUTPUT_STATUS_STOP_ONLY;
                flags &= ~OUTPUT_STATUS_RUN_ONLY;
                break;
                
            /*
             * support bash's jobs -x option. see page 110 of bash manual.
             */
            case 'x':
                return replace_and_run(v+1, argc, argv);
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    /* loop on the arguments */
    for(i = v; i < argc; i++)
    {
        /* first try POSIX-style job ids */
        job = get_job_by_jobid(get_jobid(argv[i]));
        /* maybe we have a process pid? */
        if(!job)
        {
            char *strend;
            long pgid = strtol(argv[i], &strend, 10);
            if(strend != argv[i])
            {
                job = get_job_by_any_pid(pgid);
            }
        }
        /* still nothing? */
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %s\n", UTILITY, argv[i]);
            return 1;
        }
        output_status(job, flags);
        finish = 1;
    }
    if(finish)
    {
        return 0;
    }
    /* we have no arguments. list all unnotified jobs */
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num != 0)
        {
            output_status(job, flags);
        }
    }
    /* 
     * we didn't kill the exited jobs in the above loop, because it will
     * mess the shell's notion of who's current and who's previous job,
     * i.e. we might get two or more jobs denoted with '+' or '-'.
     * so, loop through the job list again and kill those who need killing.
     */
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num != 0)
        {
            if(job->child_exits == job->proc_count)
            {
                kill_job(job);
            }
        }
    }
    return 0;
}


/*
 * check for any child processes that has changed status since our last check.
 * called by cmdline() every time its about to print $PS1.
 */
void check_on_children()
{
    /* check for children who died while we were away */
    int i, status = 0;
    for(i = 0; i < listindex; i++)
    {
        //__output_status(deadlist[i].pid, deadlist[i].status, 0, stderr, 1);
        struct job *job = get_job_by_any_pid(deadlist[i].pid);
        if(job)
        {
            status = deadlist[i].status;
            set_job_exit_status(job, deadlist[i].pid, deadlist[i].status);
            /* report job status only if all commands finished execution and it was a background job */
            if(job->child_exits == job->proc_count)
            {
                if(!flag_set(job->flags, JOB_FLAG_FORGROUND))
                {
                    __output_status(deadlist[i].pid, deadlist[i].status, 0, stderr, 1);
                }
                kill_job(job);
            }
            /* or if it was stopped/continued and not notified, regardless of fg/bg status */
            else if(!WIFEXITED(status) && !flag_set(job->flags, JOB_FLAG_NOTIFIED))
            {
                __output_status(deadlist[i].pid, deadlist[i].status, 0, stderr, 1);
            }
        }
    }
    listindex = 0;
    /* check for children who died but are not yet reported */
    while(1)
    {
        pid_t pid = waitpid(-1, &status, WUNTRACED|WNOHANG);
        if(pid <= 0)
        {
            break;
        }
        struct job *job = get_job_by_any_pid(pid);
        if(job)
        {
            //set_pid_exit_status(job, pid, status);
            set_job_exit_status(job, pid, status);
            /* report job status only if the last command finished execution */
            if(job->child_exits == job->proc_count)
            {
                if(!flag_set(job->flags, JOB_FLAG_FORGROUND))
                {
                    __output_status(pid, status, 0, stderr, 1);
                }
                kill_job(job);
            }
            /* or if it was stopped/continued and not notified, regardless of fg/bg status */
            else if(!WIFEXITED(status) && !flag_set(job->flags, JOB_FLAG_NOTIFIED))
            {
                __output_status(pid, status, 0, stderr, 1);
            }
        }
    }
}


/*
 * if a child process changes status, notify the user of this by calling
 * __output_status() if the -b option is set.. otherwise, add the pid and
 * status to the deadlist for us to reap later on in the check_on_children()
 * function.
 */
void notice_termination(pid_t pid, int status)
{
    if(pid <= 0)
    {
        return;
    }
    /* asynchronous notification flag is on */
    if(option_set('b'))
    {
        __output_status(pid, status, 0, stderr, 1);
    }
    /* don't add the zombie job if it's already on the list */
    int i;
    for(i = 0; i < listindex; i++)
    {
        if(deadlist[i].pid == pid)
        {
            deadlist[i].status = status;
            break;
        }
    }
    /* zombie not found. add a new entry */
    if(i == listindex)
    {
        deadlist[listindex].pid    = pid;
        deadlist[listindex].status = status;
        listindex++;
    }
    /* update the job table entry with the child process status */
    struct job *job = get_job_by_any_pid(pid);
    if(job)
    {
        /* if the job is stopped, save the terminal state so we can restore it later with fg or wait */
        if(WIFSTOPPED(status) && isatty(0))
        {
            if((job->tty_attr = malloc(sizeof(struct termios))))
            {
                /*
                 * if the job is a background job, we can't just save the current terminal attributes,
                 * as these will be the ones set by the shell (i.e. non-canonical mode).. in this case,
                 * we save the terminal attributes we received when the shell was started.
                 */
                if(!flag_set(job->flags, JOB_FLAG_FORGROUND))
                {
                    memcpy(job->tty_attr, &tty_attr_old, sizeof(struct termios));
                }
                else if(tcgetattr(0, job->tty_attr) == -1)
                {
                    /* failed to get tty attributes. lose the memory used for the struct */
                    free(job->tty_attr);
                    job->tty_attr = NULL;
                }
            }
        }
        else
        {
            if(job->tty_attr)
            {
                free(job->tty_attr);
            }
            job->tty_attr = NULL;
        }
        set_pid_exit_status(job, pid, status);
        set_job_exit_status(job, pid, status);
        /*
         * tcsh has a notify builtin that allows us to select to notify indiviual jobs.
         * notify if not already notified up there.
         */
        if(!option_set('b') && flag_set(job->flags, JOB_FLAG_NOTIFY))
        {
            __output_status(pid, status, 0, stderr, 1);
        }
        /*
         * ksh and zsh execute the CHLD trap handler when background
         * jobs exit and the -m option is set.
         */
        if(WIFEXITED(status) && !flag_set(job->flags, JOB_FLAG_FORGROUND) && option_set('m'))
        {
            if(job->child_exits == job->proc_count)
            {
                trap_handler(CHLD_TRAP_NUM);
            }
        }
    }
}


/*
 * reap a dead child process whose pid is given by removing it from the dead
 * list and returning its exit status.. if the child process is not found in
 * the table, return -1.
 */
int rip_dead(pid_t pid)
{
    /* the deadlist is empty */
    if(!listindex)
    {
        return -1;
    }
    /* find the process's entry in the deadlist */
    int i;
    for(i = 0; i < listindex; i++)
    {
        if(deadlist[i].pid == pid)
        {
            int status = deadlist[i].status;
            /* shift down by one */
            for( ; i < listindex; i++)
            {
                deadlist[i].pid    = deadlist[i+1].pid;
                deadlist[i].status = deadlist[i+1].status;
            }
            deadlist[i].pid    = 0;
            deadlist[i].status = 0;
            listindex--;
            /* return the dead's status */
            return status;
        }
    }
    /* entry not found */
    return -1;
}


/*
 * return a job entry given the pgid of the job.. if the job is not found, return NULL.
 */
struct job *get_job_by_pid(pid_t pgid)
{
    /* job control must be on */
    if(!option_set('m') || !pgid)
    {
        return NULL;
    }
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->pgid == pgid)
        {
            return job;
        }
    }
    return NULL;
}


/*
 * return a job entry given the pid of any process in the job pipeline..
 * if the job is not found, return NULL.
 */
struct job *get_job_by_any_pid(pid_t pid)
{
    /* job control must be on */
    if(!option_set('m') || !pid)
    {
        return NULL;
    }
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        int i;
        if(!job->pids)
        {
            continue;
        }
        for(i = 0; i < job->proc_count; i++)
        {
            if(job->pids[i] == pid)
            {
                return job;
            }
        }
    }
    return NULL;
}


/*
 * return a job entry given the job id.. if the job is not found, return NULL.
 */
struct job *get_job_by_jobid(int n)
{
    /* job control must be on */
    if(!option_set('m') || !n)
    {
        return NULL;
    }
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num == n)
        {
            return job;
        }
    }
    return NULL;
}


/*
 * set the current job.
 * 
 * returns 1 if the current job is set successfully, 0 otherwise.
 */
int set_cur_job(struct job *job)
{
    /* job control must be on */
    if(!option_set('m') || !job)
    {
        return 0;
    }
    /*
     * only make a suspended job the current one.
     * NOTE: maybe redundant, as we only call this function for suspended jobs.
     */
    if(WIFSTOPPED(job->status))
    {
        prev_job = cur_job;
        cur_job  = job->job_num;
    }
    else
    {
        if(cur_job == 0 && prev_job == 0)
        {
            cur_job = job->job_num;
        }
    }
    return 1;
}


/*
 * add a new job entry given the job pgid, the processes pid list and their count,
 * the job's command string, and a flag telling whether its a background job or not.
 * 
 * returns the new job struct, or NULL on error.
 */
struct job *add_job(pid_t pgid, pid_t pids[], int pid_count, char *commandstr, int is_bg)
{
    /* set the $! special parameter */
    if(is_bg)
    {
        //struct symtab_entry_s *entry = add_to_symtab("!");
        char buf[12];
        sprintf(buf, "%d", pgid);
        //symtab_entry_setval(entry, buf);
        set_shell_varp("!", buf);
    }

    /* job control must be on */
    if(!option_set('m'))
    {
        return NULL;
    }

    /* find an empty slot in the jobs table */
    struct job *job, *j2;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num == 0)
        {
            /* find the highest job number */
            int jnum = 0;
            for(j2 = &__jobs[0]; j2 < &__jobs[MAX_JOBS]; j2++)
            {
                if(j2->job_num > jnum)
                {
                    jnum = j2->job_num;
                }
            }
            /* initialize the job struct */
            job->job_num     = jnum+1;
            job->pgid        = pgid;
            job->commandstr  = get_malloced_str(commandstr);
            job->flags       = is_bg ? 0 : JOB_FLAG_FORGROUND;
            job->status      = 0;
            job->child_exits = 0;
            job->child_exitbits = 0;
            job->proc_count  = 0;
            job->tty_attr    = NULL;
            job->pids        = get_malloced_pids(pids, pid_count);
            if(job->pids)
            {
                size_t sz = pid_count*sizeof(int);
                job->exit_codes = malloc(sz);
                if(job->exit_codes)
                {
                    memset(job->exit_codes, 0, sz);
                    job->proc_count = pid_count;
                }
            }
            total_jobs++;
            return job;
        }
    }
    fprintf(stderr, "%s: jobs table is full\n", SHELL_NAME);
    return NULL;
}


/*
 * remove the given job from the jobs table.
 * 
 * returns the job number if it was successfully removed from the jobs table,
 * or 0 if the job is not found in the table.
 */
int kill_job(struct job *job)
{
    /* job control must be on */
    if(!option_set('m'))
    {
        return 0;
    }

    int res = 0;
    struct job *job2;
    if(job)
    {
        res = job->job_num;
        /* free the job command string */
        if(job->commandstr)
        {
            free_malloced_str(job->commandstr);
        }
        /* free the job pids table */
        if(job->pids)
        {
            free(job->pids);
        }
        /* free the job exit codes table */
        if(job->exit_codes)
        {
            free(job->exit_codes);
        }
        /* free the job terminal attributes struct */
        if(job->tty_attr)
        {
            free(job->tty_attr);
        }
        /* reset the rest of the fields */
        job->job_num     = 0;
        job->commandstr  = NULL;
        job->pids        = NULL;
        job->exit_codes  = NULL;
        job->proc_count  = 0;
        job->child_exits = 0;
        job->tty_attr    = NULL;
        /* if this is the current job, bring on the prev job to be current */
        if(res == cur_job)
        {
            cur_job  = prev_job;
            prev_job = 0;
        }
        int last_job       = 0;
        int last_suspended = 0;
        /* shift jobs down by one */
        for(job2 = &job[1]; job2 < &__jobs[MAX_JOBS]; job2++, job++)
        {
            if(job2->job_num == 0)
            {
                continue;
            }
            memcpy((void *)job, (void *)job2, sizeof(struct job));
            job2->job_num    = 0;
            job2->commandstr = 0;
            job2->proc_count = 0;
            job->child_exits = 0;
            job->tty_attr    = NULL;
            if(job->job_num > last_job)
            {
                last_job = job->job_num;
            }
            if(WIFSTOPPED(job->status) && job->job_num > last_suspended)
            {
                last_suspended = job->job_num;
            }
        }
        if(!prev_job)
        {
            if(last_suspended)
            {
                prev_job = last_suspended;
            }
            else
            {
                prev_job = last_job;
            }
        }
        total_jobs--;
    }
    return res;
};


/*
 * return the total number of jobs.
 */
int get_total_jobs()
{
    return total_jobs;
}


/*
 * alloc memory for the process pids table for a job.
 */
pid_t *get_malloced_pids(pid_t pids[], int pid_count)
{
    size_t sz = pid_count*sizeof(pid_t);
    pid_t *new_pids = malloc(sz);
    if(!new_pids)
    {
        return NULL;
    }
    memcpy(new_pids, pids, sz);
    return new_pids;
}
