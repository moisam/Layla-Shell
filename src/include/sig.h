/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: sig.h
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

#ifndef SIGNAMES_H
#define SIGNAMES_H

#include <signal.h>

#define SIGNAL_COUNT      32

extern char *signames[];

/* block and unblock signals */
#define SIGNAL_BLOCK(signal, set)           \
do                                          \
{                                           \
    sigemptyset(&set);                      \
    sigaddset(&set, signal);                \
    sigprocmask(SIG_BLOCK, &set, NULL);     \
} while(0)


#define SIGNAL_UNBLOCK(set)                 \
do                                          \
{                                           \
    sigprocmask(SIG_UNBLOCK, &set, NULL);   \
} while(0)

/* functions to work with signals */
struct  sigaction *get_sigaction(int signum);
void    save_signals(void);
void    restore_signals(void);
void    init_signals(void);

int     set_signal_handler(int signum, void (handler)(int));
void    set_SIGQUIT_handler(void);
void    set_SIGALRM_handler(void);

void    SIGCHLD_handler(int signum);
void    SIGINT_handler(int signum);
void    SIGHUP_handler(int signum);
void    SIGWINCH_handler(int signum);
void    SIGQUIT_handler(int signum);
void    SIGALRM_handler(int sig __attribute__((unused)),
                        siginfo_t *si __attribute__((unused)),
                        void *uc __attribute__((unused)));

#endif
