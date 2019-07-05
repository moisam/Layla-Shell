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
struct source_s  __src;
struct source_s *src = &__src;
char  *cmdline_filename = "CMDLINE";
char  *cmdstr_filename  = "CMDSTR" ;
int    read_stdin       = 1;
int    SIGINT_received  = 0;    /* used to break out of loops when SIGINT is received */

// FILE  *STDIN  = NULL;
// FILE  *STDOUT = NULL;
// FILE  *STDERR = NULL;

#define CLOCKID CLOCK_REALTIME

/* defined in cmdline.c */
void kill_input();
extern int do_periodic;

/* defined in initsh.c */
extern int   noprofile;        /* do not load login scripts */
extern int   norc     ;        /* do not load rc scripts */

/* defined in backend/backend.c */
extern int cur_loop_level;
extern int req_break     ;


static void SIGALRM_handler(int sig __attribute__((unused)), siginfo_t *si __attribute__((unused)), void *uc __attribute__((unused)))
{
    do_periodic = 1;
}

void SIGINT_handler(int signum __attribute__((unused)))
{
    SIGINT_received = 1;
    if(option_set('i')) kill_input();
    
    /* force break from any running loop */
    if(cur_loop_level)
    {
        req_break = 1;
    }
    /* make sure we're the signal handler so no funny business happens */
    signal(SIGINT, SIGINT_handler);
}

void SIGQUIT_handler(int signum)
{
    fprintf(stderr, "%s: received signal %d\r\n", SHELL_NAME, signum);
    /* make sure we're the signal handler so no funny business happens */
    signal(SIGQUIT, SIGQUIT_handler);
}

void SIGWINCH_handler(int signum)
{
    fprintf(stderr, "%s: received signal %d\r\n", SHELL_NAME, signum);
    get_screen_size();
    /* make sure we're the signal handler so no funny business happens */
    signal(SIGWINCH, SIGWINCH_handler);
}

void SIGCHLD_handler(int signum __attribute__((unused)))
{
    int pid, status, save_errno = errno;
    status = 0;
    while(1)
    {
        status = 0;
        pid = waitpid(-1, &status, WAIT_FLAG|WNOHANG);
        //if(WIFEXITED(status)) status = WEXITSTATUS(status);
        if(pid < 0)
        {
            //perror("waitpid");
            break;
        }
        if(pid == 0) break;
        /*
         * don't notify foreground job completions, as they will be waited for in 
         * wait_on_child() in backend/backend.c.
         */

        notice_termination(pid, status);
        /* tcsh extensions */
        if(optionx_set(OPTION_LIST_JOBS_LONG)) jobs(2, (char *[]){ "jobs", "-l" });
        else if(optionx_set(OPTION_LIST_JOBS)) jobs(1, (char *[]){ "jobs"       });
        /* in tcsh, special alias jobcmd is run before running commands and when jobs change state */
        run_alias_cmd("jobcmd");
    }
    errno = save_errno;
    /* make sure we're the signal handler so no funny business happens */
    signal(SIGCHLD, SIGCHLD_handler);
}

void SIGHUP_handler(int signum)
{
    /* only interactive shells worry about killing their child jobs */
    if(option_set('i'))
    {
        kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
    }
    exit_gracefully(signum+128, NULL);
}


int main(int argc, char **argv)
{
    
    /*
     * NOTE: check the name by which this executable was called.
     *       if the caller wants one of the regular builtin utilities
     *       (e.g., alias, cd, fg, bg, ...), then redirect argc & argv
     *       to the respective function and then exit.
     *       otherwise carry on with the shell loading as usual.
     */
    char     *invok = argv[0];
    char     *slash = strrchr(invok, '/');
    if(slash) invok = slash+1;
    size_t   cmdlen = strlen(invok);
    int      j;
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(strlen(regular_builtins[j].name) != cmdlen) continue;
        if(strcmp(regular_builtins[j].name, invok) == 0)
        {
            int (*func)(int, char **) = (int (*)(int, char **))regular_builtins[j].func;
            return do_exec_cmd(argc, argv, NULL, func);
        }
    }
        

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
    
    /* parse the command line options, if any */
    char islogin = parse_options(argc, argv);
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
     * startup files if the shell is not restricted.
     */
    char *s = getenv("SHELLOPTS");
    if(s && !option_set('r'))
    {
        char *s1 = s;
        while(*s1)
        {
            char *s2 = s1+1;
            while(*s2 && *s2 != ':') s2++;
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
        signal(SIGCHLD , SIGCHLD_handler );
        signal(SIGINT  , SIGINT_handler  );
        signal(SIGWINCH, SIGWINCH_handler);
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
            fprintf(stderr, "%s: failed to catch SIGALRM: %s\r\n", SHELL_NAME, strerror(errno));
        }

        /* Create the timer */
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGALRM;
        sev.sigev_value.sival_ptr = &timerid;
        if(timer_create(CLOCKID, &sev, &timerid) == -1)
        {
            fprintf(stderr, "%s: failed to create timer: %s\r\n", SHELL_NAME, strerror(errno));
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
               fprintf(stderr, "%s: failed to start timer: %s\r\n", SHELL_NAME, strerror(errno));
            }
        }
    }
    signal(SIGHUP , SIGHUP_handler);

    /* not in privileged mode? reset ids (bash) */
    if(!option_set('p'))
    {
        uid_t euid = geteuid(), ruid = getuid();
        gid_t egid = getegid(), rgid = getgid();
        if(euid != ruid) seteuid(ruid);
        if(egid != rgid) setegid(rgid);
        /* bash doesn't read startup files in this case */
        noprofile = 1;
        norc      = 1;
    }
    
    /*
     * we check to see if this is a login shell. If so, read /etc/profile
     * and then ~/.profile.
     */
    if(islogin) init_login();

    /*
     * check for and execute $ENV file, if any (if not in privileged mode).
     * if not privileged, ksh also uses /etc/suid_profile instead of $ENV
     * (but we pass this one).
     */
    if(option_set('i')) init_rc();
    
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
        signal(SIGQUIT, SIGQUIT_handler);
        set_option('m', 0);
    }

  
    /* init aliases */
    memset((void *)__aliases, 0, sizeof(__aliases));
    
    /* init our internal clock */
    start_clock();

    
    /* seed the random number generator */
    init_rand();
    
    /* only set $PPID if this is not a subshell */
    if(subshell_level == 0)
    {
        pid_t ppid = getppid();
        char ppid_str[10];
        _itoa(ppid_str, ppid);
        setenv("PPID", ppid_str, 1);
        struct symtab_entry_s *entry = add_to_symtab("PPID");
        if(entry)
        {
            symtab_entry_setval(entry, ppid_str);
            entry->flags |= FLAG_READONLY;
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
    exit_status = EXIT_SUCCESS;
    
    /* main program loop */
    if(read_stdin)
    {
        cmdline();
    }
    else
    {
        do_cmd();
    }
    exit_gracefully(exit_status, NULL);
}


int do_cmd()
{
    struct node_s   *root   = (struct node_s *)NULL;
    eof_token.src           = src;
    root = parse_translation_unit();
    if(root)
    {
        /* dump the Abstract Source Tree (AST) of this translation unit.
         * note that we are using an extended option '-d' which is not
         * defined by POSIX.
         */
        if(option_set('d'))
            dump_node_tree(root, 1);
        
        /* -n option means read commands but don't execute them */
        int noexec = (option_set('n') && !option_set('i'));
        if(!noexec)
        {
            do_translation_unit(root);
            /* defined in initsh.c */
            extern char *stdin_filename;
            if(src->filename == stdin_filename) term_canon(0);
        }
        free_node_tree(root);
    }
    return 1;
}


int read_file(char *__filename, struct source_s *src)
{
    errno = 0;
    if(!__filename) return 0;
    char *filename = word_expand_to_str(__filename);
    if(!  filename) return 0;
    char *tmpbuf = (char *)NULL;
    FILE *f      = (FILE *)NULL;
    if(strchr(filename, '/'))
    {
        /* pathname with '/', try it */
        f = fopen(filename, "r");
    }
    else
    {
        size_t len = strlen(filename);
        /* try CWD */
        char tmp[len+3];
        strcpy(tmp, "./");
        strcat(tmp, filename);
        f = fopen(tmp, "r");
        if(f) goto read;
        /* search using $PATH */
        char *path = search_path(filename, NULL, 0);
        if(!path) return 0;
        f = fopen(path, "r");
        free_malloced_str(path);
        if(f) goto read;
        free(filename);
        return 0;
    }
    if(!f) goto error;
read:
    if(fseek(f, 0, SEEK_END) != 0) goto error;
    long i;
    if((i = ftell(f)) == -1) goto error;
    rewind(f);
    tmpbuf = (char *)malloc(i+1);
    if(!tmpbuf) goto error;
    long j = fread(tmpbuf, 1, i, f);
    if(j != i) goto error;
    tmpbuf[i]     = '\0';
    fclose(f);
    src->buffer   = tmpbuf;
    src->bufsize  = i;
    src->filename = filename;
    src->curpos   = -2;
    //free(filename);
    return 1;
error:
    if(f) fclose(f);
    if(tmpbuf) free(tmpbuf);
    free(filename);
    return 0;
}
