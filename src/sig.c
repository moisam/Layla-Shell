/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: signames.c
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

#include <stdlib.h>
#include "sig.h"

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

struct sigaction signal_handlers[total_signames];
int    core_signals[] = { SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD };


/*
 * save the default sigaction for each signal.
 */
void save_signals()
{
    int i = 1;
    for( ; i < total_signames; i++)
    {
        sigaction(i, NULL, &signal_handlers[i]);
    }
}


/*
 * reset signal to the action it had when the shell started.
 */
void reset_signal(int signum)
{
    if(signum <= 0 || signum >= total_signames)
    {
        return;
    }
    sigaction(signum, &signal_handlers[signum], NULL);
}


/*
 * return the sigaction for the given signal.
 */
struct sigaction *get_sigaction(int signum)
{
    if(signum <= 0 || signum >= total_signames)
    {
        return NULL;
    }
    return &signal_handlers[signum];
}
