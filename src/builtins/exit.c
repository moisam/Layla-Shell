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

int tried_exit = 0;

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;
extern struct termios tty_attr;
//extern int            old_keyboard_mode;

/* declared in main.c   */
extern int read_stdin;

/* declared in dirstack.c */
extern struct dirstack_ent_s *dirstack;

extern char *__buf;                     /* defined in lexical.c */
extern char *cmdbuf;                    /* defined in cmdline.c */
extern struct hashtab_s *str_hashes;    /* defined in strbuf.c  */


int __exit(int argc, char **argv)
{
    if(argc > 2)
    {
        fprintf(stderr, "exit: too many arguments\r\n");
        return 1;
    }    
    if(argc == 2) exit_status = (atoi(argv[1]) & 0xff);

    /*
     * similar to ksh, alert the user for the presence of running/suspended
     * jobs. if the user insists on exiting, don't alert them the 2nd time.
     */
    if(option_set('i') && pending_jobs())
    {
        /* bash extension */
        if(optionx_set(OPTION_CHECK_JOBS)) jobs(1, (char *[]){ "jobs" });

        /* interactive login shell will kill all jobs in exit_gracefully() below */
        if(/* !option_set('L') || */ !optionx_set(OPTION_HUP_ON_EXIT))
        {
            if(!tried_exit)
            {
                fprintf(stderr, "You have pending jobs.\r\n");
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


void free_buffers()
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

    if(cwd) free(cwd);

    struct dirstack_ent_s *ds = dirstack;
    while(ds)
    {
        struct dirstack_ent_s *next = ds->next;
        if(ds->path) free_malloced_str(ds->path);
        free(ds);
        ds = next;
    }

    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(__aliases[i].name) free(__aliases[i].name);
        if(__aliases[i].val ) free(__aliases[i].val );
    }

    for(i = 0; i < special_var_count; i++)
    {
        if(special_vars[i].val) free(special_vars[i].val);
    }
    
    if(utility_hashes) free_hashtable(utility_hashes);
    
    if(__buf ) free(__buf );
    if(cmdbuf) free(cmdbuf);
}


/*
 * the optional argument is an error message to be output
 * before exiting.
 */
void exit_gracefully(int stat, char *errmsg)
{
    /* execute EXIT traps */
    if(!executing_trap) trap_handler(0);
    
    /* flush history list to the history file */
    if(optionx_set(OPTION_SAVE_HIST)) flush_history();
    
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
    
    /* perform logout script on login shell's exit */
    if(option_set('L'))
    {
        if(read_file("/etc/logout", src))
        {
            do_cmd();
            free(src->buffer);
        }
        if(read_file("~/.logout", src))
        {
            do_cmd();
            free(src->buffer);
        }
    }
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);

    /* restore terminal's canonical mode */
    if(read_stdin)
    {
        tcsetattr(0, TCSAFLUSH, &tty_attr_old);
        /*
        if(kbd_fd >= 0)
        {
            if(using_con) ioctl(0, KDSKBMODE, old_keyboard_mode);
            close(kbd_fd);
        }
        */
    }

    /* check if we have an error message. if so, print it. */
    if(errmsg) fprintf(stderr, "%s: %s\r\n", SHELL_NAME, errmsg);

    /* interactive login shells send SIGHUP to jobs on exit */
    if(/* option_set('L') && */ optionx_set(OPTION_HUP_ON_EXIT))
    {
        kill_all_jobs(SIGHUP, JOB_FLAG_DISOWNED);
    }
    
    /* save the dirstack if login shell */
    if(option_set('L'))
    {
        /* must be set to save dirs */
        if(optionx_set(OPTION_SAVE_DIRS)) save_dirstack(NULL);
    }
    
    /* free our internal buffers (make valgrind happy) */
    free_buffers();
    
    /* flush any hanging messages in the output streams */
    fflush(stdout);
    fflush(stderr);
    
    /* say bye bye. */
    exit(stat);
}
