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

/* for usleep(), but also _POSIX_C_SOURCE shouldn't be >= 200809L */
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

/*
 * if the shell is waiting for a foreground job, this field stores the child
 * process's pid.
 */
pid_t waiting_pid = 0;

/* if set, we're executing the test clause of a loop or conditional */
int in_test_clause = 0;

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
    if(interactive_shell &&                           \
       optionx_set(OPTION_PRINT_EXIT_VALUE) &&      \
       WIFEXITED((status)) && WEXITSTATUS((status)))\
    {                                               \
        fprintf(stderr, "Exit %d\n",                \
                WEXITSTATUS(status));               \
    }                                               \
} while(0)


/* defined below */
char *get_cmdstr(struct node_s *cmd);


/*
 * try to fork a child process. if failed, try to a maximum number of retries, then return.
 * if successful, the function returns the pid of the new child process (in the parent), or
 * zero (in the child), or a negative value in case of error.
 */
pid_t fork_child(void)
{
    int tries = 5;
    useconds_t usecs = 1;
    pid_t pid;
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
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
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    return pid;
}


/*
 * wait on the child process with the given pid until it changes status.
 * if the struct job_s *job is passed, wait for all processes in the job to finish,
 * then update the job's status. if the command is stopped or signalled and no job
 * has been added for it, use the *cmd nodetree to re-construct the command line
 * and add it as a background job.
 * the return value is the exit status of the waited-for child process, or -1 on error.
 */
int wait_on_child(pid_t pid, struct node_s *cmd, struct job_s *job)
{
    /*
     * if we're running in a subshell, the reset_nonignored_traps() function call
     * we did at the beginning of our subshell's life would have screwed our
     * SIGCHLD function handling.. this normally isn't a problem, except when we
     * want to fork another process to execute some command, in which case we need
     * to restore our SIGCHLD handler so that we can carry on with our lives.
     */
    set_signal_handler(SIGCHLD , SIGCHLD_handler);

    /* create a new, empty sigset so that we'll wake up with any signal */
    int status = 0;
    sigset_t sigset;
    sigemptyset(&sigset);

    waiting_pid = pid;
    
_wait:
    /* 
     * as long as we don't receive SIGCHLD with the exit status of pid,
     * suspend this process.
     */
    while((status = rip_dead(pid)) < 0)
    {
        /* stop waiting if we've receive SIGINT */
        if(signal_received == SIGINT)
        {
            waiting_pid = 0;
            return 128;
        }
        
        /* keep waiting */
        sigsuspend(&sigset);
    }

    /* collect the status. if stopped, add as background job */
    if(option_set('m') && (WIFSTOPPED(status) || WIFSIGNALED(status)))
    {
        /* command doesn't have a job yet. create a new job */
        if(!job)
        {
            /* TODO: use correct command str */
            char *cmdstr = get_cmdstr(cmd);
            job = add_job(getpgid(pid), (pid_t[]){pid}, 1, cmdstr, 1);
            if(cmdstr)
            {
                free(cmdstr);
            }
            set_pid_exit_status(job, pid, status);
            set_cur_job(job);
            notice_termination(pid, status);
        }
    }
    else
    {
        set_exit_status(status);
        
        /* wait on every process in the job to finish execution */
        if(job)
        {
            set_pid_exit_status(job, pid, status);
            //if(pid == job->pgid) set_job_exit_status(job, status);
            set_job_exit_status(job, pid, status);
            
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
    
    /* execute any pending traps */
    waiting_pid = 0;
    do_pending_traps();

    /* return the exit status */
    return status;
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
    if(set_env)
    {
        setenv("_", val, 1);
    }
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
        char *sh = get_alias_val(s);
        if(sh && sh != s)
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
        wait_on_child(pid, NULL, NULL);
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
    /*
    int i = 0;      //
    while(i <= argc) //
    {   //
        i++;    //
    }   //
    for(i = 0; i < 1000; i++) printf("*");  //
    printf("\n"); //
    fflush(stdout); //
    fflush(stderr); //
    */

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
 * execute a list, which can be asynchronous (background) or sequential (foreground).
 * for asynchronous lists, we fork a child process and let it handle the list. for
 * sequential lists, we call other functions to handle the list, and we wait for the
 * list to finish execution.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_list(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }

    if(node->type != NODE_LIST && node->type != NODE_TERM)
    {
        return do_and_or(src, node, redirect_list, 1);
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

    /* run a background (asynchronous) list */
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
            struct job_s *job;
            char *cmdstr = get_cmdstr(node);
            /*
             * add as a new job if job control is on, or set $! if job control is off.
             * we add the job giving it the tty's pgid, then reset that to the
             * job's pgid if the '-m' option is set (see code below).
             */
            job = add_job(tty_pid, (pid_t[]){pid}, 1, cmdstr, 1);

            /*
             * if job control is on, set the current job, or complain if the
             * job couldn't be added.
             */
            if(option_set('m'))
            {
                setpgid(pid, pid);
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
                    job->pgid = pid;
                    set_cur_job(job);
                    fprintf(stderr, "[%d] %u\n", job->job_num, pid);
                }
            }
            set_internal_exit_status(0);
            /*
             * give the child process a headstart, in case the scheduler decided to run us first.
             * we wouldn't need to do this if we used vfork() instead of fork(), but the former has
             * a lot of limitations and things we need to worry about if we're using it. still, it
             * would be worth our while using vfork() instead of fork().
             * TODO: use vfork() in this function and in do_complete_command() instead of fork().
             */
            sigset_t sigset;
            sigemptyset(&sigset);
            sigsuspend(&sigset);
            //usleep(1);
            if(cmdstr)
            {
                free(cmdstr);
            }
            return 1;
        }
        else
        {
            /* init our subshell environment */
            init_subshell();

            /* execute the AND-OR list in the background */
            int res = do_and_or(src, cmd, redirects, 0);
            if(!res)
            {
                exit(exit_status);
            }

            /* execute the rest of the AND-OR list */
            if(cmd->next_sibling)
            {
                do_list(src, cmd->next_sibling, redirects);
            }
            exit(exit_status);
        }
    }

    /* run a foreground (sequential) list */
    int res = do_and_or(src, cmd, redirects, 1);
    if(!res)
    {
        return 0;
    }

    /* break or continue encountered inside the list */
    if(cur_loop_level && (req_break || req_continue))
    {
        return res;
    }
    
    if(cmd->next_sibling)
    {
        res = do_list(src, cmd->next_sibling, redirects);
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
int do_and_or(struct source_s *src, struct node_s *node, struct node_s *redirect_list, int fg)
{
    int res = 0;
    if(!node)
    {
        return 0;
    }

    if(node->type != NODE_ANDOR)
    {
        int is_bang = (node->type == NODE_BANG);
        res = do_pipeline(src, node, redirect_list, fg);
        /* 
         * exit on failure? only applicable to commands that are:
         * - having a non-zero exit status
         * - not following while or until
         * - not part of an if test clause
         * - not preceded by && or ||, except for the last command
         *   in the AND-OR list
         * - not part of a pipeline, except for the last command
         *   in the pipeline
         * - don't have their exit status inverted with '!'
         * 
         * NOTE: see also the loop below.
         */
        if(exit_status && !is_bang && option_set('e') && !in_test_clause)
        {
            trap_handler(ERR_TRAP_NUM);
            exit_gracefully(EXIT_FAILURE, NULL);
        }
        return res;
    }
    node = node->first_child;
    
    struct node_s *cmd = node;

    /* update the options string */
    symtab_save_options();
    
    while(cmd)
    {
        int is_bang = (node->type == NODE_BANG);
        res = do_pipeline(src, cmd, redirect_list, fg);
        if(res == 0)
        {
            break;
        }

        if(exit_status && !is_bang && !node->next_sibling &&
            option_set('e') && !in_test_clause)
        {
            trap_handler(ERR_TRAP_NUM);
            exit_gracefully(EXIT_FAILURE, NULL);
        }

        /* break or continue encountered inside the list */
        if(cur_loop_level && (req_break || req_continue))
        {
            break;
        }

        /* get the next item in the AND-OR list */
        cmd = NULL;
        while((node = node->next_sibling))
        {
            /* determine whether to execute the next command/pipeline or skip it */
            if((!exit_status && node->type == NODE_AND_IF) ||
               (exit_status && node->type != NODE_AND_IF))
            {
                cmd = node->first_child;
                break;
            }
            /*
             * we have one of the following two cases:
             * - previous command succeeded with a following || clause, or
             * - previous command failed with a following && clause.
             * in both cases, we skip the current clause and check the next one.
             */
        }
    }
    
    return res;
}


#if 0
/*
 * set the exit status after executing a pipeline.
 */
void pipeline_set_exit_status(int status, int is_bang)
{
    /* invert the result if the pipeline has a bang */
    if(is_bang)
    {
        set_internal_exit_status(!status);
    }

    /* exit on failure? */
    if(!is_bang && status)
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
}
#endif


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
int do_pipeline(struct source_s *src, struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node)
    {
        return 0;
    }

    int is_bang = 0;
    int res = 0;
    if(node->type == NODE_BANG)
    {
        is_bang = 1;
        node = node->first_child;
    }

    if(node->type != NODE_PIPE)
    {
        res = do_command(src, node, redirect_list);
        if(res && is_bang)
        {
            /* invert the result if the pipeline has a bang */
            set_internal_exit_status(!exit_status);
        }
        return res;
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
#if 0
            /* tell the terminal who's the foreground pgid now */
            if(option_set('m'))
            {
                setpgid(0, 0);
                if(fg)
                {
                    tcsetpgrp(0, pid);
                }
            }
            else
#endif
            if(!option_set('m'))
            {
                set_signal_handler(SIGINT , SIG_IGN);
                set_signal_handler(SIGQUIT, SIG_IGN);
            }

            /* 2nd command component of command line */
            close(0);   /* stdin */
            dup2(filedes[0], 0);
            close(filedes[0]);
            close(filedes[1]);

            /* standard input now comes from pipe */
            do_command(src, cmd, redirect_list);
            exit(exit_status);
        }
        else if(pid < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
    }

#if 0
    /* set fg process group id if job control is enabled */
    if(option_set('m'))
    {
        setpgid(pid, pid);
        if(fg)
        {
            if(pid != tty_pid)
            {
                tcsetpgrp(0, pid);
            }
        }
    }
#endif

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
#if 0
            if(option_set('m'))
            {
                setpgid(0, pid);
                if(fg)
                {
                    tcsetpgrp(0, pid);
                }
            }
            else
            {
                set_signal_handler(SIGINT , SIG_IGN);
                set_signal_handler(SIGQUIT, SIG_IGN);
            }
#endif

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
            do_command(src, cmd, redirect_list);
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
    struct job_s *job = NULL;

    /*
     * add as a new job if job control is on, or set $! if job control is off.
     * we add the job giving it the tty's pgid, then reset that to the
     * job's pgid if the '-m' option is set (see code below).
     */
    job = add_job(tty_pid, all_pids, count, cmdstr, !fg);

    /*
     * if job control is on, set the current job.
     */
    if(option_set('m'))
    {
        if(job)
        {
            job->pgid = pid;
            set_cur_job(job);
        }
    }

    /* free temp memory */
    if(cmdstr)
    {
        free(cmdstr);
    }

    /* run the last command in this process if extended option 'lastpipe' is set (bash) */
    if(lastpipe)
    {
        do_command(src, cmd, redirect_list);
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
        res = wait_on_child(pid, node, job);
#if 0
        /* reset the terminal's foreground pgid */
        if(option_set('m'))
        {
            tcsetpgrp(0, tty_pid);
        }
#endif
        /* invert the result if the pipeline has a bang */
        if(is_bang)
        {
            set_internal_exit_status(!res);
        }
        PRINT_EXIT_STATUS(exit_status);
        return !res;
    }
    else
    {
        fprintf(stderr, "[%d] %u %s\n", job->job_num, pid, job->commandstr);
        /* invert the result if the pipeline has a bang */
        set_internal_exit_status(is_bang ? 1 : 0);
        return 1;
    }
}


/*
 * execute a compound list, which forms the body of most compound commands,
 * such as loops and conditionals. a compound list consists of one or more terms.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_compound_list(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    if(node->type != NODE_LIST)
    {
        return do_list(src, node, redirect_list);
    }
    node = node->first_child;
    int res = 0;
    while(node)
    {
        /* execute the first term (or list) */
        res = do_list(src, node, redirect_list);
        
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
        
        /* return encountered inside a loop's do-done group */
        if(return_set)
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
int do_subshell(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
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
        return 1;
    }

    /* init our subshell environment */
    init_subshell();

    /* perform I/O redirections */
    if(redirect_list)
    {
        if(!redirect_prep_and_do(redirect_list))
        {
            return 0;
        }
    }

    /* do the actual commands */
    do_compound_list(src, subshell, NULL);
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
int do_do_group(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    /*
     * this will take care of executing ERR trap for while, until, 
     * select and for loops.
     */
    int res = do_compound_list(src, node, redirect_list);
    ERR_TRAP_OR_EXIT();
    return res;
}


/*
 * execute a brace group (commands enclosed in curly braces '{' and '}');
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_brace_group(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    int res = do_compound_list(src, node, redirect_list);
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
int do_compound_command(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    switch(node->type)
    {
        case NODE_SUBSHELL: return do_subshell   (src, node, redirect_list);
        case NODE_FOR     : return do_for_loop   (src, node, redirect_list);
        case NODE_CASE    : return do_case_clause(src, node, redirect_list);
        case NODE_IF      : return do_if_clause  (src, node, redirect_list);
        case NODE_WHILE   : return do_while_loop (src, node, redirect_list);
        case NODE_UNTIL   : return do_until_loop (src, node, redirect_list);
        case NODE_SELECT  : return do_select_loop(src, node, redirect_list);
        case NODE_TERM    :
        case NODE_LIST    : return do_brace_group(src, node, redirect_list);
        default           : return 0;
    }
}


/*
 * execute a function definition. we simply add the function definition to the
 * global functions table, then we set the exit status to zero.
 * 
 * the function's AST has the following structure:
 * 
 * NODE_FUNCTION (root node, val field contains function name as string)
 * |--> NODE_LIST (function body as AST)
 * +--> NODE_VAR (function body as string)
 * 
 */
int do_function_definition(struct node_s *node)
{
    /* get the function name */
    char *func_name = node->val.str;
    if(!func_name)
    {
        set_internal_exit_status(1);
        return 0;
    }
    
    /* add the function name to the functions table */
    struct symtab_entry_s *func = add_func(func_name);
    if(!func)
    {
        set_internal_exit_status(1);
        return 0;
    }
    
    /* free the old function body, if any */
    if(func->func_body)
    {
        free_node_tree(func->func_body);
        func->func_body = NULL;
    }
    
    /* get the function body */
    struct node_s *func_body = node->first_child;
    if(!func_body)
    {
        set_internal_exit_status(1);
        return 0;
    }
    func->func_body = func_body;
    func->val_type = SYM_FUNC;
    
    /* 
     * detach the function body from the AST so it won't be freed when we return
     * to parse_and_execute().
     */
    node->first_child = NULL;
    
    /* get the function string, if any */
    struct node_s *func_str = func_body->next_sibling;
    if(func_str && func_str->val_type == VAL_STR)
    {
        symtab_entry_setval(func, func_str->val.str);
    }
    
    set_internal_exit_status(0);
    dump_node_tree(func->func_body, 1);
    return 1;
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
int do_function_body(struct source_s *src, int argc, char **argv)
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
                struct source_s src2;
                src2.srctype  = SOURCE_FUNCTION;
                //src->srcname  = argv[0];
                src2.buffer   = f;
                src2.bufsize  = strlen(f);
                src2.curpos   = INIT_SRC_POS;
                
                struct token_s *tok = tokenize(&src2);
                struct node_s *body = parse_function_body(tok);
                if(body)
                {
                    func->func_body = body;
                }
                func->val_type = SYM_FUNC;
            }
        }
        else
        {
            return 1;
        }
    }

    struct node_s *body = func->func_body;
    if(!body || !body->first_child)
    {
        fprintf(stderr, "%s: function %s has an empty body\n", SHELL_NAME, argv[0]);
        return 1;
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
    callframe_push(callframe_new(argv[0], src->srcname, src->curline));
    
    int  i;
    char buf[32];
    struct symtab_s *st = get_local_symtab();
    /* set param $0 (bash doesn't set $0 to the function's name) */
    struct symtab_entry_s *param = add_to_any_symtab("0", st);
    if(param)
    {
        symtab_entry_setval(param, argv[0]);
        param->flags = FLAG_LOCAL|FLAG_READONLY;
    }
    
    /* set arguments $1...$argc-1 */
    for(i = 1; i < argc; i++)
    {
        sprintf(buf, "%d", i);
        param = add_to_any_symtab(buf, st);
        if(param)
        {
            symtab_entry_setval(param, argv[i]);
            param->flags = FLAG_LOCAL|FLAG_READONLY;
        }
    }
    
    /*
     * overshadow the rest of the parameters, if any, so that the user cannot
     * access them behind our back.
     */
    param = get_symtab_entry("#");
    if(param && param->val)
    {
        int old_count = atoi(param->val);
        if(old_count > 0)
        {
            for( ; i <= old_count; i++)
            {
                sprintf(buf, "%d", i);
                param = add_to_any_symtab(buf, st);
                if(param)
                {
                    param->flags = FLAG_LOCAL|FLAG_READONLY;
                }
            }
        }
    }
    
    /* set our new param $# */
    param = add_to_any_symtab("#", st);
    if(param)
    {
        sprintf(buf, "%d", argc-1);
        symtab_entry_setval(param, buf);
        param->flags = FLAG_LOCAL|FLAG_READONLY;
    }
    
    /* additionally, set $FUNCNAME to the function's name (bash) */
    param = add_to_any_symtab("FUNCNAME", st);
    if(param)
    {
        symtab_entry_setval(param, argv[0]);
        param->flags = FLAG_LOCAL|FLAG_READONLY;
    }
    
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

    /* execute the function */
    int res = do_compound_command(src, body, NULL);

    /*
     * clear the return flag so that we won't cause the parent 
     * shell to exit as well.
     * 
     * TODO: test to make sure this results in the desired behaviour.
     */
    return_set = 0;
    trap_handler(RETURN_TRAP_NUM);
    
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
    /*
    if(pos)
    {
        set_pos_paramsp(pos);
        for(i = 0; pos[i]; i++)
        {
            free(pos[i]);
        }
        free(pos);
    }
    */
    callframe_popf();
    cur_func_level--;
    return res;
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
            set_internal_exit_status(status);
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
            set_internal_exit_status(status);
            return 1;
        }
    }
    return 0;
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
 * 
 * this function forks a new child process to execute an external command, while builtin utilities
 * and shell functions are executed in the same process as the shell. the steps shown in the
 * function's comments reflect the steps mentioned in the POSIX standard, under the "Command
 * Search and Execution" sub-section of the "Shell Commands" section of POSIX's vol. 3,
 * Shell & Utilities: https://pubs.opengroup.org/onlinepubs/9699919799/.
 * 
 * the forked process doesn't return (unless there was an error), while the parent process (the
 * shell) returns 1 on success and 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_simple_command(struct source_s *src, struct node_s *node)
{
    struct io_file_s io_files[FOPEN_MAX];
    int i, dofork;

    /* first apply the given redirection list, if any */
    int total_redirects = init_redirect_list(io_files);
    
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
     * proper.. if the simple command we're executing is a builtin or a function, we'll
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
                if(!redirect_prep_node(child, io_files))
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
                    if(is_name(name))
                    {
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

                        /* can't set positional and special params that way */
                        if(is_pos_param(name) || is_special_param(name))
                        {
                            fprintf(stderr, "%s: error setting/unsetting '%s' is not allowed\n", SHELL_NAME, name);
                            free(name);
                            EXIT_IF_NONINTERACTIVE();
                            break;
                        }

                        /* can't set readonly variables */
                        struct symtab_entry_s *entry = get_symtab_entry(name);
                        if(entry && flag_set(entry->flags, FLAG_READONLY))
                        {
                            fprintf(stderr, "%s: cannot set '%s': readonly variable\n", SHELL_NAME, name);
                            free(name);
                            EXIT_IF_NONINTERACTIVE();
                            break;
                        }

                        /* get the variable's old value */
                        char *old_val = NULL;
                        if(add && entry)
                        {
                            old_val = get_malloced_str(entry->val);
                        }

                        /* word-expand the value */
                        char *val = word_expand_to_str(s+1);

                        /* add local entry */
                        entry = add_to_symtab(name);

                        /* regular shell and normal variable. set the value */
                        if(add && old_val)
                        {
                            s = malloc(strlen(old_val)+strlen(val)+1);
                            if(s)
                            {
                                strcpy(s, old_val);
                                strcat(s, val);
                                entry->val = get_malloced_str(s);
                                free(s);
                            }
                            free_malloced_str(old_val);
                        }
                        else
                        {
                            entry->val = get_malloced_str(val);
                            if(old_val)
                            {
                                free_malloced_str(old_val);
                            }
                        }
                        entry->flags |= FLAG_CMD_EXPORT;

                        if(val)
                        {
                            free(val);
                        }
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
                        /* update the options string */
                        symtab_save_options();
                        continue;
                    }
                }
                /* go POSIX style on the word */
                struct word_s *w = word_expand(s,
                                    FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES|FLAG_FIELD_SPLITTING);
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
                    set_option('f', saved_noglob);
                    /* update the options string */
                    symtab_save_options();
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
    /* update the options string */
    symtab_save_options();
    
    /*
     * interactive shells check for a directory passed as the command word (bash).
     * similar to setting tcsh's 'implicitcd' variable.
     */
    if(interactive_shell && argc == 1 && optionx_set(OPTION_AUTO_CD))
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

    if(total_redirects == -1)
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
        /* I/O redirection failure */
        free_argv(argc, argv);
        free_symtab(symtab_stack_pop());
        return 0;
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
        /* fork for external commands */
        else
        {
            dofork = 1;
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
            s = list_to_str(argv);
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
    char *pr = evaluate_prompt(get_shell_varp("PS0", NULL));
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

    /*
     * return if argc == 0 (if the command consisted of redirections and/or
     * variable assignments, they would have been executed in the above code).
     */
    if(argc == 0)
    {
        free_argv(argc, argv);
        MERGE_GLOBAL_SYMTAB();
        return 1;
    }
    
    pid_t child_pid = 0;
    if(dofork && (child_pid = fork_child()) == 0)
    {
#if 0
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
#endif
        reset_nonignored_traps();

        /* export the variables marked for export */
        do_export_vars(EXPORT_VARS_EXPORTED_ONLY);
    }
    

    if(child_pid == 0)
    {
        /*
         * we need to handle the special case of coproc, as this command opens
         * a pipe between the shell and the new coprocess before local redirections
         * are performed. we let coproc() do everything by passing it the list of
         * I/O redirections.
         */
        if(argc && strcmp(argv[0], "coproc") == 0 && 
            flag_set(special_builtins[REGULAR_BUILTIN_COPROC].flags, BUILTIN_ENABLED))
        {
            int res = coproc_builtin(argc, argv, io_files);
            set_internal_exit_status(res);
            free_argv(argc, argv);
            return !res;
        }
        
        /*
         * for all builtins, exept 'exec', we'll save (and later restore) the standard
         * input/output/error streams.
         */
        int savestd = !argc || (strcmp(argv[0], "exec") != 0);
        
        /* perform I/O redirection, if any */
        if(total_redirects && !redirect_do(io_files, savestd))
        {
            if(dofork)
            {
                exit(EXIT_FAILURE);
            }
            /* I/O redirection failure */
            free_argv(argc, argv);
            free_symtab(symtab_stack_pop());
            /* restore standard streams */
            if(savestd)
            {
                restore_stds();
            }
            return 0;
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
                    res = bg_builtin(2, (char *[]){ "bg", argv[0], NULL });
                }
                else
                {
                    res = fg_builtin(2, (char *[]){ "fg", argv[0], NULL });
                }
                //while(argc--) free_malloced_str(argv[argc]);
                free_argv(argc, argv);
                
                if(savestd && total_redirects)
                {
                    restore_stds();
                }
                if(exit_status)
                {
                    EXIT_IF_NONINTERACTIVE();
                }
                return res;
            }
        }
        
        /* POSIX Command Search and Execution Algorithm:      */
        search_and_exec(src, argc, argv, NULL, SEARCH_AND_EXEC_DOFUNC);
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
            i = 0;
            if((strcmp(argv[0], "break"   ) == 0) ||
               (strcmp(argv[0], "continue") == 0) ||
               (strcmp(argv[0], "return"  ) == 0))
            {
                i = 1;
            }
            else
            {
                i = 0;
            }
            free_argv(argc, argv);

            /* restore standard streams */
            if(savestd && total_redirects)
            {
                restore_stds();
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

            return 1;
        }
    }
    /* ... and parent countinues over here ...    */

#if 0
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
#endif
    
    int status = wait_on_child(child_pid, node, NULL);

#if 0
    /* reset the terminal's foreground pgid */
    if(option_set('m'))
    {
        tcsetpgrp(0, tty_pid);
    }
#endif

    PRINT_EXIT_STATUS(status);


    /* if we forked, we didn't hash the utility's path in our
     * hashtable. if so, do it now.
     */
    int cmdfound = WIFEXITED(status) && WEXITSTATUS(status) != 126 && WEXITSTATUS(status) != 127;
    if(option_set('h') && cmdfound && dofork)
    {
        if(!strchr(argv[0], '/'))
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

    free_symtab(symtab_stack_pop());

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
int do_command(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }

    switch(node->type)
    {
        case NODE_FUNCTION:
            return do_function_definition(node);

        case NODE_COMMAND:
            return do_simple_command(src, node);

        case NODE_TIME:
            return time_builtin(src, node->first_child);

        default:
            return do_compound_command(src, node, redirect_list);
    }
}
