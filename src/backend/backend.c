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

int cur_loop_level = 0;
int req_loop_level = 0;
int req_break      = 0;
int req_continue   = 0;
int cur_func_level = 0;

#define MERGE_GLOBAL_SYMTAB()                       \
do {                                                \
    struct symtab_s *symtab = symtab_stack_pop();   \
    merge_global(symtab);                           \
    free_symtab(symtab);                            \
} while(0)

#define ERR_TRAP_OR_EXIT()                          \
    if(!res || exit_status)                         \
    {                                               \
        trap_handler(ERR_TRAP_NUM);                 \
        if(option_set('e'))                         \
            exit_gracefully(EXIT_FAILURE, NULL);    \
    }

/* 
 * in tcsh, if an interactive program exits with non-zero exit status,
 * the shell prints a message with the exit status.
 */

#define PRINT_EXIT_STATUS(status)                   \
    if(option_set('i') &&                           \
       optionx_set(OPTION_PRINT_EXIT_VALUE) &&      \
       WIFEXITED((status)) && WEXITSTATUS((status)))\
    {                                               \
        fprintf(stderr, "Exit %d\r\n",              \
                WEXITSTATUS(status));               \
    }


/* defined in jobs.c */
int rip_dead(pid_t pid);

/* defined in redirect.c */
int __redirect_do(struct io_file_s *io_files, int do_savestd);

/* defined in main.c */
extern char  *cmdstr_filename;

/* defined in cmdline.c */
extern char  *stdin_filename;

/* defined in builtins/coproc.c */
int    coproc(int argc, char **argv, struct io_file_s *io_files);

/* defined below */
char *get_cmdstr(struct node_s *cmd);


/*
 * increment the value of the $SUBSHELL variable.
 */
void inc_subshell_var()
{
    /*
    int subshell = get_shell_vari("SUBSHELL", 0);
    if(subshell < 0) subshell = 0;
    subshell++;
    */
    subshell_level++;
    char buf[8];
    //sprintf(buf, "%d", subshell);
    sprintf(buf, "%d", subshell_level);
    struct symtab_entry_s *entry = get_symtab_entry("SUBSHELL");
    if(!entry) entry = add_to_symtab("SUBSHELL");
    symtab_entry_setval(entry, buf);
}


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
        status = rip_dead(pid);
    }
    /* collect the status. if stopped, add as background job */
    if(WIFSTOPPED(status) && option_set('m'))
    {
        if(!job)
        {
            /* TODO: use correct command str */
            char *cmdstr = get_cmdstr(cmd);
            job = add_job(pid, (pid_t[]){pid}, 1, cmdstr, 1);
            if(cmdstr) free(cmdstr);
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
                    if(!(job->child_exitbits & j))
                    /*
                     * even a process that exited with 0 exit status should have
                     * a non-zero status field (that's why we check exit status using
                     * the macro WIFEXITED, not by hand).
                     */
                    {
                        pid = job->pids[i];
                        status = 0;
                        goto _wait;
                    }
                }
            }
            job->flags |= JOB_FLAG_NOTIFIED;
        }
        status = job->status;
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
    //reset_signals();
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
    if(!entry) return;
    symtab_entry_setval(entry, val);
    if(set_env) setenv("_", val, 1);
}

/*
 * get the first line of a script file, which should read like:
 *      #!interpreter [options]
 */
char *get_first_line(char *path)
{
    if(!path || !*path) return NULL;
    FILE *f = fopen(path, "r");
    if(!f) return NULL;
    char buf[256];                  /* old Unixes allow a max of 32 */
    char *res = NULL;
    if(fgets(buf, 256, f))
    {
        size_t len = strlen(buf);
        if(buf[len-1] == '\n')      /* TODO: handle an incomplete (long) first lines */
        {
            buf[len-1] = '\0';
            if(buf[0] == '#' && buf[1] == '!') res = get_malloced_str(buf);
        }
    }
    fclose(f);
    return res;
}

void do_exec_script(char *path, int argc, char **argv)
{
    /* try executing the shell (or other interpreter) with cmd as argument */
    char *first = get_first_line(path);
    char *argv2[argc+3];
    int i = 0, lsh = 0;
    if(!first) lsh = 1;     /* use our shell */
    /* does the first line contain interpreter name with an optional argument? */
    char *space = strchr(first, ' ');
    if(lsh)
    {
        /* in tcsh, special alias 'shell' give the full pathname of the shell to use */
        char *s = "shell";
        char *sh = __parse_alias(s);
        if(sh != s && sh != null_alias)
        {
             space = strchr(sh, ' ');
             argv2[i++] = sh;
        }
        else argv2[i++] = shell_argv[0];
    }
    else argv2[i++] = first+2;           /* skip the '#!' part */

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
    if((pid = fork()) < 0)
    {
        fprintf(stderr, "%s: failed to fork subshell to execute script: %s\r\n", SHELL_NAME, strerror(errno));
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
        if(first) free_malloced_str(first);
    }
}

/*
 * POSIX algorithm for command search and execution.
 */
int do_exec_cmd(int argc, char **argv, char *use_path, int (*internal_cmd)(int, char **))
{
    int i = 0;
    while(i < argc)
    {
        i++;
    }
    if(internal_cmd)
    {
        int res = internal_cmd(argc, argv);
        set_underscore_val(argv[argc-1], 0);
        return res;
    }
    /* STEP 1-D: search for command using $PATH if there is no slash in the command name */
    if(strchr(argv[0], '/'))
    {
        /* is this shell restricted? */
        if(option_set('r'))
        {
            /* r-shells can't specify commands with '/' in their names */
            fprintf(stderr, "%s: can't execute '%s': restricted shell\r\n", SHELL_NAME, argv[0]);
            return 0;
        }
        set_underscore_val(argv[0], 1);    /* absolute pathname of command exe */
        execv(argv[0], argv);
        if(errno == ENOEXEC)
        {
            do_exec_script(argv[0], argc, argv);
        }
    }
    else
    {
        /* check for a hashed utility name */
        char *path;
        if(option_set('h'))
        {
            path = get_hashed_path(argv[0]);
            if(path)
            {
                int go = 1;
                /* check the hashed path still exists (bash) */
                if(optionx_set(OPTION_CHECK_HASH)) go = file_exists(path);
                if(go)
                {
                    set_underscore_val(path, 1);    /* absolute pathname of command exe */
                    execv(path, argv);
                }
            }
        }
        
        /* if we came back, we failed to execute the utility.
         * try searching for another utility using the given path.
         */
        path = search_path(argv[0], use_path, 1);
        if(!path) return 0;
        if(option_set('h')) hash_utility(argv[0], path);
        set_underscore_val(path, 1);    /* absolute pathname of command exe */
        execv(path, argv);
        
        /* we hashed the wrong utility. remove it. */
        if(option_set('h')) unhash_utility(argv[0]);
        
        if(errno == ENOEXEC)
        {
            do_exec_script(path, argc, argv);
        }
        free_malloced_str(path);
    }
    return 0;
}


int __break(int argc, char **argv)
{
    if(!cur_loop_level)
    {
        BACKEND_RAISE_ERROR(BREAK_OUTSIDE_LOOP, NULL, NULL);
        /* POSIX says non-interactive shell should exit on syntax errors */
        if(!option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
        return 1;
    }
    if(argc == 1) req_loop_level = 1;
    else
    {
        int n = atoi(argv[1]);
        if(n < 1) return 1;
        req_loop_level = n;
    }
    req_break = req_loop_level;
    return 0;
}

int __continue(int argc, char **argv)
{
    if(!cur_loop_level)
    {
        BACKEND_RAISE_ERROR(CONTINUE_OUTSIDE_LOOP, NULL, NULL);
        /* POSIX says non-interactive shell should exit on syntax errors */
        if(!option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
        return 1;
    }
    if(argc == 1) req_loop_level = 1;
    else
    {
        int n = atoi(argv[1]);
        if(n < 1) return 1;
        req_loop_level = n;
    }
    req_continue = req_loop_level;
    return 0;
}


char *get_cmdstr(struct node_s *cmd)
{
    /*
     * we call __get_malloced_str() because the other call to cmd_nodetree_to_str()
     * doesn't call get_malloced_str(), which will confuse our caller when it wants
     * to free the returned string.
     */
    if(!cmd) return __get_malloced_str("(no command)");
    return cmd_nodetree_to_str(cmd);
}


int  do_complete_command(struct node_s *node)
{
    return do_list(node);
}


int  do_list(struct node_s *node)
{
    if(!node) return 0;
    if(node->type != NODE_LIST) return do_and_or(node, NULL, 1);
    struct node_s *cmd = node->first_child;
    
    /* is this a background job? */
    int wait = 1;
    if(node->val_type == VAL_CHR)
    {
        wait = (node->val.chr == '&') ? 0 : 1;
    }
    else if(node->val_type == VAL_STR)
    {
        /* if it end in unquoted &, then yes */
        int c1 = strlen(node->val.str)-1;
        int c2 = c1-1;
        if(c2 < 0) c2 = 0;
        if(node->val.str[c1] == '&' && node->val.str[c2] != '\\') wait = 0;
    }

    if(!wait)
    {
        pid_t pid;
        if((pid = fork()) < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
        else if(pid > 0)
        {
            struct job *job;
            char *cmdstr = get_cmdstr(node);
            if(!(job = add_job(pid, (pid_t[]){pid}, 1, cmdstr, 1)))
            {
                BACKEND_RAISE_ERROR(FAILED_TO_ADD_JOB, NULL, NULL);
                if(cmdstr) free(cmdstr);
                return 0;
            }
            set_cur_job(job);
            fprintf(stderr, "[%d] %u\r\n", job->job_num, pid);
            set_exit_status(0, 0);
            /*
             * give the child process a headstart, in case the scheduler decided to run us first.
             */
            sleep(1);
            if(cmdstr) free(cmdstr);
            return 1;
        }
        else
        {
            asynchronous_prologue();
            int res = do_and_or(cmd, NULL, 0);
            if(!res) exit(exit_status);
            if(cmd->next_sibling) do_list(cmd->next_sibling);
            exit(exit_status);
        }
    }
    int res = do_and_or(cmd, NULL, 1 /* wait */);
    if(!res) return 0;
    if(cmd->next_sibling) return do_list(cmd->next_sibling);
    return res;
}

int  do_and_or(struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node) return 0;
    if(node->type != NODE_ANDOR)
        return do_pipeline(node, redirect_list, fg);
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
    if(res == 0) return 0;
loop2:
    node = node->next_sibling;
    if(!node) return 1;
    cmd  = node->first_child;
    if(!cmd ) return 1;
    if(exit_status == 0)    /* success */
    {
        if(node->type == NODE_AND_IF)
             goto loop;
        else goto loop2;
    }
    /* failure */
    if(node->type == NODE_AND_IF) goto loop2;
    else goto loop;
}

int  do_pipeline(struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node) return 0;
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
        if(option_set('e')) exit_gracefully(EXIT_FAILURE, NULL);
    }
    return res;
}

int  do_pipe_sequence(struct node_s *node, struct node_s *redirect_list, int fg)
{
    if(!node) return 0;
    if(node->type != NODE_PIPE) return do_command(node, redirect_list, fg);
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
        dup  (filedes[0]);
    }
    else
    {
        /* fork the last command */
        if((pid = fork()) == 0)
        {
            /* tell the terminal who's the foreground pgid now */
            if(option_set('m'))
            {
                setpgid(0, 0);
                if(fg) tcsetpgrp(0, 0);
            }
            /* only restore tty to canonical mode if we are reading from it */
            extern char *stdin_filename;    /* defined in cmdline.c */
            if(src->filename == stdin_filename) term_canon(1);
            reset_nonignored_traps();
            /* 2nd command component of command line */
            close(0);   /* stdin */
            dup  (filedes[0]);
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
        if((pid2 = fork()) == 0)
        {
            if(option_set('m'))
            {
                setpgid(0, pid);
                if(fg) tcsetpgrp(0, pid);
            }
            /* only restore tty to canonical mode if we are reading from it */
            extern char *stdin_filename;    /* defined in cmdline.c */
            if(src->filename == stdin_filename) term_canon(1);
            reset_nonignored_traps();
            /* first command of pipeline */
            close(1);   /* stdout */
            dup  (filedes[1]);
            close(filedes[1]);
            close(filedes[0]);
            /* stdout now goes to pipe */
            /* child process does command */
            if(next)
            {
                close(0);   /* stdin */
                dup  (filedes2[0]);
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

    if(option_set('m')) setpgid(pid, 0);

    /* TODO: use correct command str */
    char *cmdstr = get_cmdstr(node);
    /* $! will be set in add_job() */
    struct job *job = add_job(pid, all_pids, count, cmdstr, !fg);
    set_cur_job(job);
    if(cmdstr) free(cmdstr);
    
    /* run the last command ourselves -- bash extension */
    if(lastpipe)
    {
        do_command(cmd, redirect_list, 0);
        set_pid_exit_status(job, pid, exit_status);
        job->child_exitbits |= 1;       /* mark our entry as done */
        job->child_exits++;
        close(0);   /* restore stdin */
        open("/dev/tty", O_RDWR);
    }
    
    if(fg)
    {
        /* tell the terminal who's the foreground pgid now */
        if(option_set('m')) tcsetpgrp(0, pid);
        int status = wait_on_child(pid, node, job);
        /* reset the terminal's foreground pgid */
        if(option_set('m')) tcsetpgrp(0, tty_pid);
        PRINT_EXIT_STATUS(status);
        set_exit_status(status, 1);
        return !status;
    }
    else
    {
        fprintf(stderr, "[%d] %u %s\r\n", job->job_num, pid, job->commandstr);
        set_exit_status(0, 0);
        return 1;
    }
}

int  do_term(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    if(node->type != NODE_TERM) return do_and_or(node, redirect_list, 1);
    int wait = (node->val.chr == '&') ? 0 : 1;
    if(!wait)
    {
        pid_t pid;
        if((pid = fork()) < 0)
        {
            BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
            return 0;
        }
        else if(pid > 0)
        {
            struct job *job;
            char *cmdstr = get_cmdstr(node->first_child);
            if(!(job = add_job(pid, (pid_t[]){pid}, 1, cmdstr, 1)))
            {
                BACKEND_RAISE_ERROR(FAILED_TO_ADD_JOB, NULL, NULL);
                if(cmdstr) free(cmdstr);
                return 0;
            }
            set_cur_job(job);
            set_exit_status(0, 0);
            if(cmdstr) free(cmdstr);
            return 1;
        }
        else
        {
            asynchronous_prologue();
            struct node_s *child = node->first_child;
            int res = do_and_or(child, redirect_list, 0);
            if(!res) exit(exit_status);
            if(child->next_sibling) do_term(child->next_sibling, redirect_list);
            exit(exit_status);
        }
    }
    struct node_s *child = node->first_child;
    int res = do_and_or(child, redirect_list, 1 /* wait */);
    if(!res) return 0;
    if(child->next_sibling) return do_term(child->next_sibling, redirect_list);
    return res;
}

int  do_compound_list(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    if(node->type != NODE_LIST) return do_term(node, redirect_list);
    node = node->first_child;
    int res = 0;
    while(node)
    {
        res = do_term(node, redirect_list);
        if(!res) break;
        node = node->next_sibling;
    }
    return res;
}

int  do_subshell(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    /* redirects specific to the loop should override global ones */
    struct node_s *subshell = node->first_child;
    struct node_s *local_redirects = subshell ? subshell->next_sibling : NULL;
    if(local_redirects) redirect_list = local_redirects;
    pid_t pid;
    if((pid = fork()) < 0)
    {
        BACKEND_RAISE_ERROR(FAILED_TO_FORK, strerror(errno), NULL);
        return 0;
    }
    else if(pid > 0)
    {
        wait_on_child(pid, node, NULL);
        if(redirect_list) redirect_restore();
        if(option_set('m')) tcsetpgrp(0, tty_pid);
        return 1;
    }
    /*
     * POSIX says we should restore non-ignored signals to their
     * default actions.
     */
    reset_nonignored_traps();
    /*
     * reset the DEBUG trap if -o functrace (-T) is not set, and the ERR trap
     * if -o errtrace (-E) is not set. traced functions inherit both traps
     * from the calling shell (bash).
     */
    if(!option_set('T'))
    {
        save_trap("DEBUG" );
        save_trap("RETURN");
    }
    if(!option_set('E')) save_trap("ERR");
    /*
     * the -e (errexit) option is reset in subshells if inherit_errexit
     * is not set (bash).
     */
    if(!optionx_set(OPTION_INHERIT_ERREXIT)) set_option('e', 0);
    /* perform I/O redirections */
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }
    inc_subshell_var();
    /* do the actual commands */
    do_compound_list(subshell, NULL /* redirect_list */);
    /* no need to pop symtab or restore traps as we are exiting anyway */
    exit(exit_status);
}

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


#define ARG_COUNT   0x1000
/*
 * peform word expansion on the list items and create
 * a char ** array from it, similar to **argv.
 */
char **__make_list(struct cmd_token *first_tok, int *token_count)
{
    int count = 0;
    char *tmp[ARG_COUNT];
    struct cmd_token *t = first_tok;
    while(t)
    {
        glob_t glob;
        char **matches = filename_expand(cwd, t->data, &glob);
        if(!matches || !matches[0])
        {
            globfree(&glob);
            if(!optionx_set(OPTION_NULL_GLOB))       /* bash extension */
            {
                tmp[count++] = get_malloced_str(t->data);
            }
            if( optionx_set(OPTION_FAIL_GLOB))       /* bash extension */
            {
                fprintf(stderr, "%s: file globbing failed for %s\n", SHELL_NAME, t->data);
                while(--count >= 0) free_malloced_str(tmp[count]);
                return errno = ENOMEM, NULL;
            }
        }
        else
        {
            char **m = matches;
            int j = 0;
            do
            {
                //fflush(stdout);
                tmp[count++] = get_malloced_str(*m);
                if(count >= ARG_COUNT) break;
            } while(++m, ++j < glob.gl_pathc);
            globfree(&glob);
        }
        if(count >= ARG_COUNT) break;
        t = t->next;
    }
    if(!count) return NULL;
    char **argv = (char **)malloc((count+1) * sizeof(char *));
    if(!argv)
    {
        while(--count >= 0) free_malloced_str(tmp[count]);
        return errno = ENOMEM, NULL;
    }
    *token_count = count;
    argv[count] = NULL;
    while(--count >= 0) argv[count] = tmp[count];
    return argv;
}

char **get_word_list(struct node_s *wordlist, int *_count)
{
    struct cmd_token *prev = NULL;
    struct cmd_token *head = NULL;
    struct cmd_token *cur  = NULL;
    struct cmd_token *tail = NULL;
    int    count = 0;
    char  **list = NULL;
    *_count = 0;
    
    if(wordlist)
    {
        struct cmd_token tok_list_head;
        tail = &tok_list_head;
        wordlist = wordlist->first_child;
        while(wordlist)
        {
            struct cmd_token *t = make_cmd_token(wordlist->val.str);
            tail->next = t;
            tail       = t;
            count++;
            wordlist = wordlist->next_sibling;
        }
        head = tok_list_head.next;
    }
    else
    {
        /* use the actual arguments to the script (i.e. "$@") */
        struct symtab_entry_s *entry = get_symtab_entry("#");
        count = atoi(entry->val);
        if(count)
        {
            head = get_all_pos_params('@', 1);
        }
    }
    
    if(!head)
    {
        return NULL;
    }

    /* now do POSIX parsing on those tokens */
    cur  = head;
    tail = head;
    while(cur)
    {
        /* then, word expansion */
        struct cmd_token *t = word_expand(cur, &tail, 0, 1);
        /* null? remove this token from list */
        if(!t)
        {
            t = cur->next;
            free(cur->data);
            free(cur);
            if(prev) prev->next = t;
            else     head       = t;
            cur = t;
        }
        /* new sublist is formed (i.e. field splitting resulted in more than one field) */
        else
        {
            struct cmd_token *next = cur->next;
            if(t != cur)
            {
                free(cur->data);
                free(cur);
            }
            if(prev) prev->next = t;
            else     head       = t;
            tail->next = next;
            prev       = tail;
            cur        = next;
        }
    }
    list = __make_list(head, &count);
    free_all_tokens(head);
    *_count = count;
    return list;
}


/* 
 * execute the second form of 'for' loops:
 * 
 *    for((expr1; expr2; expr3)); do commands; done
 * 
 * this is a non-POSIX extension used by all major shells.
 */
int  do_for_clause2(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    struct node_s *expr1 = node->first_child;
    if(!expr1 || expr1->type != NODE_ARITHMETIC_EXPR) return 0;
    struct node_s *expr2 = expr1->next_sibling;
    if(!expr2 || expr2->type != NODE_ARITHMETIC_EXPR) return 0;
    struct node_s *expr3 = expr2->next_sibling;
    if(!expr3 || expr3->type != NODE_ARITHMETIC_EXPR) return 0;
    struct node_s *commands = expr3->next_sibling;
    if(!commands)
    {
        set_exit_status(0, 0);
        return 1;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }

    /* first evaluate expr1 */
    char *str = expr1->val.str;
    char *str2;
    if(str && *str)
    {
        str2 = __do_arithmetic(str);
        if(!str2)     /* invalid expr */
        {
            redirect_restore();
            return 0;
        }
        free(str2);
    }

    /* then loop as long as expr2 evaluates to non-zero result */
    int res   = 0;
    int endme = 0;
    char *onestr = "1";
    cur_loop_level++;
    while(!endme)
    {
        str = expr2->val.str;
        if(str && *str)
        {
            str2 = __do_arithmetic(str);
            if(!str2)       /* invalid expr */
            {
                res = 0;
                break;
            }
        }
        else                /* treat it as 1 and loop */
        {
            str2 = onestr;
        }
        /* perform the loop body */
        if(atol(str2))
        {
            if(!do_do_group(commands, NULL))
            {
                res = 1;
            }
            if(SIGINT_received) endme = 1;
            if(req_break) req_break--, endme = 1;
            if(req_continue >= 1)
            {
                if(--req_continue) endme = 1;
            }
            if(str2 != onestr) free(str2);
            res = 1;
            if(endme) break;
            /* evaluate expr3 */
            str = expr3->val.str;
            if(str && *str)
            {
                str2 = __do_arithmetic(str);
                if(!str2)     /* invalid expr */
                {
                    redirect_restore();
                    return 0;
                }
                free(str2);
            }
        }
        else
        {
            res   = 1;
            endme = 1;
        }
    }
    cur_loop_level--;
    redirect_restore();
    return res;
}


int  do_for_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    struct node_s *index    = node->first_child;
    if(index->type == NODE_ARITHMETIC_EXPR)
        return do_for_clause2(node, redirect_list);

    struct node_s *wordlist = index->next_sibling;
    if(wordlist->type != NODE_WORDLIST) wordlist = NULL;
    struct node_s *commands = wordlist ? 
                              wordlist->next_sibling : 
                              index->next_sibling;
    if(!commands)
    {
        set_exit_status(0, 0);
        return 1;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }

    int    count           = 0;
    char **list            = get_word_list(wordlist, &count);
    int i;
    if(!count || !list)
    {
        set_exit_status(0, 0);
        redirect_restore();
        return 1;
    }

    
    int j     = 0;
    int res   = 0;
    int endme = 0;
    /* we should now be set at the first command inside the for loop */
    char *index_name = index->val.str;
    /*
     * we set FLAG_CMD_EXPORT so that the index var will be exported to all commands
     * inside the for loop.
     */
    if(__set(index_name, NULL, 0, FLAG_CMD_EXPORT, 0) == -1)
    {
        fprintf(stderr, "%s: can't assign to readonly variable\n", SHELL_NAME);
        goto end;
    }
    cur_loop_level++;
// loop:
    for( ; j < count; j++)
    {
        if(__set(index_name, list[j], 0, 0, 0) == -1)
        {
            fprintf(stderr, "%s: can't assign to readonly variable\n", SHELL_NAME);
            res = 0;
            goto end;
        }
        if(!do_do_group(commands, NULL /* redirect_list */))
        {
            res = 1;
        }
        if(SIGINT_received) endme = 1;
        if(req_break) req_break--, endme = 1;
        if(req_continue >= 1)
        {
            if(--req_continue) endme = 1;
        }
        if(endme) break;
        res = 1;
    }
end:
    for(i = 0; i < count; i++) free_malloced_str(list[i]);
    //free(list[0]);
    free(list);
    cur_loop_level--;
    redirect_restore();
    return res /* 1 */;
}


int  do_select_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    struct node_s *index    = node->first_child;
    struct node_s *wordlist = index->next_sibling;
    if(wordlist->type != NODE_WORDLIST) wordlist = NULL;
    struct node_s *commands = wordlist ? 
                              wordlist->next_sibling : 
                              index->next_sibling;
    if(!commands)
    {
        set_exit_status(0, 0);
        return 1;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }
    int    count           = 0;
    char **list            = get_word_list(wordlist, &count);
    int i;
    if(!count || !list)
    {
        set_exit_status(0, 0);
        redirect_restore();
        return 1;
    }

    
    /* we should now be set at the first command inside the for loop */
    char *index_name = index->val.str;
    __set(index_name, NULL, 0, 0, 0);
    int j     = 0;
    int res   = 0;
    int endme = 0;
    cur_loop_level++;
    for(j = 0; j < count; j++) fprintf(stderr, "%d\t%s\r\n", j+1, list[j]);
    for(;;)
    {
        print_prompt3();
        if(__read(2, (char *[]){ "read", "REPLY" }) != 0)
        {
            res = 0;
            fprintf(stderr, "\n\n");
            break;
        }
        /* parse the selection */
        char *strend;
        struct symtab_entry_s *entry = get_symtab_entry("REPLY");
        /*
         * empty response. ksh prints PS3, while bash reprints the select list.
         * the second approach seems more appropriate, so we follow it.
         */
        if(!entry->val || !entry->val[0])
        {
            for(j = 0; j < count; j++) fprintf(stderr, "%d\t%s\r\n", j+1, list[j]);
            continue;
        }
        int sel = strtol(entry->val, &strend, 10);
        /* invalid (non-numeric) response */
        if(strend == entry->val)
        {
            symtab_entry_setval(entry, NULL);
            continue;
        }
        if(sel < 1 || sel > count)
        {
            symtab_entry_setval(entry, NULL);
            continue;
        }
        __set(index_name, list[sel-1], 0, 0, 0);
        if(!do_do_group(commands, NULL /* redirect_list */))
        {
            res = 1;
        }
        if(SIGINT_received) endme = 1;
        if(req_break) req_break--, endme = 1;
        if(req_continue >= 1)
        {
            if(--req_continue) endme = 1;
        }
        if(endme) break;
        res = 1;
        /* if var is null, reprint the list */
        entry = get_symtab_entry("REPLY");
        if(!entry->val || !entry->val[0])
        {
            for(j = 0; j < count; j++) fprintf(stderr, "%d\t%s\r\n", j+1, list[j]);
        }
    }
    for(i = 0; i < count; i++) free_malloced_str(list[i]);
    //free(list[0]);
    free(list);
    cur_loop_level--;
    redirect_restore();
    return res /* 1 */;
}


char *__tok_to_str(struct cmd_token *tok)
{
    if(!tok) return NULL;
    size_t len = 0;
    struct cmd_token *t = tok;
    while(t)
    {
        t->len = strlen(t->data);
        len += t->len+1;
        t    = t->next;
    }
    char *str = (char *)malloc(len+1);
    if(!str) return NULL;
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


int  do_case_item(struct node_s *node, char *word, struct node_s *redirect_list)
{
    /* 
     * the root node is a NODE_CASE_ITEM. we need to iterate its 
     * NODE_VAR children.
     */
    node = node->first_child;
    while(node && node->type == NODE_VAR)
    {
        char *pat_str = word_expand_to_str(node->val.str);
        if(!pat_str) goto no_match;
        if(match_filename(pat_str, word, 1, 1))
        {
            struct node_s *commands = node->next_sibling;
            while(commands && commands->type == NODE_VAR)
                commands = commands->next_sibling;
            if(commands)
            {
                int res = do_compound_list(commands, redirect_list);
                ERR_TRAP_OR_EXIT();
            }
            free(pat_str);
            return 1;
        }
        free(pat_str);
no_match:
        node = node->next_sibling;
    }
    return 0;
}

int  do_case_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node || !node->first_child) return 0;
    struct node_s *word_node = node->first_child;
    struct node_s *item = word_node->next_sibling;

    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = word_node;
    while(local_redirects &&
          local_redirects->type != NODE_IO_REDIRECT_LIST)
        local_redirects = local_redirects->next_sibling;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }

    /* get the word */
    char *word = word_expand_to_str(word_node->val.str);
    if(!word)
    {
        BACKEND_RAISE_ERROR(EMPTY_CASE_WORD, NULL, NULL);
        if(local_redirects) redirect_restore();
        /* POSIX says non-interactive shell should exit on syntax errors */
        EXIT_IF_NONINTERACTIVE();
        return 0;
    }
    
    while(item)
    {
        if(do_case_item(item, word, NULL /* redirect_list */))
        {
            /* check for case items ending in ';&' */
            if(item->val_type == VAL_CHR && item->val.chr == '&')
            {
                /* do the next item */
                item = item->next_sibling;
                if(!item || item->type != NODE_CASE_ITEM) goto fin;
                item = item->first_child;
                if(!item || item->type != NODE_VAR) goto fin;
                struct node_s *commands = item->next_sibling;
                while(commands && commands->type == NODE_VAR)
                    commands = commands->next_sibling;
                if(commands)
                {
                    int res = do_compound_list(commands, redirect_list);
                    ERR_TRAP_OR_EXIT();
                }
            }
            /* check for case items ending in ';;&' */
            else if(item->val_type == VAL_CHR && item->val.chr == ';')
            {
                /* try to match the next item */
                item = item->next_sibling;
                continue;
            }
            goto fin;
        }
        item = item->next_sibling;
    }
    set_exit_status(0, 0);
    
fin:
    free(word);
    if(local_redirects) redirect_restore();
    return 1;
}


int  do_if_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    struct node_s *clause = node->first_child;
    struct node_s *_then  = clause->next_sibling;
    struct node_s *_else  = NULL;
    if(_then) _else = _then->next_sibling;
    int res = 0;

    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = clause;
    while(local_redirects && local_redirects->type != NODE_IO_REDIRECT_LIST)
        local_redirects = local_redirects->next_sibling;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }

    if(!do_compound_list(clause, NULL /* redirect_list */))
    {
        if(local_redirects) redirect_restore();
        return 0;
    }
    if(exit_status == 0)
    {
        res = do_compound_list(_then, NULL /* redirect_list */);
        ERR_TRAP_OR_EXIT();
        if(local_redirects) redirect_restore();
        return res;
    }
    if(!_else)
    {
        if(local_redirects) redirect_restore();
        return 1;
    }
    if(_else->type == NODE_IF)
    {
        res = do_if_clause(_else, NULL /* redirect_list */);
        ERR_TRAP_OR_EXIT();
        if(local_redirects) redirect_restore();
        return res;
    }
    res = do_compound_list(_else, NULL /* redirect_list */);
    ERR_TRAP_OR_EXIT();
    if(local_redirects) redirect_restore();
    return res;
}


int  do_while_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    struct node_s *clause   = node->first_child;
    struct node_s *commands = clause->next_sibling;
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }
    int res         = 0;
    int endme       = 0;
    int first_round = 1;
    cur_loop_level++;
    
    do
    {
        if(!do_compound_list(clause, NULL /* redirect_list */))
        {
            cur_loop_level--;
            if(local_redirects) redirect_restore();
            return 0;
        }
        if(exit_status == 0)
        {
            if(!do_do_group(commands, NULL /* redirect_list */))
                res = 1;
            if(SIGINT_received) endme = 1;
            if(req_break) req_break--, endme = 1;
            if(req_continue >= 1)
            {
                if(--req_continue) endme = 1;
            }
            if(endme) break;
            first_round = 0;
            res = 1;
        }
        else
        {
            if(first_round) set_exit_status(0, 0);
            cur_loop_level--;
            if(local_redirects) redirect_restore();
            return 1;
        }
    } while(1 /* exit_status == 0 */);
    cur_loop_level--;
    if(local_redirects) redirect_restore();
    return res;
}


int  do_until_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    struct node_s *clause   = node->first_child;
    struct node_s *commands = clause->next_sibling;
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
    if(local_redirects) redirect_list = local_redirects;
    if(redirect_list)
    {
        if(!redirect_do(redirect_list)) return 0;
    }
    int res         = 0;
    int endme       = 0;
    int first_round = 1;
    cur_loop_level++;

    do
    {
        if(!do_compound_list(clause, NULL /* redirect_list */))
        {
            cur_loop_level--;
            if(local_redirects) redirect_restore();
            return 0;
        }
        if(exit_status == 0)
        {
            if(first_round) set_exit_status(0, 0);
            cur_loop_level--;
            if(local_redirects) redirect_restore();
            return 1;
        }
        else
        {
            if(!do_do_group(commands, NULL /* redirect_list */))
                res = 1;
            if(SIGINT_received) endme = 1;
            if(req_break) req_break--, endme = 1;
            if(req_continue >= 1)
            {
                if(--req_continue) endme = 1;
            }
            if(endme) break;
            first_round = 0;
            res = 1;
        }
    } while(1 /* exit_status != 0 */);
    cur_loop_level--;
    if(local_redirects) redirect_restore();
    return res;
}


int  do_brace_group(struct node_s *node, struct node_s *redirect_list)
{
    int res = do_compound_list(node, redirect_list);
    ERR_TRAP_OR_EXIT();
    return res;
}


int  do_compound_command(struct node_s *node, struct node_s *redirect_list)
{
    if(!node) return 0;
    switch(node->type)
    {
        case NODE_SUBSHELL: return do_subshell     (node, redirect_list);
        case NODE_FOR     : return do_for_clause   (node, redirect_list);
        case NODE_CASE    : return do_case_clause  (node, redirect_list);
        case NODE_IF      : return do_if_clause    (node, redirect_list);
        case NODE_WHILE   : return do_while_clause (node, redirect_list);
        case NODE_UNTIL   : return do_until_clause (node, redirect_list);
        case NODE_SELECT  : return do_select_clause(node, redirect_list);
        default           : return do_brace_group  (node, redirect_list);
    }
}


int  do_io_file(struct node_s *node, struct io_file_s *io_file)
{
    int fileno     = -1;
    int duplicates = 0;
    struct node_s *child = node->first_child;
    if(!child) return 0;
    char *str      = child->val.str;
    char buf[32];

    /* r-shells can't redirect output */
    if(option_set('r'))
    {
        char c = node->val.chr;
        if(c == IO_FILE_LESSGREAT || c == IO_FILE_CLOBBER || c == IO_FILE_GREAT ||
           c == IO_FILE_GREATAND  || c == IO_FILE_DGREAT  || c == IO_FILE_AND_GREAT_GREAT)
        {
            /*
             * NOTE: consequences of failed redirection are handled by the caller, 
             *       i.e. do_simple_command().
             */
            fprintf(stderr," %s: restricted shells can't redirect output\r\n", SHELL_NAME);
            return 0;
        }
    }
    
    switch(node->val.chr)
    {
        case IO_FILE_LESS     : io_file->flags = R_FLAG       ; break;
        case IO_FILE_LESSAND  :
            duplicates     = 1; io_file->flags = R_FLAG       ; break;
        case IO_FILE_LESSGREAT: io_file->flags = R_FLAG|W_FLAG; break;
        case IO_FILE_CLOBBER  : io_file->flags = C_FLAG       ; break;
        case IO_FILE_GREAT    : io_file->flags = W_FLAG       ; break;
        case IO_FILE_GREATAND :
            duplicates     = 1; io_file->flags = W_FLAG       ; break;
        case IO_FILE_AND_GREAT_GREAT:
            duplicates     = 1; io_file->flags = A_FLAG       ; break;
        case IO_FILE_DGREAT   : io_file->flags = A_FLAG       ; break;
    }

    if(duplicates && strcmp(str, "-"))
    {
        /* I/O from coprocess */
        if(strcmp(str, "p-") == 0 || strcmp(str, "p") == 0)
        {
            switch(node->val.chr)
            {
                case IO_FILE_LESSAND  :
                    if(wfiledes[0] == -1) goto invalid;
                    fileno = wfiledes[0];
                    break;
                    
                case IO_FILE_GREATAND :
                    if(rfiledes[1] == -1) goto invalid;
                    fileno = rfiledes[1];
                    break;
                    
                default: goto invalid;
            }
        }
        else
        {
            char *strend;
            fileno = strtol(str, &strend, 10);
            if(strend == str)
            {
                io_file->duplicates = -1;
                io_file->path       = str;
                return 1;
            }
        }
        if(fileno < 0 || fileno >= FOPEN_MAX) goto invalid;
        if(str[strlen(str)-1] == '-')   /* >&n- and <&n- */
        {
            io_file->flags |= CLOOPEN;
        }
        io_file->duplicates = fileno;
        io_file->path       = NULL;
    }
    else
    {
        io_file->duplicates = -1;
        io_file->path       = str;
    }
    
    return 1;
    
invalid:
    sprintf(buf, "%d", fileno);
    BACKEND_RAISE_ERROR(INVALID_REDIRECT_FILENO, buf, NULL);
    /*
     * NOTE: consequences of failed redirection are handled by the caller, 
     *       i.e. do_simple_command().
     */
    return 0;
}


int  do_io_here(struct node_s *node, struct io_file_s *io_file)
{
    struct node_s *child = node->first_child;
    if(!child) return 0;
    char *heredoc = child->val.str;
    FILE *tmp = tmpfile();
    if(!tmp) return 0;
    if(node->val.chr == IO_HERE_EXPAND)
    {
        struct cmd_token *cmdtok = make_cmd_token(heredoc);
        struct cmd_token *tail   = cmdtok;
        struct cmd_token *head   = word_expand(cmdtok, &tail, 1, 1);
        tail = head;
        while(tail)
        {
            fprintf(tmp, "%s", tail->data);
            tail = tail->next;
        }
        free_all_tokens(head);
    }
    else
    {
        fprintf(tmp, "%s", heredoc);
    }
    int fd = fileno(tmp);
    rewind(tmp);
    io_file->duplicates = fd;
    io_file->path       = NULL;
    io_file->flags      = CLOOPEN;
    return 1;
}


int  do_io_redirect(struct node_s *node, struct io_file_s *io_file)
{
    struct node_s *io = node->first_child;
    if(io->type == NODE_IO_FILE)
        return do_io_file(io, io_file);
    return do_io_here(io, io_file);
}

#if 0
int  do_redirect_list(struct node_s *redirect_list, struct io_file_s *io_files)
{
    if(!redirect_list || redirect_list->type != NODE_IO_REDIRECT_LIST) return 0;
    struct node_s *item = redirect_list->first_child;
    if(!item) return 1;
    while(item)
    {
        int fileno = item->val.sint;
        if(fileno < 0 || fileno >= FOPEN_MAX)
        {
            char buf[32];
            sprintf(buf, "%d", fileno);
            BACKEND_RAISE_ERROR(INVALID_REDIRECT_FILENO, buf, NULL);
            /*
             * NOTE: consequences of failed redirection are handled by the caller, 
             *       i.e. do_simple_command().
             */
            return 0;
        }
        if(!do_io_redirect(item, &io_files[fileno])) return 0;
        item = item->next_sibling;
    }
    return 1;
}
#endif


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
    /*
     * the redirect tree is now disconnected from the root tree,
     * so we need to explicitly free it.
     */
    if(redirect)
    {
        //free_node_tree(redirect);
        /* actually, restore the redirect list as we will need
         * to use it if the function is later invoked.
         */
        child->prev_sibling->next_sibling = redirect;
    }
    /*
     * clear the return flag so that we won't cause the parent 
     * shell to exit also.
     * 
     * TODO: test to make sure this results in the desired behaviour.
     */
    return_set = 0;
    trap_handler(RETURN_TRAP_NUM);
    return res;
}


int  do_function_definition(int argc, char **argv)
{
    if(!argv[0]) return 0;
    struct symtab_entry_s *func = get_func(argv[0]);
    if(!func) return 0;
    /*
     * we keep the parse tree of a function stored, so that subsequent calls
     * to the same function will not need to go through the parsing process over
     * and over. if no parse tree if found, this is the first time the function
     * is called, and we do the parsing right away.
     */
    if(!func->func_body)
    {
        if(!func->val) return 1;
        char *s = func->val;
        /* parse functions that were passed to us in the environment */
        if(s[0] == '(' && s[1] == ')')
        {
            char *f = s+2;
            while(*f && isspace(*f)) f++;
            if(!*f || *f == '}')
            {
                /* empty function body */
                return 1;
            }
            else
            {
                struct source_s save_src = __src;
                src->filename = dot_filename;
                src->buffer   = f;
                src->bufsize  = strlen(f);
                src->curpos   = -2;
                struct token_s *tok = tokenize(src);
                struct node_s *body = parse_function_body(tok);
                if(body) func->func_body = body;
                func->val_type = SYM_FUNC;
                __src = save_src;
            }
        }
        else return 1;
    }
    /* check we are not exceeding the maximum function nesting level */
    int maxlevel = get_shell_varl("FUNCNEST", 0);
    if(maxlevel < 0) maxlevel = 0;
    if(maxlevel && cur_func_level >= maxlevel)
    {
        fprintf(stderr, "%s: can't execute the call to %s: maximum function nesting reached\r\n", 
                SHELL_NAME, argv[0]);
        return 0;
    }    
    cur_func_level++;
    //symtab_stack_push();
    /* save current positional parameters - similar to what we do in dot.c */
    char **pos = get_pos_paramsp();
    callframe_push(callframe_new(argv[0], src->filename, src->curline));
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
    struct trap_item_s *debug = NULL, *err = NULL, *ret = NULL;
    if(!flag_set(func->flags, FLAG_FUNCTRACE))
    {
        if(!option_set('T'))
        {
            debug = save_trap("DEBUG" );
            ret   = save_trap("RETURN");
        }
        if(!option_set('E')) err   = save_trap("ERR"  );
    }
    do_function_body(func->func_body);
    /*
     * restore saved traps. struct trap_item_s * memory is freed in the call.
     * if either struct is null, nothing happens to the trap.
     */
    restore_trap("DEBUG" , debug);
    restore_trap("RETURN", ret  );
    restore_trap("ERR"   , err  );
    //struct symtab_s *symtab = symtab_stack_pop();
    //free_symtab(symtab);
    /* restore pos parameters - similar to what we do in dot.c */
    if(pos)
    {
        set_pos_paramsp(pos);
        for(i = 0; pos[i]; i++) free(pos[i]);
        free(pos);
    }
    callframe_popf();
    cur_func_level--;
    return 1;
}


int do_special_builtin(int argc, char **argv)
{
    char   *cmd = argv[0];
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < special_builtin_count; j++)
    {
        if(special_builtins[j].namelen != cmdlen) continue;
        if(strcmp(special_builtins[j].name, cmd) == 0)
        {
            if(!flag_set(special_builtins[j].flags, BUILTIN_ENABLED)) return 0;
            int (*func)(int, char **) = (int (*)(int, char **))special_builtins[j].func;
            int status = do_exec_cmd(argc, argv, NULL, func);
            set_exit_status(status, 0);
            set_underscore_val(argv[argc-1], 0);    /* last argument to previous command */
            return 1;
        }
    }
    return 0;
}

int do_regular_builtin(int argc, char **argv)
{
    char   *cmd = argv[0];
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(strlen(regular_builtins[j].name) != cmdlen) continue;
        if(strcmp(regular_builtins[j].name, cmd) == 0)
        {
            if(!flag_set(regular_builtins[j].flags, BUILTIN_ENABLED)) return 0;
            int (*func)(int, char **) = (int (*)(int, char **))regular_builtins[j].func;
            int status = do_exec_cmd(argc, argv, NULL, func);
            set_exit_status(status, 0);
            set_underscore_val(argv[argc-1], 0);    /* last argument to previous command */
            return 1;
        }
    }
    return 0;
}

static int saved_stdin    = -1;
static int saved_stdout   = -1;
static int saved_stderr   = -1;
       int do_restore_std =  1;

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

void restore_std()
{
    if(!do_restore_std) return;
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


int  do_simple_command(struct node_s *node, struct node_s *redirect_list, int dofork)
{
    struct io_file_s io_files[FOPEN_MAX];
    int i;
    /* first apply the given redirection list, if any */
    int   total_redirects = redirect_prep(redirect_list, io_files);
    
    /* then loop through the command and its arguments */
    struct node_s *child = node->first_child;
    int argc = 0;
    /*
     * TODO: we must allow for larger numbers of arguments
     */
    long max_args = 2048;
    char *argv[max_args];
    /************************************/
    /* 1- collect command arguments     */
    /************************************/
    //char *cwd = getcwd(NULL, 0);
    char *const root = "/";
    char *arg;
    char *s;
    
    symtab_stack_push();

    while(child)
    {
        switch(child->type)
        {
            case NODE_IO_REDIRECT:
                if(redirect_prep_node(child, io_files) == -1) total_redirects = -1;
                else total_redirects++;
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
                    if(!name) break;
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
                    if(option_set('r') && is_restrict_var(name))
                    {
                        /* r-shells can't set/unset SHELL, ENV, FPATH, or PATH */
                        fprintf(stderr, "%s: restricted shells can't set %s\r\n", SHELL_NAME, name);
                        free(name);
                        /* POSIX says non-interactive shells exit on var assignment errors */
                        EXIT_IF_NONINTERACTIVE();
                        break;
                    }
                    else if(is_pos_param(name) || is_special_param(name))
                    {
                        fprintf(stderr, "%s: error setting/unsetting '%s' is not allowed\r\n", SHELL_NAME, name);
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
                            if(gentry && gentry->val) old_val = get_malloced_str(gentry->val);
                        }
                        entry = add_to_symtab(name);
                    }
                    if(entry->val)
                    {
                        /* release the local var's value, if any */
                        if(old_val) free_malloced_str(old_val);
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
                 * WARNING: don't put any case here!
                 */
                
            default:
                s = child->val.str;
                /* go POSIX style on the word */
                struct cmd_token *cmdtok = make_cmd_token(s);
                struct cmd_token *tail = cmdtok;
                struct cmd_token *t    = word_expand(cmdtok, &tail, 0, 1);
                if(!t)
                {
                    break;
                }
                /*
                 * ignore words that start with arithmetic operations if they are the first
                 * thing on the command line. the arithmetic operation itself has been performed
                 * by the calls to word_expand() above, so just discard the result here.
                 * 
                 * in bash, ((expr)) is equivalent to: let "expr", and bash sets the exit status
                 * to 0 if expr evalues to non-zero, or 1 if expr evaluates to zero. in our case,
                 * the exit status is set by __do_arithmetic() in shunt.c when word_expand() calls
                 * it to perform arithmetic substitution.
                 */
                if(!argc && (strncmp(s, "((", 2) == 0 || strncmp(s, "$((", 3) == 0 || strncmp(s, "$[", 2) == 0))
                {
                    free_all_tokens(t);
                    break;
                }
                /* now parse the words */
                struct cmd_token *t2   = t;
                while(t2)
                {
                    s = t2->data;
                    char *p;
                    char *dir = NULL;
                    p = s;
                    if(option_set('f') || !has_regex_chars(p, strlen(p)))
                    {
                        arg = get_malloced_str(s);
                        argv[argc++] = arg;
                        t2 = t2->next;
                        continue;
                    }
                    if(*p == '/') dir = root;
                    else          dir = cwd ;
                    glob_t glob;
                    char **matches = filename_expand(dir, p, &glob);
                    if(!matches || !matches[0])
                    {
                        globfree(&glob);
                        if(!optionx_set(OPTION_NULL_GLOB))       /* bash extension */
                        {
                            arg = get_malloced_str(s);
                            argv[argc++] = arg;
                        }
                        if( optionx_set(OPTION_FAIL_GLOB))       /* bash extension */
                        {
                            fprintf(stderr, "%s: file globbing failed for %s\n", SHELL_NAME, s);
                            for(i = 0; i < argc; i++) free_malloced_str(argv[i]);
                            free_all_tokens(t);
                            return 0;
                        }
                    }
                    else
                    {
                        /* save the matches */
                        int j = 0;
                        for( ; j < glob.gl_pathc; j++)
                        {
                            if(matches[j][0] == '.' &&
                               (matches[j][1] == '.' || matches[j][1] == '\0')) continue;
                            argv[argc++] = get_malloced_str(matches[j]);
                        }
                        globfree(&glob);
                        /* finished */
                    }
                    t2 = t2->next;
                }
                free_all_tokens(t);
        }
        child = child->next_sibling;
    }
    argv[argc] = NULL;
    //free(cwd);
    
    /*
     * interactive shells check for a directory passed as the command word (bash).
     * similat to setting tcsh's implicitcd variable.
     */
    if(option_set('i') && argc == 1 && optionx_set(OPTION_AUTO_CD))
    {
        struct stat st;
        if(stat(argv[0], &st) == 0)
        {
            if(S_ISDIR(st.st_mode))
            {
                int i;
                argc++;
                for(i = argc; i > 0; i--) argv[i] = argv[i-1];
                argv[0] = get_malloced_str("cd");
            }
        }
    }
    
    //int  builtin =  is_builtin(argv[0]);
    int builtin  = is_enabled_builtin(argv[0]);
    int function = is_function(argv[0]);
    struct symtab_entry_s *entry;

    if(total_redirects == -1 /* redir_fail */)
    {
        
        /* POSIX says non-interactive shell shall exit on redirection errors with
         * special builtins, may exit with compound commands and functions, and shall
         * not exit with other (non-special-builtin) utilities. interactive shell shall
         * not exit in any condition.
         */
        if(!argc || builtin || function)
            EXIT_IF_NONINTERACTIVE();
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
        else if(argc == 1 && *argv[0] == '%') dofork = 0;
        /* reset the request to exit flag */
        if(strcmp(argv[0], "exit")) tried_exit = 0;
    }
    else dofork = 0;
    
    if(option_set('x'))
    {
        print_prompt4();
        for(i = 0; i < argc; i++)
        {
            fprintf(stderr, "%s ", argv[i]);
        }
        printf("\r\n");
    }
    
    /* set $LINENO if we're not reading from the commandline */
    if(src->filename != stdin_filename)
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
            if(s) free(s);
        }
    }
    
    /* expand $PS0 and print the result (bash) */
    char *pr = __evaluate_prompt(get_shell_varp("PS0", NULL));
    if(pr)
    {
        fprintf(stderr, "%s", pr);
        free(pr);
    }
    
    extern char *stdin_filename;    /* defined in cmdline.c */
    
    trap_handler(DEBUG_TRAP_NUM);    
    
    /* in tcsh, special alias postcmd is run before running each command */
    run_alias_cmd("postcmd");
    
    /*
     * in tcsh, special alias jobcmd is run before running commands and when jobs.
     * change state (so why do we need 'postcmd' in the first place?).
     * 
     */
    run_alias_cmd("jobcmd");
    
    pid_t child_pid = 0;
    if(dofork && (child_pid = fork()) == 0)
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
        //reset_signals();
        reset_nonignored_traps();
        do_export_vars();
    }
    
   
// redir:
    if(child_pid == 0)
    {
        /* only restore tty to canonical mode if we are reading from it */
        if(src->filename == stdin_filename && isatty(0) && getpgrp() == tcgetpgrp(0))
            term_canon(1);
        /* if we don't have redirects, no need for forking a subshell */
        if(argc == 0 && total_redirects) /* && !in_subshell) */
        {
            pid_t pid;
            if((pid = fork()) < 0)
            {
                BACKEND_RAISE_ERROR(FAILED_TO_FORK, NULL, NULL);
                return 0;    
            }
            else if(pid > 0)
            {
                int status = wait_on_child(pid, NULL, NULL);
                PRINT_EXIT_STATUS(status);
                return 1;    
            }
        }
        
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
            while(argc--) free_malloced_str(argv[argc]);
            return !res;
        }
        
        /*
         *  for all builtins, exept 'exec', we'll save (and later restore) the standard
         * input/output/error streams.
         */
        int savestd = (argc && strcmp(argv[0], "exec"));
        
        /* perform I/O redirection, if any */
        if(total_redirects && !__redirect_do(io_files, savestd))
        {
            while(argc-- > 0) free_malloced_str(argv[argc]);
            return 0;
        }

        if(argc == 0)
        {
            if(total_redirects) exit(EXIT_SUCCESS);
            MERGE_GLOBAL_SYMTAB();
            return 1;
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
                while(argc--) free_malloced_str(argv[argc]);
                if(savestd && total_redirects) redirect_restore();
                if(exit_status) EXIT_IF_NONINTERACTIVE();
                return res;
            }
        }
        
        /* POSIX Command Search and Execution Algorithm:      */
        /* STEP 1: The command has no slash(es) in its name   */
        if(!strchr(argv[0], '/'))
        {
            /* STEP 1-A: check for special builtin utilities      */
            if(do_special_builtin(argc, argv))
            {
                int res = 1;
                i = 0;
                if(((strcmp(argv[0], "break"   ) == 0) ||
                    (strcmp(argv[0], "continue") == 0) ||
                    (strcmp(argv[0], "return"  ) == 0)))
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
                while(argc--) free_malloced_str(argv[argc]);
                //restore_std();
                if(savestd && total_redirects) redirect_restore();
                MERGE_GLOBAL_SYMTAB();
                /* 
                 * non-interactive shells exit if a special builtin returned non-zero 
                 * or error status (except if it is break, continue, or return).
                 */
                if(exit_status && i == 0) EXIT_IF_NONINTERACTIVE();
                return res;
            }
            /* STEP 1-B: check for internal functions             */
            if(do_function_definition(argc, argv))
            {
                while(argc--) free_malloced_str(argv[argc]);
                //restore_std();
                if(savestd && total_redirects) redirect_restore();
                MERGE_GLOBAL_SYMTAB();
                //free(symtab_stack_pop());
                return 1;
            }
            /* STEP 1-C: check for regular builtin utilities      */
            if(do_regular_builtin(argc, argv))
            {
                while(argc--) free_malloced_str(argv[argc]);
                //restore_std();
                if(savestd && total_redirects) redirect_restore();
                MERGE_GLOBAL_SYMTAB();
                return 1;
            }
            /* STEP 1-D: checked for in exec_cmd()                */
        }
        
        do_exec_cmd(argc, argv, NULL, NULL);
        /* NOTE: we should NEVER come back here, unless there is error of course!! */
        BACKEND_RAISE_ERROR(FAILED_TO_EXEC, argv[0], strerror(errno));
        if(errno == ENOEXEC) exit(EXIT_ERROR_NOEXEC);
        else if(errno == ENOENT) exit(EXIT_ERROR_NOENT);
        else exit(EXIT_FAILURE);
    }
    /* ... and parent countinues over here ...    */

    /* NOTE: we re-set the process group id here (and above in the child process) to make
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
    int   status = wait_on_child(child_pid, node, NULL);
    unblock_traps();

    /* reset the terminal's foreground pgid */
    if(option_set('m'))
    {
        tcsetpgrp(0, tty_pid);
    }
    PRINT_EXIT_STATUS(status);


    if(src->filename == stdin_filename)
    {
        term_canon(0);
        update_row_col();
    }


    /* if we forked, we didn't hash the utility's path in our
     * hashtable. if so, do it now.
     */
    int cmdfound = WIFEXITED(status) && WEXITSTATUS(status) != 126 && WEXITSTATUS(status) != 127;
    if(option_set('h') && cmdfound && dofork && argc)
    {
        if(!builtin && !function)
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
    
    /* check winsize and update $LINES and $COLUMNS (bash) after running external cmds */
    if(optionx_set(OPTION_CHECK_WINSIZE)) get_screen_size();
    
    set_underscore_val(argv[argc-1], 0);    /* last argument to previous command */
    while(argc > 0) free_malloced_str(argv[--argc]);
    return 1;
}


int  do_command(struct node_s *node, struct node_s *redirect_list, int fork)
{
    if(!node) return 0;
    else if(node->type == NODE_COMMAND )
    {
        int res = do_simple_command(node, redirect_list, fork);
        return res;
    }
    else if(node->type == NODE_FUNCTION)
    {
        /*
        return do_function_definition(node, redirect_list);
        */
        return do_simple_command(node, redirect_list, fork);
    }
    else if(node->type == NODE_TIME)
    {
        return __time(node->first_child);
    }
    return do_compound_command(node, redirect_list);
}


int  do_translation_unit(struct node_s *node)
{
    if(!node) return 0;
    struct node_s *child = node;
    if(node->type == NODE_PROGRAM) child = child->first_child;
    while(child)
    {
        if(!do_complete_command(child))
        {
            /* we got return stmt */
            if(return_set)
            {
                return_set = 0;
                /*
                 * TODO: we should return from dot files AND functions.
                 *       calling return outside any function/script should
                 *       cause the shell to exit.
                 */
                if(src->filename == dot_filename) break;

                if(src->filename == stdin_filename ||
                   src->filename == cmdstr_filename)
                    exit_gracefully(exit_status, NULL);
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
        if(option_set('t')) exit_gracefully(exit_status, NULL);
        child = child->next_sibling;
    }
    fflush(stdout);
    fflush(stderr);
    SIGINT_received = 0;
    return 0;
}
