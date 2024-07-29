/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: helpfunc.c
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

/* required macro definition for confstr() */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "include/cmd.h"
#include "include/sig.h"
#include "include/debug.h"
#include "include/kbdevent.h"
#include "backend/backend.h"
#include "builtins/builtins.h"

/********************************************************************
 * 
 * This file contains helper functions for other parts of the shell.
 * 
 ********************************************************************/

/****************************************
 *
 * Miscellaneous functions
 *
 ****************************************/


/*
 * Produce a beeping sound.
 * 
 * Returns 1.
 */
int beep(void)
{
    /* in tcsh, special alias beepcmd is run when the shell wants to ring the bell */
    run_alias_cmd("beepcmd");
    putchar('\a');
    return 1;
}


/*
 * Return 1 if the current user is root, 0 otherwise.
 */
int isroot(void)
{
    static uid_t uid = -1;
    static char  gotuid = 0;
    if(!gotuid)
    {
        uid = geteuid();
        gotuid = 1;
    }
    return (uid == 0);
}


/*
 * Get the default path for searching commands.
 */
char *get_default_path(void)
{
    char *path = NULL;
    
#ifdef _CS_PATH
    
    size_t len = (size_t)confstr(_CS_PATH, (char *)NULL, (size_t)0);
    if(len > 0)
    {
        path = malloc(len + 2);
        if(path)
        {
            *path = '\0';
            confstr(_CS_PATH, path, len);
        }
        else
        {
            path = COMMAND_DEFAULT_PATH;
        }
    }
    else
    {
        path = COMMAND_DEFAULT_PATH;
    }

#else

    path = COMMAND_DEFAULT_PATH;
        
#endif
    
    return path;
}


/*
 * Search the path for the given file. If use_path is NULL, we use the value
 * of $PATH, otherwise we use the value of use_path as the search path. If
 * exe_only is non-zero, we search for executable files, otherwise we search
 * for any file with the given name in the path.
 *
 * Returns the absolute path of the first matching file, NULL if no match is found.
 */
char *search_path(char *file, char *use_path, int exe_only)
{
    /* bash extension for ignored executable files */
    char *EXECIGNORE = get_shell_varp("EXECIGNORE", NULL);
    /* use the given path or, if null, use $PATH */
    char *PATH = use_path ? use_path : get_shell_varp("PATH", NULL);
    char *p;
    
    while((p = next_path_entry(&PATH, file, 0)))
    {
        /* check if the file exists */
        struct stat st;
        if(stat(p, &st) == 0)
        {
            /* not a regular file */
            if(!S_ISREG(st.st_mode))
            {
                errno = ENOEXEC;
                free(p);
                continue;
            }

            /* requested exe files only */
            if(exe_only)
            {
                if(access(p, X_OK) != 0)
                {
                    errno = ENOEXEC;
                    free(p);
                    continue;
                }
            }

            /* check its not one of the files we should ignore */
            if(!EXECIGNORE || !match_ignore(EXECIGNORE, p))
            {
                char *p2 = get_malloced_str(p);
                free(p);
                return p2;
            }
        }
        
        free(p);
    }

    errno = ENOENT;
    return NULL;
}


/*
 * Fork a new child process to execute a command, passing it argc and argv.
 * flagarg is an optional argument needed by flags. If flags are empty (i.e. zero),
 * flagarg should also be zero.. use_path tells us if we should use $PATH when
 * searching for the command (if use_path is NULL). flags are set by some builtin
 * utilities, such as nice and nohup. The UTILITY parameter is the name of the builtin
 * utility that called us (we use it in printing error messages).
 *
 * Returns the exit status of the child process after executing the command.
 */
int fork_command(int argc, char **argv, char *use_path, char *UTILITY, int flags, int flagarg)
{
    pid_t child_pid;
    if((child_pid = fork_child()) == 0)    /* child process */
    {
        if(option_set('m'))
        {
            setpgid(0, 0);
            tcsetpgrp(0, child_pid);
        }
        reset_nonignored_traps();

        /* request to change the command's nice value (by the nice builtin) */
        if(flag_set(flags, FORK_COMMAND_DONICE))
        {
            if(setpriority(PRIO_PROCESS, 0, flagarg) == -1)
            {
                PRINT_ERROR(UTILITY, "failed to set nice value to %d: %s", flagarg, strerror(errno));
            }
        }
        /* request to ignore SIGHUP (by the nohup builtin) */
        if(flag_set(flags, FORK_COMMAND_IGNORE_HUP))
        {
            /* tcsh ignores the HUP signal here */
            if(set_signal_handler(SIGHUP, SIG_IGN) != 0)
            {
                PRINT_ERROR(UTILITY, "failed to ignore SIGHUP: %s", strerror(errno));
            }
            /*
             * ... and GNU coreutils nohup modifies the standard streams if they are
             * connected to a terminal.
             */
            if(isatty(0))
            {
                close(0);
                open("/dev/null", O_RDONLY);
            }
            if(isatty(1))
            {
                close(1);
                /* try to open a file in CWD. if failed, try to open it in $HOME */
                if(open("nohup.out", O_APPEND) == -1)
                {
                    char *home = get_shell_varp("HOME", ".");
                    char path[strlen(home)+11];
                    sprintf(path, "%s/nohup.out", home);
                    if(open(path, O_APPEND) == -1)
                    {
                        /* nothing. open the NULL device */
                        open("/dev/null", O_WRONLY);
                    }
                }
            }
            /* redirect stderr to stdout */
            if(isatty(2))
            {
                close(2);
                dup2(1, 2);
            }
        }

        /* export variables and execute the command */
        do_export_vars(EXPORT_VARS_EXPORTED_ONLY);
        do_exec_cmd(argc, argv, use_path, NULL);

        /* NOTE: we should NEVER come back here, unless there is error of course!! */
        PRINT_ERROR(UTILITY, "failed to exec `%s`: %s", argv[0], strerror(errno));
        if(errno == ENOEXEC)
        {
            exit(EXIT_ERROR_NOEXEC);
        }
        if(errno == ENOENT)
        {
            exit(EXIT_ERROR_NOENT);
        }
        exit(EXIT_FAILURE);
    }
    /* ... and parent countinues over here ...    */

    /* NOTE: we re-set the process group id here (and above in the child process) to make
     *       sure it gets set whether the parent or child runs first (i.e. avoid race condition).
     */
    if(option_set('m'))
    {
        setpgid(child_pid, 0);
        /* tell the terminal who's the foreground pgid now */
        tcsetpgrp(0, child_pid);
    }

    int status = wait_on_child(child_pid, NULL, NULL);
    set_exit_status(status);
    if(WIFSTOPPED(status) && option_set('m'))
    {
        struct job_s *job = new_job(argv[0], 0);
        if(job)
        {
            add_pid_to_job(job, child_pid);
            add_job(job);
            free(job);
        }
        notice_termination(child_pid, status, 1);
    }
    else
    {
        struct job_s *job = get_job_by_any_pid(child_pid);
        if(job)
        {
            set_pid_exit_status(job, child_pid, status);
            set_job_exit_status(job, child_pid, status);
        }
    }
    /* reset the terminal's foreground pgid */
    if(option_set('m'))
    {
        tcsetpgrp(0, shell_pid);
    }
    return exit_status;
}


/*
 * Return 1 if path exists and is a regular file, 0 otherwise.
 */
int file_exists(char *path)
{
    struct stat st;
    if(stat(path, &st) == 0)
    {
        if(S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
        {
            return 1;
        }
        else if(S_ISDIR(st.st_mode))
        {
            errno = EISDIR;
        }
        else
        {
            errno = EINVAL;
        }
    }
    else
    {
        errno = ENOENT;
    }
    return 0;
}


/*
 * Return the full path to a temporary filename template, to be passed to 
 * mkstemp() or mkdtemp(). As both functions modify the string we pass
 * them, we get an malloc'd string, instead of using a string from our
 * string buffer. It is the caller's responsibility to free that string.
 */
char *get_tmp_filename(void)
{
    char *tmpdir = get_shell_varp("TMPDIR", "/tmp");
    int   len = strlen(tmpdir);
    char  buf[len+18];
    sprintf(buf, "%s%clsh/", tmpdir, '/');
    /* try to mkdir our temp directory, so that all our tmp files reside under /tmp/lsh. */
    if(mkdir(buf, 0700) == 0)
    {
        strcat(buf, "lsh.tmpXXXXXX");
    }
    /* if we failed, just return a normal tmp file under /tmp */
    else
    {
        sprintf(buf, "%s%clsh.tmpXXXXXX", tmpdir, '/');
    }
    return __get_malloced_str(buf);
}
