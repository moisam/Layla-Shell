/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: exit.c
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

/* macro definitions needed to use sig*() */
#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include "../cmd.h"
#include "../kbdevent.h"
#include "../symtab/symtab.h"
#include "setx.h"
#include "../symtab/string_hash.h"
#include "../debug.h"

/*
 * flag to let us know if the user has already tried to exit before.. we use this
 * when the user tries to exit while having running jobs.. in this case we print an
 * alert message and return (without exiting).. if the user re-runs `exit` immediately,
 * we kill all the jobs and continue with the exit.. if the user doesn't run `exit`
 * immediately, but runs any other command, the flag is cleared, so that when they run
 * `exit` again, the cycle repeats.
 */
int tried_exit = 0;

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;
extern struct termios tty_attr;

/* declared in main.c   */
extern int read_stdin;

/* declared in dirstack.c */
extern struct dirstack_ent_s *dirstack;

extern char *tok_buf;                   /* defined in lexical.c */
extern struct hashtab_s *str_hashes;    /* defined in strbuf.c  */


/*
 * the exit builtin utility (POSIX).. exits the shell, flusing command history to the
 * history file, and freeing used buffers.. doesn't return on success (the shell exits).
 * if passed an argument, it is regarded as the numeric exit status code we will pass
 * back to our parent process.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help exit` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int exit_builtin(int argc, char **argv)
{
    /* more than 2 args is an error */
    if(argc > 2)
    {
        fprintf(stderr, "exit: too many arguments\n");
        return 1;
    }

    /*
     *  if given 2 args, get the exit status code passed as argv[1] (we only need
     *  the lower 8 bits of it).. otherwise, we will use the exit status of the
     *  last command executed (as per POSIX).
     */
    if(argc == 2)
    {
        exit_status = (atoi(argv[1]) & 0xff);
    }

    /*
     * similar to bash and ksh, alert the user for the presence of running/suspended
     * jobs. if the user insists on exiting, don't alert them the 2nd time.
     */
    if(option_set('i') && pending_jobs())
    {
        /* list the pending jobs (bash extension) */
        if(optionx_set(OPTION_CHECK_JOBS))
        {
            jobs_builtin(1, (char *[]){ "jobs" });
        }

        /* interactive login shell will kill all jobs in exit_gracefully() below */
        if(/* !option_set('L') || */ !optionx_set(OPTION_HUP_ON_EXIT))
        {
            if(!tried_exit)
            {
                fprintf(stderr, "You have suspended (running) jobs.\n");
                tried_exit = 1;
                return 1;
            }
            else
            {
                kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
            }
        }
    }

    exit_gracefully(exit_status, NULL);
    __builtin_unreachable();
}


/*
 * free memory used by the internal buffers.
 */
void free_buffers(void)
{
    int i;
    /*
     * free memory used by the symbol table and other buffers. not really needed, 
     * as we are exiting, but its good practice to balance your malloc/free call ratio. 
     * also helps us when profiling our memory usage with valgrind.
     */
    struct symtab_s *symtab;
    while((symtab = symtab_stack_pop()))
    {
        free_symtab(symtab);
    }

    /* free the current working directory string */
    if(cwd)
    {
        free(cwd);
    }

    /* free the directory stack */
    struct dirstack_ent_s *ds = dirstack;
    while(ds)
    {
        struct dirstack_ent_s *next = ds->next;
        if(ds->path)
        {
            free_malloced_str(ds->path);
        }
        free(ds);
        ds = next;
    }

    /* free the list of aliases */
    unset_all_aliases();

    /* free our special variables list (RANDOM, SECONDS, ...) */
    for(i = 0; i < special_var_count; i++)
    {
        if(special_vars[i].val)
        {
            free(special_vars[i].val);
        }
    }
    
    /* free the hashed utilities table */
    if(utility_hashes)
    {
        free_hashtable(utility_hashes);
    }
    
    /* free the command input buffer */
    if(tok_buf)
    {
        free(tok_buf);
    }
    if(cmdbuf)
    {
        free(cmdbuf);
    }
}


/*
 * the last step in exiting the shell.
 *
 * the optional *errmsg argument is an error message to be output
 * before exiting.
 */
void exit_gracefully(int stat, char *errmsg)
{
    /* execute the EXIT trap (if any) */
    if(!executing_trap)
    {
        trap_handler(0);
    }
    
    /*
     *  flush history list to the history file if the shell is interactive and
     *  the save_hist extended option is set.
     */
    if(option_set('i') && optionx_set(OPTION_SAVE_HIST))
    {
        flush_history();
    }
    
    /* 
     * don't interrupt us while we perform logout actions. this is
     * what tcsh does on logout.
     */
    sigset_t intmask;
    sigemptyset(&intmask);
    sigaddset(&intmask, SIGCHLD);
    sigaddset(&intmask, SIGINT );
    sigaddset(&intmask, SIGQUIT);
    sigaddset(&intmask, SIGTERM);
    sigprocmask(SIG_BLOCK, &intmask, NULL);
    
    struct source_s src;

    /* perform logout scripts on login shell's exit */
    if(option_set('L'))
    {
        /* local logout scripts */
        if(read_file("~/.lshlogout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }
        else if(read_file("~/.logout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }
        /* global logout scripts */
        if(read_file("/etc/lshlogout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }
        else if(read_file("/etc/logout", &src))
        {
            parse_and_execute(&src);
            free(src.buffer);
        }
    }
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);

    /* restore the terminal's canonical mode (if we're reading from it) */
    if(read_stdin)
    {
        tcsetattr(0, TCSAFLUSH, &tty_attr_old);
    }

    /* check if we have an error message and if so, print it. */
    if(errmsg)
    {
        fprintf(stderr, "%s: %s\n", SHELL_NAME, errmsg);
    }

    /* interactive login shells send SIGHUP to jobs on exit */
    if(/* option_set('L') && */ optionx_set(OPTION_HUP_ON_EXIT))
    {
        kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
    }
    
    /* save the dirstack if login shell */
    if(option_set('L'))
    {
        /* must be set to save dirs */
        if(optionx_set(OPTION_SAVE_DIRS))
        {
            save_dirstack(NULL);
        }
    }
    
    /* free our internal buffers (make valgrind happy) */
    free_buffers();
    
    /* flush any hanging messages in the output streams */
    fflush(stdout);
    fflush(stderr);
    
    /* say bye bye! */
    exit(stat);
}
