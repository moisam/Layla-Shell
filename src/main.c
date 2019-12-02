/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: main.c
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

/* macro definition needed to use setenv() */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <sys/wait.h>
#include "cmd.h"
#include "debug.h"
#include "backend/backend.h"
#include "symtab/symtab.h"
#include "builtins/setx.h"

/* process group id of the command line interpreter */
pid_t  tty_pid;

/* flag to indicate if we are reading from stdin */
int    read_stdin       = 1;

/* flag to break out of loops when SIGINT is received */
int    SIGINT_received  = 0;

/* flag to indicate a signal was received and handled by trap_handler() */
int    signal_received  = 0;

#define CLOCKID CLOCK_REALTIME

/* defined in cmdline.c */
void kill_input();
extern int do_periodic;

/* defined in initsh.c */
extern int   noprofile;        /* if set, do not load login scripts */
extern int   norc     ;        /* if set, do not load rc scripts */

/* defined in backend/backend.c */
extern int cur_loop_level;
extern int req_break     ;


/*
 * signal handler for SIGALRM (alarm signal). we use this signal to inform us
 * when a period of $TPERIOD minutes has elapsed so that we can execute the 'periodic'
 * special alias.
 */
static void SIGALRM_handler(int sig __attribute__((unused)),
                            siginfo_t *si __attribute__((unused)),
                            void *uc __attribute__((unused)))
{
    do_periodic = 1;
}


/*
 * signal handler for SIGINT (interrupt signal).
 */
void SIGINT_handler(int signum __attribute__((unused)))
{
    SIGINT_received = 1;
    signal_received = 1;
    /* force break from any running loop */
    if(cur_loop_level)
    {
        req_break = 1;
    }
}


/*
 * signal handler for SIGQUIT (quit signal).
 */
void SIGQUIT_handler(int signum)
{
    fprintf(stderr, "%s: received signal %d\n", SHELL_NAME, signum);
    signal_received = 1;
}


/*
 * signal handler for SIGWINCH (window size change signal).
 */
void SIGWINCH_handler(int signum)
{
    fprintf(stderr, "%s: received signal %d\n", SHELL_NAME, signum);
    get_screen_size();
}


/*
 * signal handler for SIGCHLD (child status change signal).
 */
void SIGCHLD_handler(int signum __attribute__((unused)))
{
    int pid, status, save_errno = errno;
    status = 0;
    while(1)
    {
        status = 0;
        pid = waitpid(-1, &status, WUNTRACED|WNOHANG);
        //if(WIFEXITED(status)) status = WEXITSTATUS(status);
        if(pid <= 0)
        {
            break;
        }
        notice_termination(pid, status);
        /* tcsh extensions */
        if(optionx_set(OPTION_LIST_JOBS_LONG))
        {
            jobs_builtin(2, (char *[]){ "jobs", "-l" });
        }
        else if(optionx_set(OPTION_LIST_JOBS))
        {
            jobs_builtin(1, (char *[]){ "jobs"       });
        }
        /* in tcsh, special alias jobcmd is run before running commands and when jobs change state */
        run_alias_cmd("jobcmd");
    }
    errno = save_errno;
}


/*
 * signal handler for SIGHUP (hangup signal).
 */
void SIGHUP_handler(int signum)
{
    /* only interactive shells worry about killing their child jobs */
    if(option_set('i'))
    {
        kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
    }
    exit_gracefully(signum+128, NULL);
}


/*
 * set the signal handler function for signal number signum to sa_handler().
 */
int set_signal_handler(int signum, void (handler)(int))
{
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = handler;
    return sigaction(signum, &sigact, NULL);
}


/*
 * main shell entry point.
 */
int main(int argc, char **argv)
{
    /*************************************
     *************************************
     * Shell initialization begins here
     *************************************
     *************************************/
    shell_argc = argc;
    shell_argv = argv;
    tty_pid    = getpid();
    setlocale(LC_ALL, "");

    /* init global symbol table */
    init_symtab();

    /* init utility names hashtable */
    init_utility_hashtable();

    /* init strings hashtable */
    init_str_hashtable();
    
    /* init the functions table */
    init_functab();
    
    /* init traps table */
    init_traps();

    /* get the umask */
    init_umask();
    
    /* init aliases */
    memset(aliases, 0, sizeof(aliases));

    /*
     * if we have a script file name or a command string passed to us, initsh() will
     * load it into the following source struct.
     */
    struct source_s src;
    memset(&src, 0, sizeof(struct source_s));

    /* parse the command line options, if any */
    int islogin = parse_shell_args(argc, argv, &src);
    set_option('L', islogin ? 1 : 0);
    if(islogin)
    {
        set_optionx(OPTION_LOGIN_SHELL, 1);
        read_dirsfile = 1;
        /* so that we will automatically save the dirstack on logout */
        set_optionx(OPTION_SAVE_DIRS, 1);
    }
    
    /* 
     * $SHELLOPTS contains a colon-separated list of options to set.
     * bash uses this variable and sets the options before reading any
     * startup files if the shell is not restricted. as we've mangled our
     * $SHELLOPTS variable by parsing options in the inish.c file, we'll
     * use the SHELLOPTS environment variable, which should contain any options
     * specified by the user before calling us.
     */
    char *s = getenv("SHELLOPTS");
    if(s && !option_set('r'))
    {
        char *s1 = s;
        while(*s1)
        {
            while(*s1 && *s1 == ':')
            {
                s1++;
            }
            char *s2 = s1+1;
            while(*s2 && *s2 != ':')
            {
                s2++;
            }
            char c = *s2;
            *s2 = '\0';
            do_options("-o", s1);
            *s2 = c;
            s1 = s2;
        }
        symtab_save_options();
    }
    
    /* 
     * save the signals we got from our parent, so that we can reset 
     * them in our children later on.
     * NOTE: if you changed any of the signal actions below, make sure to mirror
     *       those changes in trap.c in the case switch where we reset signal actions
     *       to their default values. don't also forget the processing of the -q option
     *       down below, which affects SIGQUIT.
     */
    save_signals();    

    /* if interactive shell ... */
    if(option_set('i'))
    {
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        if(option_set('m'))
        {
            signal(SIGTSTP, SIG_IGN);
            signal(SIGTTIN, SIG_IGN);
            signal(SIGTTOU, SIG_IGN);
        }
        set_signal_handler(SIGCHLD , SIGCHLD_handler );
        set_signal_handler(SIGINT  , SIGINT_handler  );
        set_signal_handler(SIGWINCH, SIGWINCH_handler);
        set_optionx(OPTION_INTERACTIVE_COMMENTS, 1);
        
        /*
         * set a special timer for handling the $TPERIOD variable, which causes
         * the 'periodic' alias to be executed at certain intervals (tcsh extension).
         */
        struct sigevent sev;
        struct itimerspec its;
        int freq = get_shell_vari("TPERIOD", 0);
        struct sigaction sa;

        /* Establish handler for timer signal */
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = SIGALRM_handler;
        sigemptyset(&sa.sa_mask);
        if(sigaction(SIGALRM, &sa, NULL) == -1)
        {
            fprintf(stderr, "%s: failed to catch SIGALRM: %s\n", SHELL_NAME, strerror(errno));
        }

        /* Create the timer */
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGALRM;
        sev.sigev_value.sival_ptr = &timerid;
        if(timer_create(CLOCKID, &sev, &timerid) == -1)
        {
            fprintf(stderr, "%s: failed to create timer: %s\n", SHELL_NAME, strerror(errno));
        }
        
        /* Start the timer ($TPERIOD is in minutes) */
        if(freq > 0)
        {
            its.it_value.tv_sec = freq*60;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_sec = its.it_value.tv_sec;
            its.it_interval.tv_nsec = its.it_value.tv_nsec;
            if(timer_settime(timerid, 0, &its, NULL) == -1)
            {
               fprintf(stderr, "%s: failed to start timer: %s\n", SHELL_NAME, strerror(errno));
            }
        }

        /*
         * speed up the startup of subshells by omitting some features that are used
         * for interactive shells, like command history, aliases and dirstacks. in this
         * case, we only init these features if this is NOT a subshell.
         */

        /* init history */
        init_history();

        /* init the directory stack */
        init_dirstack();

        /* get us some useful alias definitions */
        init_aliases();
    }
    else
    {
        /* turn off options we don't need in a (non-interactive) subshell */
        set_optionx(OPTION_EXPAND_ALIASES      , 0);
        set_optionx(OPTION_SAVE_HIST           , 0);
    }

    set_signal_handler(SIGHUP, SIGHUP_handler);

    /* not in privileged mode? reset ids (bash) */
    if(!option_set('p'))
    {
        uid_t euid = geteuid(), ruid = getuid();
        gid_t egid = getegid(), rgid = getgid();
        if(euid != ruid)
        {
            seteuid(ruid);
        }
        if(egid != rgid)
        {
            setegid(rgid);
        }
        /* bash doesn't read startup files in this case */
        noprofile = 1;
        norc      = 1;
    }
    
    /*
     * we check to see if this is a login shell. If so, read /etc/profile
     * and then ~/.profile.
     */
    if(islogin)
    {
        init_login();
    }

    /*
     * check for and execute $ENV file, if any (if not in privileged mode).
     * if not privileged, ksh also uses /etc/suid_profile instead of $ENV
     * (but we pass this one).
     */
    if(option_set('i'))
    {
        init_rc();
    }
    
    /*
     * the restricted mode '-r' is enabled in initsh() below, after $ENV and .profile
     * scripts have been read and executed (above). this is in conformance
     * to ksh behaviour.
     */


    /* init environ and finish initialization */
    initsh(argc, argv, read_stdin);
    
    /*
     * tcsh accepts -q, which causes SIGQUIT to be caught and job control to be
     * disabled.
     */
    if(option_set('q'))
    {
        set_signal_handler(SIGQUIT, SIGQUIT_handler);
        set_option('m', 0);
    }
    
    /* init our internal clock */
    start_clock();
    
    /* seed the random number generator */
    init_rand();
    
    /* only set $PPID if this is not a subshell */
    if(subshell_level == 0)
    {
        pid_t ppid = getppid();
        char ppid_str[10];
        sprintf(ppid_str, "%u", ppid);
        setenv("PPID", ppid_str, 1);
        struct symtab_entry_s *entry = add_to_symtab("PPID");
        if(entry)
        {
            symtab_entry_setval(entry, ppid_str);
            entry->flags |= FLAG_READONLY;
        }
    }

    /* disable some builtin extensions in the posix '-P' mode */
    if(option_set('P'))
    {
        /* special builtins */
        special_builtins[SPECIAL_BUILTIN_LOCAL    ].flags &= ~BUILTIN_ENABLED;
        special_builtins[SPECIAL_BUILTIN_LOGOUT   ].flags &= ~BUILTIN_ENABLED;
        special_builtins[SPECIAL_BUILTIN_REPEAT   ].flags &= ~BUILTIN_ENABLED;
        special_builtins[SPECIAL_BUILTIN_SETX     ].flags &= ~BUILTIN_ENABLED;
        special_builtins[SPECIAL_BUILTIN_SUSPEND  ].flags &= ~BUILTIN_ENABLED;
        /* regular builtins */
        regular_builtins[REGULAR_BUILTIN_BUGREPORT].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_BUILTIN  ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_CALLER   ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_COPROC   ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_DECLARE  ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_DIRS     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_DISOWN   ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_DUMP     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_ECHO     ].flags &= ~BUILTIN_ENABLED;
        /*
         * we won't disable the 'enable' builtin so the user can selectively enable builtins
         * when they're in the POSIX mode. if you insist on disabling ALL non-POSIX builtins,
         * uncomment the next line.
         */
        //regular_builtins[REGULAR_BUILTIN_ENABLE   ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_GLOB     ].flags &= ~BUILTIN_ENABLED;
        /*
         * the 'help' builtin should also be available, even in POSIX mode. if you want to
         * disable it in POSIX mode, uncomment the next line.
         */
        //regular_builtins[REGULAR_BUILTIN_HELP     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_HISTORY  ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_HUP      ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_LET      ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_MAIL     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_MEMUSAGE ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_NICE     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_NOHUP    ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_NOTIFY   ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_POPD     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_PRINTENV ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_PUSHD    ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_SETENV   ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_STOP     ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_UNLIMIT  ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_UNSETENV ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_VER      ].flags &= ~BUILTIN_ENABLED;
        regular_builtins[REGULAR_BUILTIN_WHENCE   ].flags &= ~BUILTIN_ENABLED;
    }

    /* set the exit status to 0 */
    exit_status = EXIT_SUCCESS;

    /* now we can enforce the restricted mode, as startup scripts are all read and executed */
    startup_finished = 1;
    
    /* main program loop */
    if(read_stdin)
    {
        cmdline();
    }
    else
    {
        parse_and_execute(&src);
    }
    //exit_gracefully(exit_status, NULL);
    exit_builtin(1, (char *[]){ "exit", NULL });
}


#define SAVE_TO_HISTORY_AND_PRINT(cmd_node)     \
do {                                            \
    if(save_hist)                               \
    {                                           \
        save_to_history((cmd_node));            \
    }                                           \
    if(option_set('v'))                         \
    {                                           \
        fprintf(stderr, "%s\n", (cmd_node));    \
    }                                           \
} while(0)


/*
 * parse and execute the translation unit we have in the passed source_s struct.
 * 
 * returns 1.
 */
int parse_and_execute(struct source_s *src)
{
    eof_token.src = src;

    /*
     * parse and execute the translation unit, one command at a time.
     */

    /* skip any leading whitespace chars */
    skip_white_spaces(src);

    /* save the start of this line */
    src->wstart = src->curpos;

    /*
     * the -n option means read commands but don't execute them.
     * only effective in non-interactive shells (POSIX says interactive shells
     * may safely ignore it). this option is good for checking a script for
     * syntax errors.
     */
    int noexec = (option_set('n') && !option_set('i'));
    int i      = src->curpos;
    int res    = 1;             /* the result of parsing/executing */
    struct token_s  *tok  = tokenize(src);

    /* skip any leading comments/newlines */
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
    {
        /* skip comments and newlines */
        if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
        {
            i = src->curpos;
            /* save the start of this line */
            src->wstart = src->curpos;
            tok = tokenize(tok->src);
        }
        else
        {
            break;
        }
    }

    /* reached EOF or error getting next token */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        return 0;
    }

    /* first time ever? keep a track of the first char of the current command line */
    if(i < 0)
    {
        i = 0;
    }

    /*
     * determine if we're going to save commands to the history list.
     * we save history commands when the shell is interactive and we're reading
     * from stdin.
     */
    int save_hist = option_set('i') && src->srctype == SOURCE_STDIN;

    /* clear the parser's error flag */
    parser_err = 0;

    /* restore the terminal's canonical mode if needed */
    if(read_stdin)
    {
        term_canon(1);
    }

    /* loop parsing and executing commands */
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
    {
        /* parse the next command */
        struct node_s *cmd = parse_complete_command(tok);
        tok = get_current_token();

        /* parser encountered an error */
        if(parser_err)
        {
            if(cmd)
            {
                free_node_tree(cmd);
            }
            res = 0;
            break;
        }

        /* input consisted of empty lines and/or comments with no commands */
        if(!cmd)
        {
            break;
        }

        /* we've got a command. now are we going to execute it? */
        if(noexec)
        {
            free_node_tree(cmd);
            continue;
        }

        if(!cmd->lineno)
        {
            cmd->lineno = src->curline;
        }

        /* add command to the history list and echo it (if -v option is set) */
        switch(cmd->type)
        {
            case NODE_TIME:
                /* the real command is the first child of the 'time' node */
                if(cmd->first_child)
                {
                    cmd = cmd->first_child;
                    /* fall through to the next case */
                }
                else
                {
                    /* 'time' word with no timed command */
                    SAVE_TO_HISTORY_AND_PRINT("time");
                    break;
                }
                /* fall through to the next case */
                __attribute__((fallthrough));

            case NODE_COMMAND:
            case NODE_LIST:
                if(cmd->val.str)
                {
                    SAVE_TO_HISTORY_AND_PRINT(cmd->val.str);
                    break;
                }
                /* fall through to the next case */
                __attribute__((fallthrough));

            default:
                ;
                int j = src->curpos-(tok->text_len);
                while(src->buffer[j] == '\n')
                {
                    j--;
                }
                /* copy command line to buffer */
                char buf[j-i+1];
                int k = 0;
                do
                {
                    buf[k++] = src->buffer[i++];
                } while(i < j);
                buf[k] = '\0';
                SAVE_TO_HISTORY_AND_PRINT(buf);
                break;
        }
        
        /*
         * dump the Abstract Source Tree (AST) of this translation unit.
         * note that we are using an extended option '-d' which is not
         * defined by POSIX.
         */
        if(option_set('d'))
        {
            dump_node_tree(cmd, 1);
        }

        /* now execute the command */
        if(!do_complete_command(src, cmd))
        {
            /* we've got a return statement */
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
            }
            /* failed to execute command. bail out */
            else
            {
                res = 0;
            }
            free_node_tree(cmd);
            break;
        }
        free_node_tree(cmd);
        fflush(stdout);
        fflush(stderr);

        /*
         * POSIX does not specify the -t (or onecmd) option, as it says it is
         * mainly used with here-documents. this flag causes the shell to read
         * and execute only one command before exiting.. it is not clear what
         * exactly constitutes 'one command'.. here, we just execute the first
         * node tree we've got and exit.
         */
        if(option_set('t'))
        {
            exit_gracefully(exit_status, NULL);
        }

        /*
         * prepare for parsing the next command.
         * skip optional newline and comment tokens.
         */
        while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
        {
            if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
            {
                tok = tokenize(tok->src);
            }
            else
            {
                break;
            }
        }

        /* reached EOF or error getting next token */
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            break;
        }

        /* keep a track of the first char of the current command line */
        i = src->curpos-(tok->text_len);
        while(src->buffer[i] == '\n')
        {
            i++;
        }
        /* save the start of this line */
        src->wstart = src->curpos-(tok->text_len);
    }

    /* don't leave any hanging token structs */
    free_token(get_current_token());
    free_token(get_previous_token());

    /* finished parsing and executing commands */
    fflush(stdout);
    fflush(stderr);
    SIGINT_received = 0;

    /* restore the terminal's non-canonical mode if needed */
    if(read_stdin)
    {
        term_canon(0);
        update_row_col();
    }

    return res;
}


/*
 * read a file (presumably a script file) and initialize the
 * source_s struct so that we can parse and execute the file.
 * 
 * NOTE: this function doesn't handle big files.. we should extend
 *       it so it uses memmaped files or any other method for handling
 *       big files.
 * 
 * returns 1 if the file is loaded successfully, 0 otherwise.
 */
int read_file(char *__filename, struct source_s *src)
{
    errno = 0;
    if(!__filename)
    {
        return 0;
    }
    char *filename = word_expand_to_str(__filename);
    if(!filename)
    {
        return 0;
    }
    char *tmpbuf = NULL;
    FILE *f      = NULL;
    if(strchr(filename, '/'))
    {
        /* pathname with '/', try opening it */
        f = fopen(filename, "r");
    }
    else
    {
        /* pathname with no slashes, try to locate it */
        size_t len = strlen(filename);
        /* try CWD */
        char tmp[len+3];
        strcpy(tmp, "./");
        strcat(tmp, filename);
        f = fopen(tmp, "r");
        if(f)   /* file found */
        {
            goto read;
        }
        /* search using $PATH */
        char *path = search_path(filename, NULL, 0);
        if(!path)   /* file not found */
        {
            free(filename);
            return 0;
        }
        f = fopen(path, "r");
        free_malloced_str(path);
        if(f)
        {
            goto read;
        }
        /* failed to open the file */
        free(filename);
        return 0;
    }
    /* failed to open the file */
    if(!f)
    {
        goto error;
    }
    
read:
    /* seek to the end of the file */
    if(fseek(f, 0, SEEK_END) != 0)
    {
        goto error;
    }
    /* get the file length */
    long i;
    if((i = ftell(f)) == -1)
    {
        goto error;
    }
    rewind(f);
    /* alloc buffer */
    tmpbuf = malloc(i+1);
    if(!tmpbuf)
    {
        goto error;
    }
    /* read the file */
    long j = fread(tmpbuf, 1, i, f);
    if(j != i)
    {
        goto error;
    }
    tmpbuf[i]     = '\0';
    fclose(f);
    src->buffer   = tmpbuf;
    src->bufsize  = i;
    src->srctype  = SOURCE_EXTERNAL_FILE;
    src->srcname  = get_malloced_str(filename);
    free(filename);
    src->curpos   = INIT_SRC_POS;
    //free(filename);
    return 1;
    
error:
    if(f)
    {
        fclose(f);
    }
    if(tmpbuf)
    {
        free(tmpbuf);
    }
    free(filename);
    return 0;
}
