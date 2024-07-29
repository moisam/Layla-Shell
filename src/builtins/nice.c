/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: nice.c
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../backend/backend.h"
#include "../include/debug.h"

#define UTILITY         "nice"

/*
 * we use tcsh's default value (4), rather than GNU coreutils default (10).
 */
#define DEFAULT_NICEVAL 4


/*
 * Extract the numeric nice value from the string str.
 *
 * Returns the default nice value if str contains an invalid value.
 */
int get_niceval(char *str)
{
    errno = 0;
    if(!str)
    {
        return DEFAULT_NICEVAL;
    }
    char *strend = NULL;
    int i = strtol(str, &strend, 10);
    if(strend == str)
    {
        PRINT_ERROR(UTILITY, "invalid nice value: %s", str);
        errno = EINVAL;
        return DEFAULT_NICEVAL;
    }
    return i;
}


/*
 * The enable builtin utility (non-POSIX). If called without a command, this
 * utility sets the nice value for the shell. Otherwise it runs the given
 * command with the passed nice value.
 *
 * The nice utility is a tcsh non-POSIX extension. bash doesn't have it,
 * as it is part of the GNU coreutils package, not the shell itself.
 *
 * If setting the shell's nice value, returns 0 on success, non-zero otherwise.
 * If running a command, returns the result of executing the command.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help nice` or `nice -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int nice_builtin(int argc, char **argv)
{
    int niceval = DEFAULT_NICEVAL;
    int hasnice = 0;
    int i;
    /* process options and arguments */
    for(i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if(*arg == '-')
        {
            char *p = arg;
            /* special options '-' and '--' signal the end of options */
            if(strcmp(p, "-") == 0 || strcmp(p, "--") == 0)
            {
                i++;
                break;
            }
            /* skip the '-' */
            p++;
            /* process the options string */
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], &NICE_BUILTIN, 0);
                        return 0;
                        
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                        
                    default:
                        /* we have a negative nice value */
                        niceval = get_niceval(arg);
                        if(errno == EINVAL)
                        {
                            return 2;
                        }
                        hasnice = 1;
                        i++;
                        break;
                }
                /* break the loop if we have a nice value */
                if(hasnice)
                {
                    break;
                }
                p++;
            }
            /* break the loop if we have a nice value */
            if(hasnice)
            {
                break;
            }
        }
        /* we have a positive nice value */
        else if(*arg == '+' || isdigit(*arg))
        {
            niceval = get_niceval(arg);
            if(errno == EINVAL)
            {
                return 2;
            }
            hasnice = 1;
            i++;
            break;
        }
        /* end of options */
        else
        {
            break;
        }
    }

    /* no arguments. print or set the shell's nice value */
    if(i >= argc)
    {
        if(!hasnice)      /* print current nice value if called with no args */
        {
            errno = 0;
            niceval = getpriority(PRIO_PROCESS, 0);
            if(niceval == -1 && errno)
            {
                PRINT_ERROR(UTILITY, "failed to get nice value: %s", strerror(errno));
                return 3;
            }
            printf("%d\n", niceval);
            return 0;
        }
        else              /* set our own nice value, similar to tcsh's nice */
        {
            if(setpriority(PRIO_PROCESS, 0, niceval) == -1)
            {
                PRINT_ERROR(UTILITY, "failed to set nice value to %d: %s", niceval, strerror(errno));
                return 2;
            }
            return 0;
        }
    }
    
    int    cargc = argc-i;
    char **cargv = &argv[i];
    return fork_command(cargc, cargv, NULL, UTILITY, FORK_COMMAND_DONICE, niceval);
}
