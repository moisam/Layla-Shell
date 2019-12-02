/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "cmd.h"
#include "backend/backend.h"
#include "debug.h"
#include "kbdevent.h"

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
 * produce a beeping sound.
 * 
 * returns 1.
 */
int beep(void)
{
    /* in tcsh, special alias beepcmd is run when the shell wants to ring the bell */
    run_alias_cmd("beepcmd");
    putchar('\a');
    return 1;
}


/*
 * return 1 if the current user is root, 0 otherwise.
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
 * search the path for the given file.. if use_path is NULL, we use the value
 * of $PATH, otherwise we use the value of use_path as the search path.. if
 * exe_only is non-zero, we search for executable files, otherwise we search
 * for any file with the given name in the path.
 *
 * returns the absolute path of the first matching file, NULL if no match is found.
 */
char *search_path(char *file, char *use_path, int exe_only)
{
    /* bash extension for ignored executable files */
    char *EXECIGNORE = get_shell_varp("EXECIGNORE", NULL);
    /* use the given path or, if null, use $PATH */
    char *PATH = use_path ? use_path : getenv("PATH");
    char *p    = PATH;
    char *p2;
    
    while(p && *p)
    {
        p2 = p;
        /* get the end of the next entry */
        while(*p2 && *p2 != ':')
        {
            p2++;
        }
        int  plen = p2-p;
        if(!plen)
        {
            plen = 1;
        }
        /* copy the next entry */
        int  alen = strlen(file);
        char path[plen+1+alen+1];
        strncpy(path, p, p2-p);
        path[p2-p] = '\0';
        if(p2[-1] != '/')
        {
            strcat(path, "/");
        }
        /* and concat the file name */
        strcat(path, file);
        /* check if the file exists */
        struct stat st;
        if(stat(path, &st) == 0)
        {
            /* not a regular file */
            if(!S_ISREG(st.st_mode))
            {
                errno = ENOENT;
                /* check the next path entry */
                p = p2;
                if(*p2 == ':')
                {
                    p++;
                }
                continue;
            }
            /* requested exe files only */
            if(exe_only)
            {
                if(access(path, X_OK) != 0)
                {
                    errno = ENOEXEC;
                    /* check the next path entry */
                    p = p2;
                    if(*p2 == ':')
                    {
                        p++;
                    }
                    continue;
                }
            }
            /* check its not one of the files we should ignore */
            if(EXECIGNORE)
            {
                if(!match_ignore(EXECIGNORE, path))
                {
                    return get_malloced_str(path);
                }
            }
            else
            {
                return get_malloced_str(path);
            }
        }
        else    /* file not found */
        {
            /* check the next path entry */
            p = p2;
            if(*p2 == ':')
            {
                p++;
            }
        }
    }

    errno = ENOEXEC;
    return 0;
}


/*
 * fork a new child process to execute a command, passing it argc and argv..
 * flagarg is an optional argument needed by flags.. if flags are empty (i.e. zero),
 * flagarg should also be zero.. use_path tells us if we should use $PATH when
 * searching for the command (if use_path is NULL).. flags are set by some builtin
 * utilities, such as nice and nohup.. the UTILITY parameter is the name of the builtin
 * utility that called us (we use it in printing error messages).
 *
 * returns the exit status of the child process after executing the command.
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
                fprintf(stderr, "%s: failed to set nice value to %d: %s\n", UTILITY, flagarg, strerror(errno));
            }
        }
        /* request to ignore SIGHUP (by the nohup builtin) */
        if(flag_set(flags, FORK_COMMAND_IGNORE_HUP))
        {
            /* tcsh ignores the HUP signal here */
            if(signal(SIGHUP, SIG_IGN) == SIG_ERR)
            {
                fprintf(stderr, "%s: failed to ignore SIGHUP: %s\n", UTILITY, strerror(errno));
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
        fprintf(stderr, "%s: failed to exec '%s': %s\n", UTILITY, argv[0], strerror(errno));
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

    int   status;
    waitpid(child_pid, &status, WUNTRACED);
    if(WIFSTOPPED(status) && option_set('m'))
    {
        struct job_s *job = add_job(child_pid, (pid_t[]){child_pid}, 1, argv[0], 0);
        set_cur_job(job);
        notice_termination(child_pid, status);
    }
    else
    {
        set_exit_status(status);
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
        tcsetpgrp(0, tty_pid);
    }
    return status;
}


/*
 * return 1 if path exists and is a regular file, 0 otherwise.
 */
int file_exists(char *path)
{
    struct stat st;
    if(stat(path, &st) == 0)
    {
        if(S_ISREG(st.st_mode))
        {
            return 1;
        }
    }
    return 0;
}


/*
 * return the full path to a temporary filename template, to be passed to 
 * mkstemp() or mkdtemp().. as both functions modify the string we pass
 * them, we get an malloc'd string, instead of using a string from our
 * string buffer.. it is the caller's responsibility to free that string.
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
