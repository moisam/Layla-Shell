/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: stop.c
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
#include <stdlib.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include "../cmd.h"
#include "../debug.h"

#define UTILITY         "stop"


/*
 * the stop builtin utility (non-POSIX).. used to stop background jobs.
 *
 * the stop utility is a tcsh non-POSIX extension. bash doesn't have it.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help stop` or `stop -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int stop_builtin(int argc, char **argv)
{
    /* job control must be on */
    if(!option_set('m'))
    {
        fprintf(stderr, "%s: job control is not enabled\n", UTILITY);
        return 2;
    }
    
    struct job_s *job;
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hv", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_STOP, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }

    /* missing job argument */
    if(v >= argc)
    {
        fprintf(stderr, "%s: missing argument: job id\n", UTILITY);
        return 2;
    }
    
    /* process the job arguments */
    c = 0;
    for( ; v < argc; v++)
    {
        /* first try POSIX-style job ids */
        job = get_job_by_jobid(get_jobid(argv[v]));
        /* maybe we have a process pid? */
        if(!job)
        {
            char *strend;
            long pgid = strtol(argv[v], &strend, 10);
            if(strend != argv[v])
            {
                job = get_job_by_any_pid(pgid);
            }
        }
        /* still nothing? */
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %s\n", UTILITY, argv[v]);
            return 3;
        }
        /* make sure it is a background job */
        if(flag_set(job->flags, JOB_FLAG_FORGROUND))
        {
            fprintf(stderr, "%s: not a background job: %s\n", UTILITY, argv[v]);
            c = 3;
            continue;
        }
        /* stop the job */
        pid_t pid = -(job->pgid);
        /* 
         * we need to make sure the target process is running, so that
         * it will receive our signal.
         */
        kill(pid, SIGCONT);
        if(kill(pid, SIGSTOP) != 0)
        {
            fprintf(stderr, "%s: failed to send signal %s to process %d: %s\n",
                    UTILITY, "SIGSTOP", pid, strerror(errno));
            c = 3;
        }
    }
    return c;
}
