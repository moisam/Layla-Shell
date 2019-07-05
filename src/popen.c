/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: popen.c
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

#define _XOPEN_SOURCE   500     /* fdopen() */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "cmd.h"
#include "builtins/setx.h"
#include "backend/backend.h"
#include "debug.h"


/*
 * this call is similar to popen(), except it sets the environment in the subshell
 * by exporting variables and function definitions. the 'r' suffix is because we
 * open the pipe for reading, equivalent to calling pipe(cmd, "r").
 */

FILE *popenr(char *cmd)
{
    if(!cmd) return NULL;
    int filedes[2] = { -1, -1 };
    pid_t pid;
    
    /* create new pipe */
    pipe(filedes);
    
    if((pid = fork()) == 0)     /* child process */
    {
        
        /* reset the -dumpast option if set */
        set_option('d', 0);

        /*
         * export environment variables and functions.
         */
        do_export_vars();
        
        /* indicate we are in a subshell */
        inc_subshell_var();

        /* modify our standard streams */
        close(0);   /* stdin */
        open("/dev/null", O_RDONLY);
        close(1);   /* stdout */
        dup  (filedes[1]);
        close(filedes[0]);
        close(filedes[1]);

        /*
         * execute the command. we imitate POSIX's definition of popen(), where the call to popen()
         * results in behaviour akin to:
         * 
         *      execl(shell path, "sh", "-c", command, (char *)0);
         * 
         * see this link: https://pubs.opengroup.org/onlinepubs/009695399/functions/popen.html
         * 
         * we first try executing our shell, using our argv[0]. if failed, we try /bin/sh.
         */
        errno = 0;
        execl(shell_argv[0], "sh", "-c", cmd, NULL);

        /* returning from execl means error */
        execl("/bin/sh", "sh", "-c", cmd, NULL);

        /* returning from execl means error */
        if(errno)
            fprintf(stderr, "%s: failed to exec '%s': %s\r\n", SHELL_NAME, "/bin/sh", strerror(errno));

        if(errno == ENOEXEC)
        {
            exit(EXIT_ERROR_NOEXEC);
        }
        else if(errno == ENOENT)
        {
            exit(EXIT_ERROR_NOENT);
        }
        else exit(EXIT_FAILURE);
    }
    else if(pid < 0)            /* error */
    {
        fprintf(stderr, "%s: failed to fork subshell: %s\r\n", SHELL_NAME, strerror(errno));
        return NULL;
    }
    else
    {
        close(filedes[1]);                 /* close the other end, we will not use it */
        /*
         * set the close-on-exec flag. we could've used pipe2() to set this flag when we
         * created the pipe, but this would have caused the coprocess to fail after fork,
         * when it calls calls execl().
         */
        fcntl(filedes[0], F_SETFD, FD_CLOEXEC);
        return fdopen(filedes[0], "r");
    }
}
