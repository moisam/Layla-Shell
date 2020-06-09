/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: hup.c
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
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"


/*
 * The hup/nohup builtin utilities (non-POSIX) are used to run a command, making it
 * ignore the SIGHUP signal (nohup utility) or not (hup utility).
 *
 * The hup/nohup utilities are tcsh non-POSIX extensions. bash doesn't have them.
 * nohup is part of the GNU coreutils package, not the shell itself.
 *
 * This function does the role of both utilities, depending on the name it is called
 * with (to hup or to nohup, that is the question).
 */

int hup_builtin(int argc, char **argv)
{
    int i;
    /* determine whether to run as hup or nohup, depending on argv[0] */
    int hup = strcmp(argv[0], "hup") == 0 ? 1 : 0;
    char *UTILITY = hup ? "hup" : "nohup";

    /* parse options */
    for(i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if(*arg == '-')
        {
            char *p = arg;
            /* the special - and -- options signal the end of options */
            if(strcmp(p, "-") == 0 || strcmp(p, "--") == 0)
            {
                i++;
                break;
            }
            /* skip the '-' */
            p++;
            /* parse the options string */
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], hup ? &HUP_BUILTIN : &NOHUP_BUILTIN, 0);
                        return 0;
                        
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                        
                    default:
                        PRINT_ERROR("%s: invalid option: -%c\n", UTILITY, *p);
                        return 2;
                }
                p++;
            }
        }
        else
        {
            /* end of options */
            break;
        }
    }

    /* we should have at least one argument */
    if(i >= argc)
    {
        PRINT_ERROR("%s: missing argument: command name\n", UTILITY);
        return 2;
    }
    
    int    cargc = argc-i;
    char **cargv = &argv[i];
    return fork_command(cargc, cargv, NULL, UTILITY, hup ? 0 : FORK_COMMAND_IGNORE_HUP, 0);
}
