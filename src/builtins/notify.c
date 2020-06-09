/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: notify.c
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

#include <unistd.h>
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include "builtins.h"
#include "../cmd.h"
#include "../debug.h"

#define UTILITY         "notify"


/*
 * The notify builtin utility (non-POSIX). Prints the status of running jobs.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help notify` or `notify -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int notify_builtin(int argc, char **argv)
{
    /* job control must be on */
    if(!option_set('m'))
    {
        PRINT_ERROR("%s: job control is not enabled\n", UTILITY);
        return 2;
    }
    
    struct job_s *job;
    /* called with no job arguments. use the current job */
    if(argc == 1)
    {
        job = get_job_by_jobid(get_jobid("%%"));
        if(!job)
        {
            PRINT_ERROR("%s: unknown job: %%%%\n", UTILITY);
            return 3;
        }
        /* mark the job as notified */
        job->flags |= JOB_FLAG_NOTIFY;
        return 0;
    }

    int v = 1, c;
    /************************************
     * process the options and arguments
     ************************************/
    while((c = parse_args(argc, argv, "hv", &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        if(c == 'h')
        {
            print_help(argv[0], &NOTIFY_BUILTIN, 0);
        }
        else if(c == 'v')
        {
            printf("%s", shell_ver);
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* no job arguments */
    if(v >= argc)
    {
        return 0;
    }
    
    for( ; v < argc; v++)
    {
        job = get_job_by_jobid(get_jobid(argv[v]));
        if(!job)
        {
            PRINT_ERROR("%s: unknown job: %s\n", UTILITY, argv[v]);
            return 3;
        }
        /* mark the job as notified */
        job->flags |= JOB_FLAG_NOTIFY;
    }
    return 0;
}
