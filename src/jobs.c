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
#include "signames.h"
#include "error/error.h"
#include "debug.h"

#define W_FLAG      (O_RDWR | O_CREAT | O_TRUNC )
#define A_FLAG      (O_RDWR | O_CREAT | O_APPEND)
#define R_FLAG      (O_RDONLY)

// #ifdef __detour_kernel
// #  define WAIT_FLAG (WUNTRACED)
// #else
// #  define WAIT_FLAG (WUNTRACED|WNOHANG)
// #endif

#define UTILITY         "jobs"

/* all jobs running under this shell */
struct job __jobs[MAX_JOBS];
int total_jobs   = 0;
int cur_job      = 0;
int prev_job     = 0;

/* tokens per input string */
struct cmd_token *tokens[MAX_TOKENS];

/*
 * the domacros flag tells us if we should examine the status
 * argument to extract the actual exit status. if the flag is 0,
 * we set the exit status to status, otherwise we use wait.h macros
 * to get the status.
 */
void set_exit_status(int status, int domacros)
{
    if(domacros)
    {
        if     (WIFEXITED  (status)) status = WEXITSTATUS(status);
        else if(WIFSIGNALED(status)) status = WTERMSIG   (status) + 128;
        else if(WIFSTOPPED (status)) status = WSTOPSIG   (status) + 128;
    }
    char status_str[10];
    _itoa(status_str, status);
    struct symtab_entry_s *entry = get_symtab_entry("?");
    if(entry) symtab_entry_setval(entry, status_str);
    exit_status = status;
}

void set_pid_exit_status(struct job *job, pid_t pid, int status)
{
    int i;
    if(!job || !job->pids || !job->exit_codes) return;
    for(i = 0; i < job->proc_count; i++)
    {
        if(job->pids[i] == pid)
        {
            job->exit_codes[i] = status;
            if(WIFEXITED(status)) job->child_exitbits |=  (1 << i);
            else                  job->child_exitbits &= ~(1 << i);
            break;
        }
    }
    /* count the number of children that exited */
    job->child_exits = 0;
    int j = 1;
    for(i = 0; i < job->proc_count; i++, j <<= 1)
    {
        if((job->child_exitbits & j) == j) job->child_exits++;
    }
}

void set_job_exit_status(struct job *job, pid_t pid, int status)
{
    if(!job) return;
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
        if(res != job->status) job->flags &= ~JOB_FLAG_NOTIFIED;
        job->status = res;
    }
    else
    {
        /* normal, everyday pipe (with no pipefail option) */
        if(job->pgid == pid) job->status = status;
    }
    /* now get the $! variable and set $? if needed */
    struct symtab_entry_s *entry = get_symtab_entry("!");
    if(!entry || !entry->val) return;
    char *endptr;
    long n = strtol(entry->val, &endptr, 10);
    if(endptr == entry->val) return;
    if(n == job->pgid)
    {
        entry = get_symtab_entry("?");
        if(!entry) return;
        char buf[16];
        if     (WIFEXITED  (job->status)) status = WEXITSTATUS(job->status);
        else if(WIFSIGNALED(job->status)) status = WTERMSIG   (job->status) + 128;
        else if(WIFSTOPPED (job->status)) status = WSTOPSIG   (job->status) + 128;
        else status = job->status;
        sprintf(buf, "%d", status);
        symtab_entry_setval(entry, buf);
    }
}


/* check for POSIX list terminators: ';', '\n', and '&' */
inline int is_list_terminator(char *c)
{
    if(*c == ';' || *c   == '\n') return 1;
    if(*c == '&' && c[1] !=  '&') return 1;
    return 0;
}


struct
{
    pid_t pid;
    int   status;
} deadlist[32];
int listindex = 0;

void __output_status(pid_t pid, int status, int output_pid, FILE *out, int rip_dead)
{
    struct job *job = get_job_by_any_pid(pid);
    /* 
     * no job with this pid? probably it was a subshell or some command
     * substitution task. bail out.
     */
    if(!job) return;
    char statstr[32];
    if(WIFSTOPPED(status))
    {
        if     (WSTOPSIG(status) == SIGTSTP) strcpy(statstr, "Stopped          ");
        else if(WSTOPSIG(status) == SIGSTOP) strcpy(statstr, "Stopped (SIGSTOP)");
        else if(WSTOPSIG(status) == SIGTTIN) strcpy(statstr, "Stopped (SIGTTIN)");
        else if(WSTOPSIG(status) == SIGTTOU) strcpy(statstr, "Stopped (SIGTTOU)");
    }
    else if(WIFCONTINUED(status))
    {
        return;
    }
    else if(WIFSIGNALED (status))
    {
        int sig = WTERMSIG(status);
        if(sig < 0 || sig >= total_signames)
             sprintf(statstr, "Signaled(%3d)     ", sig);
        else sprintf(statstr, "Signaled(%.9s)    ", signames[sig]);
    }
    else if(WIFEXITED   (status))
    {
        if(WEXITSTATUS(status) == 0) strcpy(statstr, "Done             ");
        else
            sprintf(statstr, "Done(%3u)      ", WEXITSTATUS(status));
    }
    else strcpy(statstr, "Running          ");

    char current = (job->job_num == cur_job ) ? '+' :
                   (job->job_num == prev_job) ? '-' : ' ';
    if(output_pid)
         fprintf(out, "[%d]%c %u %s  %s\r\n",
                 job->job_num, current, job->pgid, statstr, job->commandstr);
    else fprintf(out, "[%d]%c %s     %s\r\n",
                 job->job_num, current, statstr, job->commandstr);
    if(WIFEXITED(status) && rip_dead) kill_job(job);
    job->flags |= JOB_FLAG_NOTIFIED;
}

/*
 *            POSIX Job Control Job ID Formats:
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
int get_jobid(char *jobid_str)
{
    if(!jobid_str || !*jobid_str) return 0;
    if(*jobid_str != '%') return 0;
    if(strcmp(jobid_str, "%%") == 0) return cur_job ;
    if(strcmp(jobid_str, "%+") == 0) return cur_job ;
    if(strcmp(jobid_str, "%-") == 0) return prev_job;
    if(!*++jobid_str) return 0;
    if(*jobid_str >= '0' && *jobid_str <= '9')
    {
        return atoi(jobid_str);
    }
    struct job *job;
    if(*jobid_str == '?')
    {
        jobid_str++;
        for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
        {
            if(job->job_num == 0) continue;
            if(strstr(job->commandstr, jobid_str)) return job->job_num;
        }
    }
    else
    {
        size_t len = strlen(jobid_str);
        for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
        {
            if(job->job_num == 0) continue;
            if(strncmp(job->commandstr, jobid_str, len) == 0)
                return job->job_num;
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
            if(!WIFEXITED(job->status) || job->child_exits != job->proc_count)
                count++;
        }
    }
    return count;
}

/*
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
            if(flag && flag_set(job->flags, flag)) continue;
            pid_t pid = -(job->pgid);
            kill(pid, SIGCONT);
            kill(pid, signum);
        }
    }
}

/*
 * replace all jobspec occurences in the form of '%n' in the given argv, then
 * call command, passing it the arguments to execute them.
 */
int replace_and_run(int startat, int argc, char **argv)
{
    char buf[32];
    int i = startat;
    if(i >= argc) return 2;
    for( ; i < argc; i++)
    {
        int modified = 0;
        char *perc, *p;
        char __arg[strlen(argv[i])*2];
        char *arg = __arg;
        strcpy(arg, argv[i]);
        while((perc = strchr(arg, '%')))
        {
            char c = perc[1];
            if(c == '%' || c == '+' || c == '-' || c == '\0')
            {
                buf[0] = '%' ;
                if(c) buf[1] = c  ;
                else  buf[1] = '%';
                buf[2] = '\0';
            }
            else if(isdigit(c))
            {
                buf[0] = '%' ;
                p = buf+1;
                while(isdigit((c = *++perc))) *p++ = c;
                *p = '\0';
            }
            else if(isalpha(c) || c == '?')
            {
                buf[0] = '%' ;
                p = buf+1;
                if(c == '?') *p++ = c, c = *++perc;
                while(isalnum((c = *++perc))) *p++ = c;
                *p = '\0';
            }
            else
            {
                arg = perc+1;
                continue;
            }
            int len = strlen(buf);
            /* get the pgid of this job */
            struct job *job = get_job_by_jobid(get_jobid(buf));
            if(!job)
            {
                fprintf(stderr, "%s: unknown job: %s\r\n", UTILITY, buf);
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
                while(p1 > arg) *p2-- = *p1--;
            }
            strncpy(arg, buf, len2);
            arg = perc+len2;
            modified = 1;
        }
        
        if(modified)
        {
            p = get_malloced_str(__arg);
            if(!p) continue;
            free_malloced_str(argv[i]);     /* strings alloc'd in backend.c by calling get_malloced_str() */
            argv[i] = p;
        }
    }
    int    cargc = argc-2;
    char **cargv = &argv[2];
    return search_and_exec(cargc, cargv, NULL, SEARCH_AND_EXEC_DOFORK|SEARCH_AND_EXEC_DOFUNC);
}


int jobs(int argc, char **argv)
{
    int pids_only    = 0;
    int new_only     = 0;
    int run_only     = 0;   /* list only running jobs */
    int stop_only    = 0;   /* list only stopped jobs */
    int verbose_info = 0;
    int i;
    int finish       = 0;
    struct job *job;
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    /*
     * TODO: support bash jobs -x option. see page 110 of bash manual.
     */
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
                verbose_info = 1;
                break;
                
            case 'p':
                pids_only    = 1;
                break;
                
            case 'n':
                new_only     = 1;
                break;
                
            case 'r':
                run_only     = 1;
                break;
                
            case 's':
                stop_only    = 1;
                break;
                
            case 'x':
                return replace_and_run(v+1, argc, argv);
        }
    }
    /* unknown option */
    if(c == -1) return 1;
    
    for(i = v; i < argc; i++)
    {
        /* first try POSIX-style job ids */
        job = get_job_by_jobid(get_jobid(argv[i]));
        /* maybe we have a process pid? */
        if(!job)
        {
            char *strend;
            long pgid = strtol(argv[i], &strend, 10);
            if(strend != argv[i]) job = get_job_by_any_pid(pgid);
        }
        /* still nothing? */
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %s\r\n", UTILITY, argv[i]);
            return 1;
        }

        if(run_only  && NOT_RUNNING(job->status)) continue;
        if(stop_only && !WIFSTOPPED(job->status)) continue;
        
        if(pids_only) printf("%d\r\n", job->pgid);
        else
        {
            if(!new_only || !flag_set(job->flags, JOB_FLAG_NOTIFIED))
            {
                if(!flag_set(job->flags, JOB_FLAG_DISOWNED))
                    __output_status(job->pgid, job->status, verbose_info, stdout, 1);
            }
        }
        finish = 1;
    }
    if(finish) return 0;

    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num != 0)
        {
            if(run_only  && NOT_RUNNING(job->status)) continue;
            if(stop_only && !WIFSTOPPED(job->status)) continue;
            
            if(pids_only) printf("%d\r\n", job->pgid);
            else
            {
                if(!new_only || !flag_set(job->flags, JOB_FLAG_NOTIFIED))
                {
                    if(!flag_set(job->flags, JOB_FLAG_DISOWNED))
                        __output_status(job->pgid, job->status, verbose_info, stdout, 0);
                }
            }
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
            if(WIFEXITED(job->status))
                kill_job(job);
    }
    return 0;
}

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
                    __output_status(deadlist[i].pid, deadlist[i].status, 0, stderr, 1);
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
        pid_t pid = waitpid(-1, &status, WAIT_FLAG|WNOHANG);
        if(pid <= 0) break;
        struct job *job = get_job_by_any_pid(pid);
        if(job)
        {
            //set_pid_exit_status(job, pid, status);
            set_job_exit_status(job, pid, status);
            /* report job status only if the last command finished execution */
            if(job->child_exits == job->proc_count)
            {
                if(!flag_set(job->flags, JOB_FLAG_FORGROUND))
                    __output_status(pid, status, 0, stderr, 1);
                kill_job(job);
            }
            else if(!WIFEXITED(status) && !flag_set(job->flags, JOB_FLAG_NOTIFIED))
            {
                __output_status(pid, status, 0, stderr, 1);
            }
        }
    }
}

void notice_termination(pid_t pid, int status)
{
    if(pid <= 0) return;
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
    /* not found. add a new entry */
    if(i == listindex)
    {
        deadlist[listindex].pid    = pid;
        deadlist[listindex].status = status;
        listindex++;
    }
    struct job *job = get_job_by_any_pid(pid);
    if(job)
    {
        set_pid_exit_status(job, pid, status);
        //if(pid == job->pgid) set_job_exit_status(job, status);
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
         * ksh executes the CHLD trap handler when background
         * jobs exit and the -m option is set.
         */
        if(WIFEXITED(status) && !flag_set(job->flags, JOB_FLAG_FORGROUND) && option_set('m'))
        {
            trap_handler(CHLD_TRAP_NUM);
        }
    }
}

int rip_dead(pid_t pid)
{
    if(!listindex) return -1;
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
            return status;
        }
    }
    return -1;
}

struct job *get_job_by_pid(pid_t pgid)
{
    if(!pgid) return NULL;
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->pgid == pgid) return job;
    }
    return NULL;
}

struct job *get_job_by_any_pid(pid_t pid)
{
    if(!pid) return NULL;
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        int i;
        if(!job->pids) continue;
        for(i = 0; i < job->proc_count; i++)
            if(job->pids[i] == pid) return job;
    }
    return NULL;
}

struct job *get_job_by_jobid(int n)
{
    if(!n) return NULL;
    struct job *job;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num == n) return job;
    }
    return NULL;
}

int set_cur_job(struct job *job)
{
    if(!job) return 0;
    /* only make a suspended job the current one.
     * NOTE: maybe redundant, as we only call this function for suspended jobs.
     */
    if(WIFSTOPPED(job->status))
    {
        prev_job   = cur_job;
        cur_job    = job->job_num;
    }
    else
    {
        if(cur_job == 0 && prev_job == 0)
            cur_job = job->job_num;
    }
    return 1;
}

struct job *add_job(pid_t pgid, pid_t pids[], int pid_count, char *commandstr, int is_bg)
{
    struct job *job, *j2;
    for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
    {
        if(job->job_num == 0)
        {
            int jnum = 0;
            for(j2 = &__jobs[0]; j2 < &__jobs[MAX_JOBS]; j2++)
            {
                if(j2->job_num > jnum) jnum = j2->job_num;
            }
            job->job_num     = jnum+1;
            job->pgid        = pgid;
            job->commandstr  = get_malloced_str(commandstr);
            job->flags       = is_bg ? 0 : JOB_FLAG_FORGROUND;
            job->status      = 0;
            job->child_exits = 0;
            job->child_exitbits = 0;
            job->proc_count  = 0;
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
            if(is_bg)
            {
                struct symtab_entry_s *entry = add_to_symtab("!");
                char buf[12];
                sprintf(buf, "%d", pgid);
                symtab_entry_setval(entry, buf);
            }
            total_jobs++;
            return job;
        }
    }
    return NULL;
}


int kill_job(struct job *job)
{
    int res = 0;
    struct job *job2;
    if(job)
    {
        res = job->job_num;
        if(job->commandstr) free_malloced_str(job->commandstr);
        if(job->pids      ) free(job->pids      );
        if(job->exit_codes) free(job->exit_codes);
        job->job_num     = 0;
        job->commandstr  = NULL;
        job->pids        = NULL;
        job->exit_codes  = NULL;
        job->proc_count  = 0;
        job->child_exits = 0;
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
            if(job2->job_num == 0) continue;
            memcpy((void *)job, (void *)job2, sizeof(struct job));
            job2->job_num    = 0;
            job2->commandstr = 0;
            job2->proc_count = 0;
            job->child_exits = 0;
            if(job->job_num > last_job) last_job = job->job_num;
            if(WIFSTOPPED(job->status) && job->job_num > last_suspended)
                last_suspended = job->job_num;
        }
        if(!prev_job)
        {
            if(last_suspended) prev_job = last_suspended;
            else               prev_job = last_job;
        }
        total_jobs--;
    }
    return res;
};

int get_total_jobs() { return total_jobs; }

pid_t *get_malloced_pids(pid_t pids[], int pid_count)
{
    size_t sz = pid_count*sizeof(pid_t);
    pid_t *new_pids = malloc(sz);
    if(!new_pids) return NULL;
    memcpy(new_pids, pids, sz);
    return new_pids;
}

struct cmd_token *get_heredoc(char *_cmd, int strip)
{
    char *cmd   = _cmd;
    char delim[32];
    int  expand = 1;        /* expand by default, unless delimiter is quoted */
    int  heredoc_len = 0;
    while(*cmd && !isspace(*cmd) && *cmd != ';' && *cmd != '&') cmd++;
    if(cmd == _cmd) delim[0] = '\0';
    else
    {
        /* copy delimiter */
        char *c1 = _cmd;
        char *c2 = delim;
        while(c1 < cmd)
        {
            if(*c1 == '"' || *c1 == '\'')
            {
                c1++;
                expand = 0;
            }
            else
            {
                if(*c1 == '\\')
                {
                    expand = 0;
                    c1++;
                }
                *c2++ = *c1++;
            }
        }
        *c2++ = '\n';
        *c2   = '\0';
        /* now remove it from the original command string */
        c2 = _cmd;
        c1 = cmd;
        while((*c2++ = *c1++)) ;
    }
    /* get the next new line */
    char *nl = strchr(_cmd, '\n');
    char *end;
    if(!nl)
    {
        BACKEND_RAISE_ERROR(HEREDOC_MISSING_NEWLINE, NULL, NULL);
        return NULL;
    }
    /* get the heredoc length */
    if(delim[0] == '\0')
    {
        BACKEND_RAISE_ERROR(HEREDOC_EXPECTED_DELIM, NULL, NULL);
        return NULL;
    }
    else
    {
        end = strstr(_cmd, delim);
        if(!end)
        {
            /* remove the newline char to print delimiter nicely */
            delim[cmd - _cmd] = '\0';
            BACKEND_RAISE_ERROR(HEREDOC_EXPECTED_DELIM, delim, NULL);
            return NULL;
        }
        heredoc_len = end - nl;
    }
    /* now get the heredoc proper */
    char *heredoc = (char *)malloc(heredoc_len+1);
    if(!heredoc)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "saving heredoc", NULL);
        return NULL;
    }
    char *p1 = nl+1;
    char *p2 = heredoc;
    do
    {
        if(*p1 == '\n')
        {
            /* stripping tabs */
            if(strip) while(*p1 == '\t') p1++;
        }
        *p2++ = *p1++;
    } while(p1 < end);
    *p2 = '\0';
    /* remove the heredoc from the original command string */
    p2 = nl+1;
    p1 = end;
    p1 += strlen(delim);
    while((*p2++ = *p1++)) ;
    
    struct cmd_token *t = (struct cmd_token *)malloc(sizeof(struct cmd_token));
    if(!t)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "parsing heredoc", NULL);
        return NULL;
    }
    t->data    = heredoc;
    t->len     = strlen(heredoc);
    if(expand) t->token_type = HEREDOC_TOKEN_EXP;
    else       t->token_type = HEREDOC_TOKEN_NOEXP;
    return t;
}
