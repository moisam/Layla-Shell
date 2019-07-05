/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: disown.c
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

#include <stdio.h>
#include <stdlib.h>
#include "../cmd.h"
#include "../debug.h"

#define UTILITY             "disown"

/* defined in ../jobs.c */
extern struct job __jobs[];

// int disown_all = 0;

void disown_job(struct job *job, int nohup);


int disown(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    struct job *job;
    int v = 1, c;
    int all_jobs     = 0;
    int running_only = 0;
    int stopped_only = 0;
    int nohup        = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "ahrsv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                nohup = 1;
                break;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'a':
                all_jobs = 1;
                break;
                
            case 'r':
                running_only = 1;
                break;
                
            case 's':
                stopped_only = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;
    
    /* 
     * ksh : if no job ids given, disown all jobs.
     * bash: if no job ids given, and no -a or -r supplied, disown 
     *       the current job.
     *
     * we follow bash here.
     */
    if(v >= argc)
    {
        /* use the current job */
        if(!all_jobs && !running_only && !stopped_only)
        {
            job = get_job_by_jobid(get_jobid("%%"));
            if(!job)
            {
                fprintf(stderr, "%s: unknown job: %%%%\r\n", UTILITY);
                return 1;
            }
            disown_job(job, nohup);
        }
        
        /* disown all jobs */
        for(job = &__jobs[0]; job < &__jobs[MAX_JOBS]; job++)
        {
            if(job->job_num != 0)
            {
                if(running_only && NOT_RUNNING(job->status)) continue;
                if(stopped_only && !WIFSTOPPED(job->status)) continue;
                disown_job(job, nohup);
            }
        }
        return 0;
    }

    for( ; v < argc; v++)
    {
        /* first try POSIX-style job ids */
        job = get_job_by_jobid(get_jobid(argv[v]));
        /* maybe we have a process pid? */
        if(!job)
        {
            char *strend;
            long pgid = strtol(argv[v], &strend, 10);
            if(strend != argv[v]) job = get_job_by_any_pid(pgid);
        }
        /* still nothing? */
        if(!job)
        {
            fprintf(stderr, "%s: unknown job: %s\r\n", UTILITY, argv[v]);
            return 1;
        }

        if(running_only && NOT_RUNNING(job->status)) continue;
        if(stopped_only && !WIFSTOPPED(job->status)) continue;
        disown_job(job, nohup);
    }
    return 1;
}


void disown_job(struct job *job, int nohup)
{
    if(nohup)
    {
        /* don't remove the job, just mark it as disowned */
        job->flags |= JOB_FLAG_DISOWNED;
    }
    else
    {
        /* disown this b&^%$ */
        kill_job(job);
    }
}
