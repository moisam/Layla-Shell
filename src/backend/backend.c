/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: backend.c
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

/* macro definitions needed to use sig*() and setenv() */
#define _POSIX_C_SOURCE 200112L
/* for uslepp(), but also _POSIX_C_SOURCE shouldn't be >= 200809L */
#define _XOPEN_SOURCE   500

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include "backend.h"
#include "../error/error.h"
#include "../debug.h"
#include "../kbdevent.h"
#include "../builtins/setx.h"

/* current function level (number of nested function calls) */
int cur_func_level = 0;

/* declared in main.c   */
extern int read_stdin;


/*
 * merge the local symbol table of a builtin or function with the global
 * symbol table of the shell after the builtin or function finishes execution.
 */
#define MERGE_GLOBAL_SYMTAB()                       \
do {                                                \
    struct symtab_s *symtab = symtab_stack_pop();   \
    merge_global(symtab);                           \
    free_symtab(symtab);                            \
} while(0)


/* 
 * in tcsh, if an interactive program exits with non-zero exit status,
 * the shell prints a message with the exit status.
 */

#define PRINT_EXIT_STATUS(status)                   \
do {                                                \
    if(option_set('i') &&                           \
       optionx_set(OPTION_PRINT_EXIT_VALUE) &&      \
       WIFEXITED((status)) && WEXITSTATUS((status)))\
    {                                               \
        fprintf(stderr, "Exit %d\n",              \
                WEXITSTATUS(status));               \
    }                                               \
} while(0)


/* defined in jobs.c */
int rip_dead(pid_t pid);

/* defined in redirect.c */
int __redirect_do(struct io_file_s *io_files, int do_savestd);

/* defined in builtins/coproc.c */
int    coproc(int argc, char **argv, struct io_file_s *io_files);

/* defined below */
char *get_cmdstr(struct node_s *cmd);


/*
 * try to fork a child process. if failed, try to a maximum number of retries, then return.
 * if successful, the function returns the pid of the new child process (in the parent), or
 * zero (in the child), or a negative value in case of error.
 */
pid_t fork_child()
{
    int tries = 5;
    useconds_t usecs = 1;
    pid_t pid;
    while(tries--)
    {
        if((pid = fork()) < 0)
        {
            /* if we reached the system's limit on child process count, retry after a little nap */
            if(errno == EAGAIN)
            {
                if(usleep(usecs << 1) != 0)
                {
                    break;
                }
                continue;
            }
            break;
        }
        break;
    }
    return pid;
}


/*
 * wait on the child process with the given pid until it changes status.
 * if the struct job *job is passed, wait for all processes in the job to finish,
 * then update the job's status. if the command is stopped or signalled and no job
 * has been added for it, use the *cmd nodetree to re-construct the command line
 * and add it as a background job.
 * the return value is the exit status of the waited-for child process, or -1 on error.
 */
int wait_on_child(pid_t pid, struct node_s *cmd, struct job *job)
{
    int   status = 0;
    pid_t res;
    
_wait:
    res = waitpid(pid, &status, WAIT_FLAG);
    /*
     * error fetching child exit status. of all the possible causes,
     * most probably is the fact that there is no children (in our case).
     * which probably means that the exit status was collected
     * in the SIGCHLD_handler() function in main.c
     */
    if(res < 0)
    {
        if((status = rip_dead(pid)) < 0)
        {
            goto _wait;
        }
    }
    /* collect the status. if stopped, add as background job */
    if(option_set('m') && (WIFSTOPPED(status) || WIFSIGNALED(status)))
    {
        if(!job)
        {
            /* TODO: use correct command str */
            char *cmdstr = get_cmdstr(cmd);
            job = add_job(pid, (pid_t[]){pid}, 1, cmdstr, 1);
            if(cmdstr)
            {
                free(cmdstr);
            }
        }
        set_pid_exit_status(job, pid, status);
        set_cur_job(job);
        notice_termination(pid, status);
    }
    else
    {
        set_exit_status(status, 1);
        set_pid_exit_status(job, pid, status);
        //if(pid == job->pgid) set_job_exit_status(job, status);
        set_job_exit_status(job, pid, status);
        
        /* wait on every process in the job to finish execution */
        if(job)
        {
            if(job->exit_codes && job->pids && job->child_exits < job->proc_count)
            {
                int i, j = 1;
                for(i = 0; i < job->proc_count; i++, j <<= 1)
                {
                    /*
                     * even a process that exited with 0 exit status should have
                     * a non-zero status field (that's why we check exit status using
                     * the macro WIFEXITED, not by hand).
                     */
                    if(!(job->child_exitbits & j))
                    {
                        pid = job->pids[i];
                        status = 0;
                        goto _wait;
                    }
                }
            }
            job->flags |= JOB_FLAG_NOTIFIED;
            status = job->status;
        }
    }
    return status;
}

/*
 * POSIX defines how background jobs should handle signals
 * and read from /dev/null.
 * we will do this preparation in this function.
 */
void asynchronous_prologue()
{
    /*
     * POSIX says we should restore non-ignored signals to their
     * default actions.
     */
    reset_nonignored_traps();

    if(!option_set('m'))
    {
        signal(SIGINT , SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT );
        sigaddset(&mask, SIGQUIT);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        close(0);
        open("/dev/null", O_RDONLY);
    }
}

/*
 * set the value of the underscore '$_' variable.
 */
static inline void set_underscore_val(char *val, int set_env)
{
    struct symtab_entry_s *entry = add_to_symtab("_");
    if(!entry)
    {
        return;
    }
    symtab_entry_setval(entry, val);
    if(set_env) setenv("_", val, 1);
}

/*
 * get the first line of a script file, which should read like:
 *      #!interpreter [options]
 */
char *get_first_line(char *path)
{
    if(!path || !*path)
    {
        return NULL;
    }
    FILE *f = fopen(path, "r");
    if(!f)
    {
        return NULL;
    }
    char buf[256];                  /* old Unixes allow a max of 32 */
    char *res = NULL;
    if(fgets(buf, 256, f))
    {
        size_t len = strlen(buf);
        if(buf[len-1] == '\n')      /* TODO: handle incomplete (long) first lines */
        {
            buf[len-1] = '\0';
            if(buf[0] == '#' && buf[1] == '!')
            {
                res = get_malloced_str(buf);
            }
        }
    }
    fclose(f);
    return res;
}

/*
 * if a command file is not executable, try to execute it as a shell script by
 * reading the first line to determine the interpreter program we need to invoke
 * on the script. if no suitable first line is found, we try to invoke our own
 * shell by checking the value of the 'shell' special alias, or argv[0] if the
 * alias is not set.
 */
void do_exec_script(char *path, int argc, char **argv)
{
    /* try executing the shell (or other interpreter) with cmd as argument */
    char *first = get_first_line(path);
    char *argv2[argc+3];
    int i = 0, lsh = 0;
    if(!first)
    {
        lsh = 1;     /* use our shell */
    }
    /* does the first line contain interpreter name with an optional argument? */
    char *space = first ? strchr(first, ' ') : NULL;
    if(lsh)
    {
        /* in tcsh, special alias 'shell' give the full pathname of the shell to use */
        char *s = "shell";
        char *sh = parse_alias(s);
        if(sh != s && sh != null_alias)
        {
             space = strchr(sh, ' ');
             argv2[i++] = sh;
        }
        else
        {
            argv2[i++] = shell_argv[0];
        }
    }
    else
    {
        argv2[i++] = first+2;           /* skip the '#!' part */
    }

    if(space)
    {
        *space = '\0';
        while(isspace(*++space)) ;
        argv2[i++] = space;
    }
    /* copy the rest of args */
    char **v1 = argv, **v2 = &argv2[i];
    while((*v2++ = *v1++)) ;
    /* fork a subshell to execute the script */
    pid_t pid;
    if((pid = fork_child()) < 0)
    {
        fprintf(stderr, "%s: failed to fork subshell to execute script: %s\n",
                SHELL_NAME, strerror(errno));
        return;
    }
    else if(pid == 0)
    {
        set_underscore_val(argv2[0], 1);    /* absolute pathname of command exe */
        /*
         * TODO: turn off resricted mode in the new shell (bash).
         */
        execvp(argv2[0], argv2);
    }
    else
    {
        int status = 0;
        waitpid(pid, &status, WAIT_FLAG);
        if(first)
        {
            free_malloced_str(first);
        }
    }
}

/*
 * last part of the POSIX algorithm for command search and execution. this function handles
 * the search and execution of external commands. if passed a pointer to a builtin utility's
 * function, it calls that function to execute the builtin and return. otherwise, it searches
 * $PATH (or alternatively *use_path if not NULL) to find the external executable.

 * returns the exit status if the command is a builtin utility, otherwise it shouldn't return
 * at all (if the command is external). in the latter case, the return result is zero to tell
 * the caller we've failed in executing the command.
 */
int do_exec_cmd(int argc, char **argv, char *use_path, int (*internal_cmd)(int, char **))
{

    if(internal_cmd)
    {
        int res = internal_cmd(argc, argv);
        set_underscore_val(argv[argc-1], 0);
        return res;
    }
    /*
     * zsh has a useful builtin extension called the precommand modifier, where a special word
     * preceding the command name changes how the command interprets that name. in this case,
     * the - modifier causes the shell to add - to the beginning of argv[0] of the command.
     * this is similar to calling exec with the -l option in bash (and in this shell too).
     */
    char  *cmdname = argv[0];
    char **cmdargs = argv;
    if(cmdname[0] == '-' && cmdname[1] == '\0' && !option_set('P'))
    {
        if(argc < 2)
        {
            return 0;
        }
        cmdname = argv[1];
        char buf[strlen(cmdname)+2];
        sprintf(buf, "-%s", cmdname);
        argv[1] = get_malloced_str(buf);
        if(!argv[1])
        {
            return 0;
        }
        cmdargs = &argv[1];
    }
    /* STEP 1-D: search for command using $PATH if there is no slash in the command name */
    if(strchr(cmdname, '/'))
    {
        /* is this shell restricted? */
        if(startup_finished && option_set('r'))
        {
            /* r-shells can't specify commands with '/' in their names */
            fprintf(stderr, "%s: can't execute '%s': restricted shell\n", SHELL_NAME, cmdname);
            goto err;
        }
        set_underscore_val(cmdname, 1);    /* absolute pathname of command exe */
        execv(cmdname, cmdargs);
        if(errno == ENOEXEC)
        {
            do_exec_script(cmdname, argc-1, cmdargs);
        }
    }
    else
    {
        /* check for a hashed utility name */
        char *path;
        if(option_set('h'))
        {
            path = get_hashed_path(cmdname);
            if(path)
            {
                int go = 1;
                /* check the hashed path still exists (bash) */
                if(optionx_set(OPTION_CHECK_HASH))
                {
                    go = file_exists(path);
                }
                if(go)
                {
                    set_underscore_val(path, 1);    /* absolute pathname of command exe */
                    execv(path, cmdargs);
                }
            }
        }
        
        /*
         * if we came back, we failed to execute the utility.
         * try searching for another utility using the given path.
         */
        path = search_path(cmdname, use_path, 1);
        if(!path)
        {
            goto err;
        }
        set_underscore_val(path, 1);    /* absolute pathname of command exe */
        execv(path, cmdargs);
        
        if(errno == ENOEXEC)
        {
            do_exec_script(path, argc-1, cmdargs);
        }
        free_malloced_str(path);
    }
    
err:
    if(cmdname != argv[0])
    {
        free_malloced_str(cmdname);
    }
    return 0;
}


/*
 * convert a nodetree to a string containing the command line of the command specified
 * in the nodetree. returns the malloc'd command line, or NULL in case of error.
 */
char *get_cmdstr(struct node_s *cmd)
{
    /*
     * we call __get_malloced_str() because the other call to cmd_nodetree_to_str()
     * doesn't call get_malloced_str(), which will confuse our caller when it wants
     * to free the returned string.
     */
    if(!cmd)
    {
        return __get_malloced_str("(no command)");
    }
    return cmd_nodetree_to_str(cmd);
}


/*
 * execute a complete command. this function is called by do_translation_unit(). in turn,
 * it calls do_list() to handle the complete command, which is nothing more than a list (see
 * the next function below).
 * 
 * returns 1 if the nodetree is executed without errors (such as syntax and I/O redirection
 * errors), otherwise 0. note that a successful (1) result only means the executor have succeeded
 * in running the nodetree, it doesn't mean the commands were ran successfully (you need to
 * check the exit_status variable's value to get the exit status of the last command executed).
 */
int  do_complete_command(struct node_s *node)
{
    return do_list(node, NULL);
}


/*
 * execute a list, which can be asynchronous (background) or sequential (foreground).
 * for asynchronous lists, we fork a child process and let it handle the list. for
 * sequential lists, we call other functions to handle the list, and we wait for the
 * list to finish execution.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_list(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    if(node->type != NODE_LIST)
    {
        return do_and_or(node, NULL, 1);
    }
    struct node_s *cmd = last_child(node);
    struct node_s *redirects = (cmd && cmd->type == NODE_IO_REDIRECT_LIST) ? cmd : redirect_list;
    cmd = node->first_child;
    
    /* is this a background job? */
    int wait = 1;
    if(node->val_type == VAL_CHR)
    {
        wait = (node->val.chr == '&') ? 0 : 1;
    }
    else if(node->val_type == VAL_STR)
    {
        /* if it ends in unquoted &, then yes */
        int c1 = strlen(node->val.str)-1;
        int c2 = c1-1;
        if(c2 < 0)
        {
            c2 = 0;
        }
        if(node->val.str[c1] == '&' && node->val.str[c2] != '\\')
        {
            wait = 0;
        }
    }

    if(!wait)
    {
        pid_t pid;
        if((pid = fork_child()) < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
        else if(pid > 0)
        {
            setpgid(pid, 0);
            struct job *job;
            char *cmdstr = get_cmdstr(node);
            /* add new job, or set $! if job control is off */
            job = add_job(pid, (pid_t[]){pid}, 1, cmdstr, 1);
            /*
             * if job control is on, set the current job, or complain if the
             * job couldn't be added.
             */
            if(option_set('m'))
            {
                if(!job)
                {
                    BACKEND_RAISE_ERROR(FAILED_TO_ADD_JOB, NULL, NULL);
                    if(cmdstr)
                    {
                        free(cmdstr);
                    }
                    //return 0;
                }
                else
                {
                    set_cur_job(job);
                    fprintf(stderr, "[%d] %u\n", job->job_num, pid);
                }
            }
            set_exit_status(0, 0);
            /*
             * give the child process a headstart, in case the scheduler decided to run us first.
             * we wouldn't need to do this if we used vfork() instead of fork(), but the former has
             * a lot of limitations and things we need to worry about if we're using it. still, it
             * would be worth our while using vfork() instead of fork().
             * TODO: use vfork() in this function and in do_complete_command() instead of fork().
             */
            sleep(1);
            /*
            int status;
            if(waitpid(pid, &status, WNOHANG) > 0)
            {
                set_pid_exit_status(job, pid, status);
                set_job_exit_status(job, pid, status);
            }
            */
            if(cmdstr)
            {
                free(cmdstr);
            }
            return 1;
        }
        else
        {
            setpgid(0, pid);
            asynchronous_prologue();
            int res = do_and_or(cmd, redirects, 0);
            if(!res)
            {
                exit(exit_status);
            }
            if(cmd->next_sibling)
            {
                do_list(cmd->next_sibling, redirects);
            }
            exit(exit_status);
        }
    }
    int res = do_and_or(cmd, redirects, 1 /* wait */);
    if(!res)
    {
        return 0;
    }
    if(cmd->next_sibling)
    {
        return do_list(cmd->next_sibling, redirects);
    }
    return res;
}

/*
 * execute and AND-OR list, which consists of one or more pipelines, joined by
 * AND '&&' and OR '||' operators.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_and_or(struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node)
    {
        return 0;
    }
    if(node->type != NODE_ANDOR)
    {
        return do_pipeline(node, redirect_list, fg);
    }
    node = node->first_child;
    struct node_s *cmd = node;
    int res = 0;
    int esave = option_set('e');
    set_option('e', 0);
loop:
    res = do_pipeline(cmd, redirect_list, fg);
    /* exit on failure? only applicable for last command in AND-OR chain */
    if((!res || exit_status) && !node->next_sibling && esave)
    {
        set_option('e', esave);
        exit_gracefully(exit_status, NULL);
    }
    if(res == 0)
    {
        return 0;
    }
loop2:
    node = node->next_sibling;
    if(!node)
    {
        return 1;
    }
    cmd  = node->first_child;
    if(!cmd)
    {
        return 1;
    }
    if(exit_status == 0)    /* success */
    {
        if(node->type == NODE_AND_IF)
        {
            goto loop;
        }
        else
        {
            goto loop2;
        }
    }
    /* failure */
    if(node->type == NODE_AND_IF)
    {
        goto loop2;
    }
    else
    {
        goto loop;
    }
}

/*
 * execute a pipeline, which consists of a group of commands joined by the
 * pipe operator '|'. for each pipe operator, we create a two-way pipe. we join
 * stdout of the command on the leftside of the operator to stdin of the command
 * on the rightside of the operator. if the pipeline is preceded by a bang '!',
 * we logically inverse the pipeline's exit status after it finishes execution.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_pipeline(struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node)
    {
        return 0;
    }
    int is_bang = 0;
    if(node->type == NODE_BANG)
    {
        is_bang = 1;
        node = node->first_child;
    }
    int res = do_pipe_sequence(node, redirect_list, fg);
    if(res && is_bang)
    {
        set_exit_status(!exit_status, 0);
    }
    /* exit on failure? */
    if(!is_bang && (!res || exit_status))
    {
        /*
         * NOTE: we are mixing POSIX and ksh-behavior, where ERR trap is exec'd
         *       when a command has non-zero exit status (ksh), while -e causes
         *       errors to execute ERR trap (ksh) and exit the shell (POSIX).
         */
        trap_handler(ERR_TRAP_NUM);
        if(option_set('e'))
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }
    }
    return res;
}

/*
 * execute a pipe sequence, which is a pipeline without the optional bang '!' operator.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_pipe_sequence(struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node)
    {
        return 0;
    }
    if(node->type != NODE_PIPE)
    {
        return do_command(node, redirect_list, fg);
    }
    struct node_s *cmd = node->first_child;
    pid_t pid;
    pid_t all_pids[node->children];
    int count = 0;
    int filedes[2];

    /* create pipe */
    pipe(filedes);
    /*
     * last command of a foreground pipeline (in the absence of job control) will 
     * be run by the shell itself (bash).
     */
    int lastpipe = (fg && !option_set('m') && optionx_set(OPTION_LAST_PIPE));
    if(lastpipe)
    {
        pid = tty_pid;
        close(0);   /* stdin */
        dup2(filedes[0], 0);
    }
    else
    {
        /* fork the last command */
        if((pid = fork_child()) == 0)
        {
            /* tell the terminal who's the foreground pgid now */
            if(option_set('m'))
            {
                setpgid(0, 0);
                if(fg)
                {
                    tcsetpgrp(0, pid);
                }
            }
            reset_nonignored_traps();
            /* 2nd command component of command line */
            close(0);   /* stdin */
            dup2(filedes[0], 0);
            close(filedes[0]);
            close(filedes[1]);
            /* standard input now comes from pipe */
            do_command(cmd, redirect_list, 0);
            exit(exit_status);
        }
        else if(pid < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
    }
    if(option_set('m'))
    {
        setpgid(pid, pid);
        if(pid != tty_pid)
        {
            tcsetpgrp(0, pid);
        }
    }
    all_pids[count++] = pid;
    cmd = cmd->next_sibling;
    while(cmd)
    {
        pid_t pid2;
        struct node_s *next = cmd->next_sibling;
        int filedes2[2];
        if(next)
        {
            pipe(filedes2);
        }
        /* fork the first command */
        if((pid2 = fork_child()) == 0)
        {
            if(option_set('m'))
            {
                setpgid(0, pid);
                if(fg)
                {
                    tcsetpgrp(0, pid);
                }
            }
            /* only restore tty to canonical mode if we are reading from it */
            if(read_stdin /* isatty(0) */)
            {
                term_canon(1);
            }
            reset_nonignored_traps();
            /* first command of pipeline */
            close(1);   /* stdout */
            dup2(filedes[1], 1);
            close(filedes[1]);
            close(filedes[0]);
            /* stdout now goes to pipe */
            /* child process does command */
            if(next)
            {
                close(0);   /* stdin */
                dup2(filedes2[0], 0);
                close(filedes2[0]);
                close(filedes2[1]);
            }
            do_command(cmd, redirect_list, 0);
            exit(exit_status);
        }
        else if(pid2 < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
        all_pids[count++] = pid2;
        close(filedes[1]);
        close(filedes[0]);
        if((cmd = next))
        {
            filedes[0] = filedes2[0];
            filedes[1] = filedes2[1];
        }
    }

    /* TODO: use correct command str */
    char *cmdstr = get_cmdstr(node);

    /* $! will be set in add_job() */
    struct job *job = NULL;

    /* add new job, or set $! if job control is off */
    job = add_job(pid, all_pids, count, cmdstr, !fg);
    /*
     * if job control is on, set the current job.
     */
    if(option_set('m'))
    {
        set_cur_job(job);
    }

    /* free temp memory */
    if(cmdstr)
    {
        free(cmdstr);
    }
    /* run the last command in this process if extended option 'lastpipe' is set (bash) */
    if(lastpipe)
    {
        do_command(cmd, redirect_list, 0);
        if(job)
        {
            set_pid_exit_status(job, pid, exit_status);
            job->child_exitbits |= 1;       /* mark our entry as done */
            job->child_exits++;
        }
        close(0);   /* restore stdin */
        open("/dev/tty", O_RDWR);
    }
    
    if(fg)
    {
        /* tell the terminal who's the foreground pgid now */
        int status = wait_on_child(pid, node, job);
        /* reset the terminal's foreground pgid */
        if(option_set('m'))
        {
            tcsetpgrp(0, tty_pid);
        }
        PRINT_EXIT_STATUS(status);
        set_exit_status(status, 1);
        return !status;
    }
    else
    {
        fprintf(stderr, "[%d] %u %s\n", job->job_num, pid, job->commandstr);
        set_exit_status(0, 0);
        return 1;
    }
}

/*
 * execute a term, which consists of one or more AND-OR lists.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_term(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    if(node->type != NODE_TERM)
    {
        return do_and_or(node, redirect_list, 1);
    }
    int wait = (node->val.chr == '&') ? 0 : 1;
    if(!wait)
    {
        pid_t pid;
        if((pid = fork_child()) < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
        else if(pid > 0)
        {
            struct job *job = NULL;
            char *cmdstr = get_cmdstr(node->first_child);
            /* add new job, or set $! if job control is off */
            job = add_job(pid, (pid_t[]){pid}, 1, cmdstr, 1);
            set_exit_status(0, 0);
            /*
             * if job control is on, set the current job.
             */
            if(option_set('m'))
            {
                if(!job)
                {
                    BACKEND_RAISE_ERROR(FAILED_TO_ADD_JOB, NULL, NULL);
                    if(cmdstr)
                    {
                        free(cmdstr);
                    }
                    return 0;
                }
                else
                {
                    set_cur_job(job);
                }
            }
            if(cmdstr)
            {
                free(cmdstr);
            }
            return 1;
        }
        else
        {
            asynchronous_prologue();
            struct node_s *child = node->first_child;
            int res = do_and_or(child, redirect_list, 0);
            if(!res)
            {
                exit(exit_status);
            }
            if(child->next_sibling)
            {
                do_term(child->next_sibling, redirect_list);
            }
            exit(exit_status);
        }
    }
    struct node_s *child = node->first_child;
    int res = do_and_or(child, redirect_list, 1 /* wait */);
    if(!res)
    {
        return 0;
    }
    if(child->next_sibling)
    {
        return do_term(child->next_sibling, redirect_list);
    }
    return res;
}

/*
 * execute a compound list, which forms the body of most compound commands,
 * such as loops and conditionals. a compound list consists of one or more terms.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_compound_list(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    if(node->type != NODE_LIST)
    {
        return do_term(node, redirect_list);
    }
    node = node->first_child;
    int res = 0;
    while(node)
    {
        /* execute the first term (or list) */
        res = do_term(node, redirect_list);
        /* error executing the term */
        if(!res)
        {
            break;
        }
        /* break or continue encountered inside a loop's do-done group */
        if(cur_loop_level && (req_break || req_continue))
        {
            break;
        }
        node = node->next_sibling;
    }
    return res;
}

/*
 * execute a nodetree in a subshell.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_subshell(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *subshell = node->first_child;
    struct node_s *local_redirects = subshell ? subshell->next_sibling : NULL;
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }
    pid_t pid;
    if((pid = fork_child()) < 0)
    {
        BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
        return 0;
    }
    else if(pid > 0)
    {
        wait_on_child(pid, node, NULL);
        if(redirect_list)
        {
            redirect_restore();
        }
        if(option_set('m'))
        {
            tcsetpgrp(0, tty_pid);
        }
        return 1;
    }

    /* init our subshell environment */
    init_subshell();

    /* perform I/O redirections */
    if(redirect_list)
    {
        if(!redirect_do(redirect_list))
        {
            return 0;
        }
    }

    /* do the actual commands */
    do_compound_list(subshell, NULL /* redirect_list */);
    /* no need to pop symtab or restore traps as we are exiting anyway */
    exit(exit_status);
}

/*
 * execute a compound list of commands that has been enclosed between the 'do' and 'done'
 * keywords, a.k.a. do groups. these groups form the body of loops (for, while, until).
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_do_group(struct node_s *node, struct node_s *redirect_list)
{
    /*
     * this will take care of executing ERR trap for while, until, 
     * select and for loops.
     */
    int res = do_compound_list(node, redirect_list);
    ERR_TRAP_OR_EXIT();
    return res;
}


/*
 * convert a tree of tokens into a command string (i.e. re-create the original command line
 * from the token tree.
 * 
 * returns the malloc'd command string, or NULL if there is an error.
 */
char *__tok_to_str(struct word_s *tok)
{
    if(!tok)
    {
        return NULL;
    }
    size_t len = 0;
    struct word_s *t = tok;
    while(t)
    {
        t->len = strlen(t->data);
        len += t->len+1;
        t    = t->next;
    }
    char *str = (char *)malloc(len+1);
    if(!str)
    {
        return NULL;
    }
    char *str2 = str;
    t = tok;
    while(t)
    {
        sprintf(str2, "%s ", t->data);
        str2 += t->len+1;
        t     = t->next;
    }
    /* remove the last separator */
    str2[-1] = '\0';
    return str;
}


/*
 * execute a brace group (commands enclosed in curly braces '{' and '}');
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_brace_group(struct node_s *node, struct node_s *redirect_list)
{
    int res = do_compound_list(node, redirect_list);
    ERR_TRAP_OR_EXIT();
    return res;
}


/*
 * execute a compound command (loops and conditionals) by calling the appropriate
 * delegate function to handle each command.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_compound_command(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    switch(node->type)
    {
        case NODE_SUBSHELL: return do_subshell     (node, redirect_list);
        case NODE_FOR     : return do_for_clause   (node, redirect_list);
        case NODE_CASE    : return do_case_clause  (node, redirect_list);
        case NODE_IF      : return do_if_clause    (node, redirect_list);
        case NODE_WHILE   : return do_while_clause (node, redirect_list);
        case NODE_UNTIL   : return do_until_clause (node, redirect_list);
        case NODE_SELECT  : return do_select_clause(node, redirect_list);
        case NODE_LIST    : return do_brace_group  (node, redirect_list);
        default           : return 0;
    }
}


/*
 * execute a function's body (which is nothing more than a compound command).
 * after executing the function body, the RETURN trap is executed before returning.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_function_body(struct node_s *func)
{
    struct node_s *child    = func->first_child;
    struct node_s *prev     = child;
    struct node_s *next     = child->next_sibling;
    struct node_s *redirect = NULL;
    while(next)
    {
        prev = next;
        next = next->next_sibling;
    }
    child = prev;
    if(child->type == NODE_IO_REDIRECT_LIST)
    {
        redirect = child;
        child->prev_sibling->next_sibling = NULL;
    }
    int res = do_compound_command(func, redirect);
    if(redirect)
    {
        /* 
         * restore the redirect list as we will need
         * to use it if the function is later invoked again.
         */
        child->prev_sibling->next_sibling = redirect;
    }
    /*
     * clear the return flag so that we won't cause the parent 
     * shell to exit as well.
     * 
     * TODO: test to make sure this results in the desired behaviour.
     */
    return_set = 0;
    trap_handler(RETURN_TRAP_NUM);
    return res;
}


/*
 * execute a function definition. if the function has been called before, it means
 * we have already parsed its body and we have the nodetree of the commands inside
 * the function. in this case, we simply execute the function body directly.
 * if this is the first time we execute the function, we'll need to parse the function
 * body to create the AST (abstract source tree), a.k.a. the function body's nodetree.
 * we always keep an eye on the number of nested functions so that it doesn't execute
 * a maximumm value. we the push a call frame (to help the 'caller' builtin utility
 * do its job) which we will pop later after the function finishes execution. we also
 * set some parameters, like the positional parameters, the $# parameter and the
 * $FUNCNAME variable. we restore these things after the function finishes execution.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_function_definition(int argc, char **argv)
{
    if(!argv[0])
    {
        return 0;
    }
    struct symtab_entry_s *func = get_func(argv[0]);
    if(!func)
    {
        return 0;
    }
    /*
     * we keep the parse tree of a function stored, so that subsequent calls
     * to the same function will not need to go through the parsing process over
     * and over. if no parse tree if found, this is the first time the function
     * is called, and we do the parsing right away.
     */
    if(!func->func_body)
    {
        if(!func->val)
        {
            return 1;
        }
        char *s = func->val;
        /* parse functions that were passed to us in the environment */
        if(s[0] == '(' && s[1] == ')')
        {
            char *f = s+2;
            while(*f && isspace(*f))
            {
                f++;
            }
            if(!*f || *f == '}')
            {
                /* empty function body */
                return 1;
            }
            else
            {
                struct source_s save_src = __src;
                src->srctype  = SOURCE_FUNCTION;
                //src->srcname  = argv[0];
                src->buffer   = f;
                src->bufsize  = strlen(f);
                src->curpos   = -2;
                struct token_s *tok = tokenize(src);
                struct node_s *body = parse_function_body(tok);
                if(body)
                {
                    func->func_body = body;
                }
                func->val_type = SYM_FUNC;
                __src = save_src;
            }
        }
        else
        {
            return 1;
        }
    }
    /* check we are not exceeding the maximum function nesting level */
    int maxlevel = get_shell_varl("FUNCNEST", 0);
    if(maxlevel < 0)
    {
        maxlevel = 0;
    }
    if(maxlevel && cur_func_level >= maxlevel)
    {
        fprintf(stderr, "%s: can't execute the call to %s: maximum function nesting reached\n", 
                SHELL_NAME, argv[0]);
        return 0;
    }    
    cur_func_level++;
    //symtab_stack_push();
    /* save current positional parameters - similar to what we do in dot.c */
    char **pos = get_pos_paramsp();
    callframe_push(callframe_new(argv[0], src->srcname, src->curline));
    int  i;
    char buf[32];
    /* set param $0 (bash doesn't set $0 to the function's name) */
    struct symtab_entry_s *param = add_to_symtab("0");
    symtab_entry_setval(param, argv[0]);
    /* set arguments $1...$argc-1 */
    for(i = 1; i < argc; i++)
    {
        sprintf(buf, "%d", i);
        param = add_to_symtab(buf);
        symtab_entry_setval(param, argv[i]);
    }
    /* set param $# */
    param = add_to_symtab("#");
    sprintf(buf, "%d", argc-1);
    symtab_entry_setval(param, buf);
    /* additionally, set $FUNCNAME to the function's name (bash) */
    param = add_to_symtab("FUNCNAME");
    symtab_entry_setval(param, argv[0]);
    /*
     * reset the DEBUG trap if -o functrace (-T) is not set, and the ERR trap
     * if -o errtrace (-E) is not set. traced functions inherit both traps
     * from the calling shell (bash).
     */
    int exttrap_saved = 0;
    struct trap_item_s *debug = NULL, *err = NULL, *ret = NULL, *ext = NULL;
    if(!flag_set(func->flags, FLAG_FUNCTRACE))
    {
        if(!option_set('T'))
        {
            debug = save_trap("DEBUG" );
            ret   = save_trap("RETURN");
            ext   = save_trap("EXIT"  );
            exttrap_saved = 1;
        }
        if(!option_set('E'))
        {
            err = save_trap("ERR");
        }
    }
    do_function_body(func->func_body);
    /*
     * execute any EXIT trap set by the function, before restoring our shell's 
     * EXIT trap to its original value.
     */
    if(exttrap_saved)
    {
        trap_handler(0);
    }
    /*
     * restore saved traps. struct trap_item_s * memory is freed in the call.
     * if the struct is null, nothing happens to the trap, so the following calls
     * are safe, even NULL trap structs.
     */
    restore_trap("DEBUG" , debug);
    restore_trap("RETURN", ret  );
    restore_trap("ERR"   , err  );
    restore_trap("EXIT"  , ext  );
    /* restore pos parameters - similar to what we do in dot.c */
    if(pos)
    {
        set_pos_paramsp(pos);
        for(i = 0; pos[i]; i++)
        {
            free(pos[i]);
        }
        free(pos);
    }
    callframe_popf();
    cur_func_level--;
    return 1;
}


/*
 * search the list of special builtin utilities looking for a utility whose name matches
 * argv[0] of the passed **argv list. if found, we execute the special builtin utility,
 * passing it the **argv list as if we're executing an external command, then we return 1.
 * otherwise, we return 0 to indicate we failed in finding a special builtin utility whose
 * name matches the command we are meant to execute (that is, argv[0]).
 */
int do_special_builtin(int argc, char **argv)
{
    char   *cmd = argv[0];
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < special_builtin_count; j++)
    {
        if(special_builtins[j].namelen != cmdlen)
        {
            continue;
        }
        if(strcmp(special_builtins[j].name, cmd) == 0)
        {
            if(!flag_set(special_builtins[j].flags, BUILTIN_ENABLED))
            {
                return 0;
            }
            int (*func)(int, char **) = (int (*)(int, char **))special_builtins[j].func;
            int status = do_exec_cmd(argc, argv, NULL, func);
            set_exit_status(status, 0);
            return 1;
        }
    }
    return 0;
}


/*
 * search the list of regular builtin utilities looking for a utility whose name matches
 * argv[0] of the passed **argv list. if found, we execute the regular builtin utility,
 * passing it the **argv list as if we're executing an external command, then we return 1.
 * otherwise, we return 0 to indicate we failed in finding a regular builtin utility whose
 * name matches the command we are meant to execute (that is, argv[0]).
 */
int do_regular_builtin(int argc, char **argv)
{
    char   *cmd = argv[0];
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(strlen(regular_builtins[j].name) != cmdlen)
        {
            continue;
        }
        if(strcmp(regular_builtins[j].name, cmd) == 0)
        {
            if(!flag_set(regular_builtins[j].flags, BUILTIN_ENABLED))
            {
                return 0;
            }
            int (*func)(int, char **) = (int (*)(int, char **))regular_builtins[j].func;
            int status = do_exec_cmd(argc, argv, NULL, func);
            set_exit_status(status, 0);
            return 1;
        }
    }
    return 0;
}


static int saved_stdin    = -1;
static int saved_stdout   = -1;
static int saved_stderr   = -1;
       int do_restore_std =  1;

/*
 * if we are executing a builtin utility or a shell function, we need to save the
 * state of the standard streams so that we can restore them after the utility or
 * function finishes execution.
 */
void save_std(int fd)
{
    if(fd == 0)
    {
        fflush(stdin);
        saved_stdin = dup(0);
    }
    else if(fd == 1)
    {
        fflush(stdout);
        saved_stdout = dup(1);
    }
    else if(fd == 2)
    {
        fflush(stderr);
        saved_stderr = dup(2);
    }
}

/*
 * after a builtin utility or a shell function finishes execution, restore the
 * standard streams if there were any I/O redirections.
 */
void restore_std()
{
    if(!do_restore_std)
    {
        return;
    }
    if(saved_stdin >= 0)
    {
        fflush(stdin);
        dup2(saved_stdin, 0);
        close(saved_stdin);
    }
    if(saved_stdout >= 0)
    {
        fflush(stdout);
        dup2(saved_stdout, 1);
        close(saved_stdout);
    }
    if(saved_stderr >= 0)
    {
        fflush(stderr);
        dup2(saved_stderr, 2);
        close(saved_stderr);
    }
    saved_stdin    = -1;
    saved_stdout   = -1;
    saved_stderr   = -1;
    do_restore_std =  1;
}


/*
 * free the list of arguments (argv) after we finish executing a command.
 * we handle the special case where a file was opened via process substitution.
 * in this case, we close that file (so it won't linger around the shell
 * without being used) before freeing the memory used to store the strings.
 */
static inline void free_argv(int argc, char **argv)
{
    while(argc--)
    {
        /* free the file we opened for process substitution in redirect.c */
        if(argc && strstr(argv[argc], "/dev/fd/") == argv[argc])
        {
            char *p = argv[argc]+8, *strend = p;
            int fd = strtol(p, &strend, 10);
            if(p != strend)
            {
                close(fd);
            }
        }
        free_malloced_str(argv[argc]);
    }
    free(argv);
}


/*
 * execute a simple command. this function processes the nodetree of the parsed command,
 * performing I/O redirections and variable assignments as indicated in the command's nodetree.
 * if there's no command word, we follow ksh and zsh by inserting a predefined command name,
 * which is ':' for ksh and $NULLCMD, $READNULLCMD, 'cat' or 'more' for zsh (depending on many
 * things which you'll find detailed in zsh manpage). we simplify things by using the value of
 * $NULLCMD, or 'cat' if $NULLCMD is not set.
 * this function forks a new child process to execute an external command, while builtin utilities
 * and shell functions are executed in the same process as the shell. the steps shown in the
 * function's comments reflect the steps mentioned in the POSIX standard, under the "Command
 * Search and Execution" sub-section of the "Shell Commands" section of POSIX's vol. 3,
 * Shell & Utilities: https://pubs.opengroup.org/onlinepubs/9699919799/.
 * the forked process doesn't return (unless there was an error), while the parent process (the
 * shell) returns 1 on success and 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_simple_command(struct node_s *node, struct node_s *redirect_list, int dofork)
{
    struct io_file_s io_files[FOPEN_MAX];
    int i;
    /* first apply the given redirection list, if any */
    int total_redirects = redirect_prep(redirect_list, io_files);
    
    /* then loop through the command and its arguments */
    struct node_s *child = node->first_child;
    int argc = 0;           /* arguments count */
    int targc = 0;          /* total alloc'd arguments count */
    char **argv = NULL;
    /************************************/
    /* 1- collect command arguments     */
    /************************************/
    char *arg;
    char *s;
    int saved_noglob = option_set('f');
    
    /*
     * push a local symbol table so that any variable assignments won't affect the shell
     * proper. if the simple command we're executing is a builtin or a function, we'll
     * merge that local symbol table with the shell's global symbol table before we return.
     */
    symtab_stack_push();

    /*
     * parse the command's nodetree to obtain the list of I/O redirections, perform any
     * variable substitutions, and collect the argument list of the command.
     */
    while(child)
    {
        switch(child->type)
        {
            case NODE_IO_REDIRECT:
                /*
                 * check for the non-POSIX redirection extensions <(cmd) and >(cmd).
                 */
                if(child->first_child && child->first_child->type == NODE_IO_FILE)
                {
                    struct node_s *child2 = child->first_child;
                    s = child2->first_child->val.str;
                    if(s[0] == '(' && s[strlen(s)-1] == ')')
                    {
                        char op = (child2->val.chr == IO_FILE_LESS ) ? '<' :
                                  (child2->val.chr == IO_FILE_GREAT) ? '>' : 0;
                        if(op && (s = redirect_proc(op, s)))
                        {
                            arg = get_malloced_str(s);
                            if(check_buffer_bounds(&argc, &targc, &argv))
                            {
                                argv[argc++] = arg;
                            }
                            break;
                        }
                    }
                }
                if(redirect_prep_node(child, io_files) == -1)
                {
                    total_redirects = -1;
                }
                else
                {
                    total_redirects++;
                }
                break;
                
            case NODE_ASSIGNMENT:
                /*
                 * assignments after the command name is encountered can only take 
                 * effect if the keyword '-k' option is set. we know we haven't seen
                 * the command word if argc is 0. if -k is not set, fall through to 
                 * the default case.
                 */
                if(!argc || option_set('k'))
                {
                    s = strchr(child->val.str, '=');
                    int len = s-child->val.str;
                    char *name = malloc(len+1);
                    if(!name)
                    {
                        break;
                    }
                    strncpy(name, child->val.str, len);
                    name[len] = '\0';
                    /*
                     * support bash extended += operator. currently we only add
                     * strings, we don't support numeric addition. so 1+=2 will give
                     * you "12", not "3".
                     */
                    int add = 0;
                    if(name[len-1] == '+')
                    {
                        name[len-1] = '\0';
                        add = 1;
                        len--;
                    }
                    /* is this shell restricted? */
                    if(startup_finished && option_set('r') && is_restrict_var(name))
                    {
                        /* r-shells can't set/unset SHELL, ENV, FPATH, or PATH */
                        fprintf(stderr, "%s: restricted shells can't set %s\n", SHELL_NAME, name);
                        free(name);
                        /* POSIX says non-interactive shells exit on var assignment errors */
                        EXIT_IF_NONINTERACTIVE();
                        break;
                    }
                    else if(is_pos_param(name) || is_special_param(name))
                    {
                        fprintf(stderr, "%s: error setting/unsetting '%s' is not allowed\n", SHELL_NAME, name);
                        free(name);
                        EXIT_IF_NONINTERACTIVE();
                        break;
                    }
                    /* regular shell and normal variable. set the value */
                    char *val = word_expand_to_str(s+1);
                    struct symtab_entry_s *entry = get_local_symtab_entry(name);
                    char *old_val = NULL;
                    if(!entry)
                    {
                        if(add)
                        {
                            /* if there is a global var with that name, save its value */
                            struct symtab_entry_s *gentry = get_symtab_entry(name);
                            if(gentry && gentry->val)
                            {
                                old_val = get_malloced_str(gentry->val);
                            }
                        }
                        entry = add_to_symtab(name);
                    }
                    if(entry->val)
                    {
                        /* release the local var's value, if any */
                        if(old_val)
                        {
                            free_malloced_str(old_val);
                        }
                        /* save the local var's old value */
                        old_val = entry->val;
                        entry->val = NULL;
                    }
                    if(!flag_set(entry->flags, FLAG_READONLY))
                    {
                        if(add && old_val)
                        {
                            s = malloc(strlen(old_val)+strlen(val)+1);
                            if(s)
                            {
                                strcpy(s, old_val);
                                strcat(s, val);
                                free_malloced_str(old_val);
                                free(val);
                                entry->val = get_malloced_str(s);
                                free(s);
                            }
                        }
                        else
                        {
                            entry->val = get_malloced_str(val);
                            free(val);
                        }
                        entry->flags |= FLAG_CMD_EXPORT;
                    }
                    else
                    {
                        BACKEND_RAISE_ERROR(ASSIGNMENT_TO_READONLY, name, NULL);
                        EXIT_IF_NONINTERACTIVE();
                    }
                    free(name);
                    break;
                }
                /*
                 * WARNING: don't put any case here! we want to fall through.
                 */
                __attribute__((fallthrough));
                
            default:
                s = child->val.str;
                /*
                 * in bash and zsh, ((expr)) is equivalent to: let "expr", and bash sets the exit status
                 * to 0 if expr evaluates to non-zero, or 1 if expr evaluates to zero. in our case,
                 * we will add the "let" command name as the zeroth word, then add "expr" as the first
                 * and sole argument to `let`.
                 *
                 * $[ ... ] is a deprecated form of integer arithmetic, similar to (( ... )).
                 */
                if(!argc)
                {
                	if(strncmp(s, "((", 2) == 0 || strncmp(s, "$[", 2) == 0)
                	{
                	    /* get the index of the closing '))' or ']' */
                	    int match = 0;
                		arg = s+strlen(s)-1;
                		/* check we have a matching closing '))' or ']' */
                		if(*s == '(')
                		{
                		    arg--;
                            match = !strncmp(arg, "))", 2);
                		}
                		else
                		{
                		    match = !strncmp(arg, "]", 1);
                		}
                		/* convert `((expr))` and `$[expr]` to `let "expr"` */
                		if(match)
                		{
                		    char c = *arg;
                			(*arg) = '\0';
                			if(check_buffer_bounds(&argc, &targc, &argv))
                			{
                				argv[argc++] = get_malloced_str("let");
                			}
                			if(check_buffer_bounds(&argc, &targc, &argv))
                			{
                				argv[argc++] = get_malloced_str(s+2);
                			}
                			(*arg) = c;
                			break;
                		}
                	}
                	/*
                	 * in zsh, if the first word in the command is 'noglob', filename globbing is not performed.
                	 * we mimic this behavior by temporarily setting the noglob '-f' option, which we'll reset
                	 * later after we finish parsing the command's arguments.
                	 */
                	else if(strcmp(s, "noglob") == 0 && !option_set('P'))
                	{
                		set_option('f', 1);
                		// t2 = t2->next;
                		continue;
                	}
                }
                /* go POSIX style on the word */
                struct word_s *w = word_expand(s);
                if(!w)
                {
                    break;
                }
                /*
                if(!argc && (strncmp(s, "$((", 3) == 0))
                {
                    free_all_words(w);
                    break;
                }
                */
                /* now parse the words */
                struct word_s *w2 = w;
                /* we will get NULL if pathname expansion fails */
                if(!w)
                {
                    free_argv(argc, argv);
                    free_all_words(w);
                    set_option('f', saved_noglob);
                    return 0;
                }
                /* add the words to the arguments list */
                while(w2)
                {
                    if(check_buffer_bounds(&argc, &targc, &argv))
                    {
                        arg = get_malloced_str(w2->data);
                        argv[argc++] = arg;
                    }
                    w2 = w2->next;
                }
                free_all_words(w);
        }
        child = child->next_sibling;
    }
    /* even if arc == 0, we need to alloc memory for argv */
    if(check_buffer_bounds(&argc, &targc, &argv))
    {
        /* NULL-terminate the array */
        argv[argc] = NULL;
    }
    set_option('f', saved_noglob);
    
    /*
     * interactive shells check for a directory passed as the command word (bash).
     * similar to setting tcsh's 'implicitcd' variable.
     */
    if(option_set('i') && argc == 1 && optionx_set(OPTION_AUTO_CD))
    {
        struct stat st;
        if((stat(argv[0], &st) == 0) && S_ISDIR(st.st_mode))
        {
            argc++;
            if(check_buffer_bounds(&argc, &targc, &argv))
            {
                for(i = argc; i > 0; i--)
                {
                    argv[i] = argv[i-1];
                }
                argv[0] = get_malloced_str("cd");
            }
        }
    }
    
    /*
     * if we have redirections with no command name, zsh inserts the name specified in the
     * $NULLCMD shell variable as the command name (it also uses $READNULLCMD for input
     * redirections, but we won't use this here). as with zsh, the default $NULLCMD is cat.
     * zsh has other options when dealing with redirections that have empty commands names,
     * which can be found under the "Redirections with no command" section of zsh manpage.
     */
    if(!argc && total_redirects > 1)
    {
        char *nullcmd = get_shell_varp("NULLCMD", NULL);
        if(nullcmd && *nullcmd)
        {
            s = word_expand_to_str(nullcmd);
            if(s && check_buffer_bounds(&argc, &targc, &argv))
            {
                argv[0] = get_malloced_str(s);
                if(argv[0])
                {
                    argc++;
                }
                free(s);
            }
        }
        else if(check_buffer_bounds(&argc, &targc, &argv))
        {
            argv[0] = get_malloced_str("cat");
            if(argv[0])
            {
                argc++;
            }
        }
    }
    
    //int  builtin =  is_builtin(argv[0]);
    /*
     * this call returns 1 if argv[0] is a special builtin utility, -1 if argv[0]
     * is a regular builtin utility, and 0 otherwise.
     */
    int builtin  = is_enabled_builtin(argv[0]);
    /* this call returns 1 if argv[0] is a defined shell function, 0 otherwise */
    int function = is_function(argv[0]);
    struct symtab_entry_s *entry;

    if(total_redirects == -1 /* redir_fail */)
    {
        /*
         * POSIX says non-interactive shell shall exit on redirection errors with
         * special builtins, may exit with compound commands and functions, and shall
         * not exit with other (non-special-builtin) utilities. interactive shell shall
         * not exit in any condition.
         */
        if(!argc || builtin || function)
        {
            EXIT_IF_NONINTERACTIVE();
        }
        total_redirects = 0;
    }

    if(argc != 0)
    {
        /* do not fork for builtins and function calls */
        if(builtin || function)
        {
            dofork = 0;
        }
        //else symtab_stack_push();
        /*
         * don't fork in this case, as we will just resume the given job in the
         * fg/bg, according to the command given (see below after the fork).
         */
        else if(argc == 1 && *argv[0] == '%')
        {
            dofork = 0;
        }
        /* reset the request to exit flag */
        if(strcmp(argv[0], "exit"))
        {
            tried_exit = 0;
        }
    }
    else
    {
        dofork = 0;
    }
    
    if(option_set('x'))
    {
        print_prompt4();
        for(i = 0; i < argc; i++)
        {
            fprintf(stderr, "%s ", argv[i]);
        }
        printf("\n");
    }
    
    /* set $LINENO if we're not reading from the commandline */
    if(src->srctype != SOURCE_STDIN && src->srctype != SOURCE_EVAL)
    {
        entry = add_to_symtab("LINENO");
        char buf[32];
        sprintf(buf, "%d", node->lineno);
        symtab_entry_setval(entry, buf);
    }

    if(!executing_trap)
    {
        if(node->type == NODE_COMMAND && node->val_type == VAL_STR && node->val.str)
        {
            entry = add_to_symtab("COMMAND");       /* similar to $BASH_COMMAND */
            symtab_entry_setval(entry, node->val.str);
        }
        else
        {
            s = list_to_str(argv, 0);
            if(s && *s)
            {
                entry = add_to_symtab("COMMAND");       /* similar to $BASH_COMMAND */
                symtab_entry_setval(entry, s);
            }
            if(s)
            {
                free(s);
            }
        }
    }
    
    /* expand $PS0 and print the result (bash) */
    char *pr = __evaluate_prompt(get_shell_varp("PS0", NULL));
    if(pr)
    {
        fprintf(stderr, "%s", pr);
        free(pr);
    }
    
    /*
     * in tcsh, special alias jobcmd is run before running commands and when jobs
     * change state, or a job is brought to the foreground.
     */
    run_alias_cmd("jobcmd");
    
    /* in zsh, hook function preexec is run before running each command (similar to a tcsh's special alias) */
    run_alias_cmd("preexec");

    trap_handler(DEBUG_TRAP_NUM);    
    
    pid_t child_pid = 0;
    if(dofork && (child_pid = fork_child()) == 0)
    {
        if(option_set('m'))
        {
            /*
             * if we are running from a subshell, don't reset our PGID, or else we'll receive a SIGTTOU if
             * we needed to output to the terminal, and SIGTTIN if we needed to read from it. this is because
             * our parent (the subshell) has the same PGID as its parent (the original shell), but we won't
             * share that PGID with them if we changed it.
             */
            if(subshell_level == 0)
            {
                setpgid(0, 0);
                tcsetpgrp(0, child_pid);
            }
        }
        reset_nonignored_traps();
        do_export_vars();
    }
    
   
// redir:
    if(child_pid == 0)
    {
#if 0
        /* only restore tty to canonical mode if we are reading from it */
        if(read_stdin && /* isatty(0) && */ getpgrp() == tcgetpgrp(0))
        {
            term_canon(1);
        }
#endif
        
        /*
         * we need to handle the special case of coproc, as this command opens
         * a pipe between the shell and the new coprocess before local redirections
         * are performed. we let coproc() do everything by passing it the list of
         * I/O redirections.
         */
        if(argc && strcmp(argv[0], "coproc") == 0 && 
            flag_set(special_builtins[REGULAR_BUILTIN_COPROC].flags, BUILTIN_ENABLED))
        {
            int res = coproc(argc, argv, io_files);
            set_exit_status(res, 0);
            //while(argc--) free_malloced_str(argv[argc]);
            free_argv(argc, argv);
            return !res;
        }
        
        /*
         * for all builtins, exept 'exec', we'll save (and later restore) the standard
         * input/output/error streams.
         */
        int savestd = !argc || (strcmp(argv[0], "exec") != 0);
        
        /* perform I/O redirection, if any */
        if(total_redirects && !__redirect_do(io_files, savestd))
        {
            /* I/O redirection failure */
            free_argv(argc, argv);
            return 0;
        }

        /*
         * return if argc == 0 (if the command consisted of redirections and/or
         * variable assignments, they would have been executed in the above code).
         */
        if(argc == 0)
        {
            free_argv(argc, argv);
            if(savestd && total_redirects)
            {
                redirect_restore();
            }
            MERGE_GLOBAL_SYMTAB();
            return 1;
        }
        
        /* only restore tty to canonical mode if we are reading from it */
        if(isatty(0))
        {
            term_canon(1);
        }

        /*
         * bash/tcsh have a useful non-POSIX extension where '%n' equals 'fg %n'
         * and '%n &' equals bg %n'.
         */
        if(argc == 1 && *argv[0] == '%')
        {
            /* 
             * we can't tell from the AST if the original command contained & in it.
             * we have to check the original command's string in the parent node.
             */
            if(node->val_type == VAL_STR)
            {
                int res = 1;
                s = node->val.str;
                if(s[strlen(s)-1] == '&')
                {
                    res = bg(2, (char *[]){ "bg", argv[0], NULL });
                }
                else
                {
                    res = fg(2, (char *[]){ "fg", argv[0], NULL });
                }
                //while(argc--) free_malloced_str(argv[argc]);
                free_argv(argc, argv);
                if(savestd && total_redirects)
                {
                    redirect_restore();
                }
                if(exit_status)
                {
                    EXIT_IF_NONINTERACTIVE();
                }
                return res;
            }
        }
        
        /* POSIX Command Search and Execution Algorithm:      */
        search_and_exec(argc, argv, NULL, SEARCH_AND_EXEC_DOFUNC|SEARCH_AND_EXEC_MERGE_GLOBAL);
        if(dofork)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_EXEC, argv[0], strerror(errno));
            if(errno == ENOEXEC)
            {
                exit(EXIT_ERROR_NOEXEC);
            }
            else if(errno == ENOENT)
            {
                exit(EXIT_ERROR_NOENT);
            }
            else
            {
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            int res = 1;
            i = 0;
            if((strcmp(argv[0], "break"   ) == 0) ||
               (strcmp(argv[0], "continue") == 0) ||
               (strcmp(argv[0], "return"  ) == 0))
            {
                if(exit_status == 0)
                {
                    /*
                     * we force our caller to break any loops by returning
                     * a zero (error) status.
                     */
                    res = 0;
                }
                i = 1;
            }
            free_argv(argc, argv);
            if(savestd && total_redirects)
            {
                redirect_restore();
            }
            MERGE_GLOBAL_SYMTAB();
            /*
             * non-interactive shells exit if a special builtin returned non-zero
             * or error status (except if it is break, continue, or return).
             */
            if(exit_status && builtin > 0 && i == 0)
            {
                EXIT_IF_NONINTERACTIVE();
            }
            return res;
        }
    }
    /* ... and parent countinues over here ...    */

    /*
     * NOTE: we re-set the process group id here (and above in the child process) to make
     *       sure it gets set whether the parent or child runs first (i.e. avoid race condition).
     */
    if(option_set('m'))
    {
        /*
         * see the comments above to understand why we perform this check for a subshell first.
         */
        if(subshell_level == 0)
        {
            setpgid(child_pid, 0);
            /* tell the terminal who's the foreground pgid now */
            tcsetpgrp(0, child_pid);
        }
    }
    
    block_traps();
    int status = wait_on_child(child_pid, node, NULL);
    unblock_traps();

    /* reset the terminal's foreground pgid */
    if(option_set('m'))
    {
        tcsetpgrp(0, tty_pid);
    }
    PRINT_EXIT_STATUS(status);

#if 0
    /* only restore tty to canonical mode if the child process read from it */
    if(read_stdin /* isatty(0) */)
    {
        term_canon(0);
        update_row_col();
    }
#endif


    /* if we forked, we didn't hash the utility's path in our
     * hashtable. if so, do it now.
     */
    int cmdfound = WIFEXITED(status) && WEXITSTATUS(status) != 126 && WEXITSTATUS(status) != 127;
    if(option_set('h') && cmdfound && dofork && argc)
    {
        if(!builtin && !function && !strchr(argv[0], '/'))
        {
            char *path = get_hashed_path(argv[0]);
            if(!path)
            {
                path = search_path(argv[0], NULL, 1);
                if(path)
                {
                    hash_utility(argv[0], path);
                    free_malloced_str(path);
                }
            }
        }
    }

    struct symtab_s *symtab = symtab_stack_pop();
    free(symtab);

    /* in tcsh, special alias postcmd is run after running each command */
    run_alias_cmd("postcmd");
    
    /* check winsize and update $LINES and $COLUMNS (bash) after running external cmds */
    if(optionx_set(OPTION_CHECK_WINSIZE))
    {
        get_screen_size();
    }
    
    set_underscore_val(argv[argc-1], 0);    /* last argument to previous command */
    free_argv(argc, argv);
    return 1;
}


/*
 * execute a simple command, compound command (loops and conditionals), shell function,
 * or timed command (one that is preceded by the 'time' keyword) by calling the appropriate
 * delegate function to handle each command.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_command(struct node_s *node, struct node_s *redirect_list, int fork)
{
    if(!node)
    {
        return 0;
    }
    else if(node->type == NODE_COMMAND)
    {
        int res = do_simple_command(node, redirect_list, fork);
        return res;
    }
    else if(node->type == NODE_FUNCTION)
    {
        return do_simple_command(node, redirect_list, fork);
    }
    else if(node->type == NODE_TIME)
    {
        return __time(node->first_child);
    }
    return do_compound_command(node, redirect_list);
}


/*
 * execute a translation unit, one command at a time.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_translation_unit(struct node_s *node)
{
    if(!node)
    {
        return 0;
    }
    if(read_stdin)
    {
        term_canon(1);
    }
    struct node_s *child = node;
    if(node->type == NODE_PROGRAM)
    {
        child = child->first_child;
    }
    while(child)
    {
        if(!do_complete_command(child))
        {
            /* we got return stmt */
            if(return_set)
            {
                return_set = 0;
                /*
                 * we should return from dot files AND functions. calling return outside any
                 * function/script should cause the shell to exit.
                 */
                if(src->srctype == SOURCE_STDIN)
                {
                    exit_gracefully(exit_status, NULL);
                }
                break;
            }
        }
        fflush(stdout);
        fflush(stderr);
        /*
         * POSIX does not specify the -t (or onecmd) option, as it says
         * it is mainly used with here-documents. this flag causes the
         * shell to read and execute only one command before exiting.
         * it is not clear what exactly constitutes 'one command'.
         * here, we just execute the first node tree (which might be the
         * whole program) and exit.
         * TODO: fix this behaviour.
         */
        if(option_set('t'))
        {
            exit_gracefully(exit_status, NULL);
        }
        child = child->next_sibling;
    }
    fflush(stdout);
    fflush(stderr);
    SIGINT_received = 0;
    if(read_stdin)
    {
        term_canon(0);
        update_row_col();
    }
    return 0;
}
