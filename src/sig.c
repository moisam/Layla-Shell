/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: sig.c
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

/* required macro definition for sig*() functions */
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include "cmd.h"
#include "sig.h"
#include "builtins/builtins.h"
#include "builtins/setx.h"
#include "backend/backend.h"
#include "debug.h"

#define CLOCKID CLOCK_REALTIME

/* flag to indicate a signal was received and handled by trap_handler() */
int signal_received = 0;

/* defined in cmdline.c */
void kill_input();
extern int do_periodic;


/*
 * NOTE: these signal names are ordered such that the array's
 *       index of a signal is the value of that signal. that is,
 *       SIGHUP has the numeric value of 1, SIGINT of 2, ...
 *       These signal values (0-31) are Linux-centric. If you are
 *       compiling this software for another platform, you need
 *       to check your system's signal.h (or whatever that's called
 *       on your OS) to make sure you got the correct signal numbers.
 */

char *signames[] =
{
    "NULL"     ,
    "SIGHUP"   ,	/* Hangup (POSIX).                        -  1 */
    "SIGINT"   ,	/* Interrupt (ANSI).                      -  2 */
    "SIGQUIT"  ,	/* Quit (POSIX).                          -  3 */
    "SIGILL"   ,	/* Illegal instruction (ANSI).            -  4 */
    "SIGTRAP"  ,	/* Trace trap (POSIX).                    -  5 */
    "SIGABRT"  ,	/* Abort (ANSI).                          -  6 */
    "SIGBUS"   ,	/* BUS error (4.2 BSD).                   -  7 */
    "SIGFPE"   ,	/* Floating-point exception (ANSI).       -  8 */
    "SIGKILL"  ,	/* Kill, unblockable (POSIX).             -  9 */
    "SIGUSR1"  ,	/* User-defined signal 1 (POSIX).         - 10 */
    "SIGSEGV"  ,	/* Segmentation violation (ANSI).         - 11 */
    "SIGUSR2"  ,	/* User-defined signal 2 (POSIX).         - 12 */
    "SIGPIPE"  ,	/* Broken pipe (POSIX).                   - 13 */
    "SIGALRM"  ,	/* Alarm clock (POSIX).                   - 14 */
    "SIGTERM"  ,	/* Termination (ANSI).                    - 15 */
    "SIGSTKFLT",	/* Stack fault.                           - 16 */
    "SIGCHLD"  ,	/* Child status has changed (POSIX).      - 17 */
    "SIGCONT"  ,	/* Continue (POSIX).                      - 18 */
    "SIGSTOP"  ,	/* Stop, unblockable (POSIX).             - 19 */
    "SIGTSTP"  ,	/* Keyboard stop (POSIX).                 - 20 */
    "SIGTTIN"  ,	/* Background read from tty (POSIX).      - 21 */
    "SIGTTOU"  ,	/* Background write to tty (POSIX).       - 22 */
    "SIGURG"   ,	/* Urgent condition on socket (4.2 BSD).  - 23 */
    "SIGXCPU"  ,	/* CPU limit exceeded (4.2 BSD).          - 24 */
    "SIGXFSZ"  ,	/* File size limit exceeded (4.2 BSD).    - 25 */
    "SIGVTALRM",	/* Virtual alarm clock (4.2 BSD).         - 26 */
    "SIGPROF"  ,	/* Profiling alarm clock (4.2 BSD).       - 27 */
    "SIGWINCH" ,	/* Window size change (4.3 BSD, Sun).          */
    "SIGIO"    ,	/* I/O now possible (4.2 BSD).            - 29 */
    "SIGPWR"   ,	/* Power failure restart (System V).      - 30 */
    "SIGSYS"   ,	/* Bad system call.                       - 31 */
};

struct sigaction signal_handlers[SIGNAL_COUNT];


/*
 * save the default sigaction for each signal.
 */
void save_signals(void)
{
    int i = 1;
    for( ; i < SIGNAL_COUNT; i++)
    {
        sigaction(i, NULL, &signal_handlers[i]);
    }
}


/*
 * restore the default sigaction for each signal (called on exec).
 */
void restore_signals(void)
{
    int i = 1;
    for( ; i < SIGNAL_COUNT; i++)
    {
        sigaction(i, &signal_handlers[i], NULL);
    }
}


/*
 * return the sigaction for the given signal.
 */
struct sigaction *get_sigaction(int signum)
{
    if(signum <= 0 || signum >= SIGNAL_COUNT)
    {
        return NULL;
    }
    return &signal_handlers[signum];
}


/*
 * initialize the shell's signal handlers.
 */
void init_signals(void)
{
    if(interactive_shell)
    {
        set_signal_handler(SIGTERM , SIG_IGN);
        if(option_set('m'))
        {
            set_signal_handler(SIGTSTP , SIG_IGN);
            set_signal_handler(SIGTTIN , SIG_IGN);
            set_signal_handler(SIGTTOU , SIG_IGN);
        }
        set_signal_handler(SIGINT  , SIGINT_handler  );
        set_signal_handler(SIGWINCH, SIGWINCH_handler);
        set_optionx(OPTION_INTERACTIVE_COMMENTS, 1);
        set_SIGALRM_handler();
    }

    set_signal_handler(SIGCHLD, SIGCHLD_handler);
    set_signal_handler(SIGHUP , SIGHUP_handler );
    set_SIGQUIT_handler();
}


/*
 * set SIGQUIT's signal handler properly.
 */
void set_SIGQUIT_handler(void)
{
    /*
     * tcsh accepts -q, which causes SIGQUIT to be caught and job control to be
     * disabled.
     */
    if(option_set('q'))
    {
        set_signal_handler(SIGQUIT, SIGQUIT_handler);
        set_option('m', 0);
        /* update the options string */
        symtab_save_options();
    }
    else
    {
        set_signal_handler(SIGQUIT , SIG_IGN);
    }
}


/*
 * set SIGALRM's signal handler properly.
 */
void set_SIGALRM_handler(void)
{
    /*
     * set a special timer for handling the $TPERIOD variable, which causes
     * the 'periodic' alias to be executed at certain intervals (tcsh extension).
     */
    struct sigevent sev;
    struct itimerspec its;
    int freq = get_shell_vari("TPERIOD", 0);
    struct sigaction sa;

    /* establish handler for timer signal */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SIGALRM_handler;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGALRM, &sa, NULL) == -1)
    {
        PRINT_ERROR("%s: failed to catch SIGALRM: %s\n", SHELL_NAME, strerror(errno));
    }

    /* create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = &timerid;
    if(timer_create(CLOCKID, &sev, &timerid) == -1)
    {
        PRINT_ERROR("%s: failed to create timer: %s\n", SHELL_NAME, strerror(errno));
    }
        
    /* start the timer ($TPERIOD is in minutes) */
    if(freq > 0)
    {
        its.it_value.tv_sec = freq*60;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;
        if(timer_settime(timerid, 0, &its, NULL) == -1)
        {
            PRINT_ERROR("%s: failed to start timer: %s\n", SHELL_NAME, strerror(errno));
        }
    }
}


/*
 * signal handler for SIGALRM (alarm signal). we use this signal to inform us
 * when a period of $TPERIOD minutes has elapsed so that we can execute the 'periodic'
 * special alias.
 */
void SIGALRM_handler(int sig __attribute__((unused)),
                     siginfo_t *si __attribute__((unused)),
                     void *uc __attribute__((unused)))
{
    do_periodic = 1;
}


/*
 * signal handler for SIGINT (interrupt signal).
 */
void SIGINT_handler(int signum)
{
    signal_received = signum;
    /* force break from any running loop */
    req_break = cur_loop_level;
}


/*
 * signal handler for SIGQUIT (quit signal).
 */
void SIGQUIT_handler(int signum)
{
    fprintf(stderr, "%s: received signal %d\n", SHELL_NAME, signum);
    signal_received = signum;
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
void SIGCHLD_handler(int signum)
{
    int pid, status, save_errno = errno;
    status = 0;
    while(1)
    {
        status = 0;
        pid = waitpid(-1, &status, WUNTRACED|WCONTINUED|WNOHANG);
        //if(WIFEXITED(status)) status = WEXITSTATUS(status);

        if(pid <= 0)
        {
            break;
        }
        notice_termination(pid, status, 1);

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
    signal_received = signum;
}


/*
 * signal handler for SIGHUP (hangup signal).
 */
void SIGHUP_handler(int signum)
{
    kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
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
