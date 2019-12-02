/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: bg.c
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

#include <wait.h>
#include <signal.h>
#include "../cmd.h"
#include "../debug.h"
#include "../symtab/symtab.h"

/* defined in jobs.c */
extern int cur_job ;
extern int prev_job;

#define UTILITY         "bg"

/*
 * helper function to send the job indicated by the passed struct job_s to
 * the background. it prints a status message to stdout, then sends
 * SIGCONT to the job processes and sets $! to the PGID of the job.
 */
void do_bg(struct job_s *job)
{
    /*
     * POSIX defines bg's output as:
     *     "[%d] %s\n", <job-number>, <command>
     * 
     */
    char current = (job->job_num == cur_job ) ? '+' :
                   (job->job_num == prev_job) ? '-' : ' ';
    printf("[%d]%c %s\n", job->job_num, current, job->commandstr);
    kill(-(job->pgid), SIGCONT);

    /* set the $! special parameter */
    struct symtab_entry_s *entry = add_to_symtab("!");
    char buf[12];
    sprintf(buf, "%d", job->pgid);
    symtab_entry_setval(entry, buf);
}


/*
 * the bg builtin utility (POSIX).. used to resume a stopped job in the background. returns 0
 * in all cases, unless an unknown option was supplied.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help bg` or `bg -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int bg_builtin(int argc, char **argv)
{
    /* bg only works if job control is enabled (the monitor '-m' option is set) */
    if(!option_set('m'))
    {
        fprintf(stderr, "%s: job control is not enabled\n", UTILITY);
        return 2;
    }
    
    int i;
    struct job_s *job;
    /* if no arguments given, use the current job '%%' */
    if(argc == 1)
    {
        /* get current job */
        job = get_job_by_jobid(get_jobid("%%"));
        if(job)
        {
            /* if it is stopped, continue it in the background */
            if(WIFSTOPPED(job->status))
            {
                do_bg(job);
            }
        }
        return 0;
    }
    
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hv", &v, 1)) > 0)
    {
        if(c == 'h')
        {
            print_help(argv[0], REGULAR_BUILTIN_BG, 1, 0);
        }
        else if(c == 'v')
        {
            printf("%s", shell_ver);
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    
    /* start all the given job ids in the background */
    for(i = v; i < argc; i++)
    {
        job = get_job_by_jobid(get_jobid(argv[i]));
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %s\n", UTILITY, argv[i]);
            continue;
        }
        /* if the job is stopped, start it in the background */
        if(WIFSTOPPED(job->status))
        {
            do_bg(job);
            /*
             * save the current job in the previous job, then set the last started job
             * as the current job.
             */
            prev_job = cur_job;
            cur_job  = job->job_num;
        }
    }
    return 0;
}
