/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: ulimit.c
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

/*
 * fcntl.h and linux/fcntl.h are incompatible. we need the former for fcntl()
 * function, and the latter for the Linux-specific F_GETPIPE_SZ argument.
 * this is a hack, but it works.
 * NOTE: any other solution?
 */
#define HAVE_ARCH_STRUCT_FLOCK
#define HAVE_ARCH_STRUCT_FLOCK64

#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <sched.h>
#include <fcntl.h>
#include <linux/fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "../cmd.h"

/*
 * See the manpage at http://man7.org/linux/man-pages/man2/prlimit.2.html
 * for more info about Linux rlimits.
 */

#define UTILITY         "ulimit"
#define RLIMIT_PIPESZ   100


/*
 * struct to represent our rlimits.
 */
struct rlim_s
{
    int    which ;      /* which rlimit is it */
    rlim_t limit ;      /* the limit value */
    char   option;      /*
                         * one char describing the option passed to ulimit
                         * on the command line.
                         */
};


/*
 * get the size of pipes on this system in kilobytes (there is no direct and
 * portable way of getting the default pipe size - hence this hack).
 */
long get_pipesz(void)
{
    int pipefd[2];
    pipe(pipefd);
    long sz = ((long)fcntl(pipefd[0], F_GETPIPE_SZ));
    close(pipefd[0]);
    close(pipefd[1]);
    if(sz == -1)
    {
        return 0;
    }
    else
    {
        return sz/1024;
    }
}


/*
 * output an rlimit, printing 'unlimited' as appropriate.
 */
void print_rlimit(rlim_t limit)
{
    if(limit == RLIM_INFINITY)
    {
        printf("unlimited\n");
    }
    else
    {
        printf("%lu\n", limit);
    }
}


/*
 * return a string describing the rlimit given in 'which'.
 */
char *rlimit_name(int which)
{
    switch(which)
    {
        case RLIMIT_CORE:
            return "core file size (blocks)";

        case RLIMIT_DATA:
            return "data seg size (kbytes)";

        case RLIMIT_FSIZE:
            return "file size (blocks)";

        case RLIMIT_MEMLOCK:
            return "max locked memory (kbytes)";

        case RLIMIT_RSS:
            return "max memory size (kbytes)";

        case RLIMIT_NOFILE:
            return "open files";

        case RLIMIT_STACK:
            return "stack size (kbytes)";

        case RLIMIT_CPU:
            return "cpu time (seconds)";

        case RLIMIT_NPROC:
            return "max user processes";

        case RLIMIT_AS:
            return "virtual memory (kbytes)";

        case RLIMIT_NICE:
            return "nice value";

        case RLIMIT_RTPRIO:
            return "real-time priority";
            
        case RLIMIT_LOCKS:
            return "file locks";
            
        case RLIMIT_MSGQUEUE:
            return "message queue size (kbytes)";
            
        case RLIMIT_SIGPENDING:
            return "pending signals";
            
        case RLIMIT_PIPESZ:
            return "pipe buffer size (kbytes)";
            
        default:
            return "unknown limit";
    }
}


/*
 * get or set the rlimit given in 'which'.. if getting, the rlimit is stored in 'rlim'..
 * if setting, 'valstr' contains the value to give to the rlimit, or one of the special
 * values 'unlimited', 'hard', or 'soft'.. if the 'ishard' parameter is non-zero, we will
 * be working with the hard limit, otherwise we will manipulate the soft limit..
 * the 'div' parameter is a number we will divide the rlimit by (if getting the limit),
 * or multiply the rlimit by (if setting the limit).. for example, to set an rlimit to 2kb,
 * we can pass 2 as the rlimit value with a div of 1024.. to get the rlimit value back in kb,
 * we will pass 1024 as the value of div when we are retrieving the rlimit value.
 *
 * returns 0 if the rlimit is got/set successfully, non-zero otherwise.
 */
int parse_rlimit(struct rlim_s *rlim, int which, char *valstr, int div, int ishard)
{
    struct rlimit limit;
    /* first get the current rlimit */
    if(getrlimit(which, &limit) == 0)
    {
        /* we are setting the rlimit */
        if(valstr)
        {
            /* extract the numeric value */
            char *strend;
            long val = strtol(valstr, &strend, 10);
            if(strend == valstr)
            {
                /* no numeric value. maybe its a special string value */
                if(strcmp(valstr, "unlimited") == 0)
                {
                    /* set to unlimited */
                    if(ishard) limit.rlim_max = RLIM_INFINITY;
                    else       limit.rlim_cur = RLIM_INFINITY;
                }
                else if(strcmp(valstr, "soft") == 0)
                {
                    /* use the current soft limit.
                     * nothing to be done. if setting the soft limit here.
                     */
                    if(ishard) limit.rlim_max = limit.rlim_cur;
                }
                else if(strcmp(valstr, "hard") == 0)
                {
                    /* use the current hard limit.
                     * nothing to be done. if setting the hard limit here.
                     */
                    if(!ishard) limit.rlim_cur = limit.rlim_max;
                }
                else
                {
                    /* invalid value */
                    fprintf(stderr, "%s: invalid limit value: %s\n", UTILITY, valstr);
                    return 2;
                }
            }
            else
            {
                /* convert value as appropriate */
                if(div)
                {
                    val = val * div;
                }
                /* set the soft/hard limit */
                if(ishard)
                {
                    limit.rlim_max = val;
                }
                else
                {
                    limit.rlim_cur = val;
                }
            }
            /* now set the limit */
            if(setrlimit(which, &limit) == -1)
            {
                fprintf(stderr, "%s: failed to set rlimit: %s\n", UTILITY, strerror(errno));
                return 3;
            }
        }
        /* we are getting the rlimit */
        else
        {
            rlim->which = which;
            rlim_t l2 = ishard ? limit.rlim_max : limit.rlim_cur;
            if(l2 == RLIM_INFINITY)
            {
                rlim->limit = RLIM_INFINITY;
            }
            else
            {
                if(div)
                {
                    rlim->limit = l2/div;
                }
                else
                {
                    rlim->limit = l2;
                }
            }
        }
    }
    else
    {
        /* failed to get the rlimit */
        fprintf(stderr, "%s: failed to get rlimit: %s\n", UTILITY, strerror(errno));
        return 3;
    }
    return 0;
}


/*
 * the ulimit builtin utility (POSIX).. used to set and report process resource limits.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help ulimit` or `ulimit -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int ulimit_builtin(int argc, char **argv)
{
    struct rlimit limit;
    /* no arguments. assume the -f option */
    if(argc == 1)
    {
        if(getrlimit(RLIMIT_FSIZE, &limit) == 0)
        {
            if(limit.rlim_cur == RLIM_INFINITY)
            {
                print_rlimit(limit.rlim_cur);
            }
            else
            {
                print_rlimit(limit.rlim_cur/512);
            }
        }
        return 0;
    }
    
    /*
     * process the arguments.. we will first collect all requested
     * rlimits in an array, then print them out.. this is so if the
     * caller is asking for one rlimit, we will output it.. otherwise,
     * we will print a nicely formatted multi-line result indicating
     * each rlimit and its value.
     */
    struct rlim_s limits[16];
    memset(limits, 0, 10*sizeof(struct rlim_s));
    int count  = 0;
    int v      = 1, c;
    int res    = 0;
    int res2   = 0;
    int ishard = 0;     /* set soft limits by default, unless -H is specified */
    char *newlimit;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hac:d:e:f:i:l:m:n:p:q:r:s:t:u:v:x:HS", &v, 1)) > 0)
    {
        newlimit = ((__optarg) && (__optarg != INVALID_OPTARG)) ? __optarg : NULL;
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_ULIMIT, 1, 0);
                return 0;
                
            case 'H':               /* set hard limit */
                ishard = 1;
                continue;
                
            case 'S':               /* set soft limit */
                ishard = 0;
                continue;
                
            case 'a':               /* get all limits */
                parse_rlimit(&limits[0 ], RLIMIT_CORE      , NULL, 512 , ishard);
                limits[0 ].option = 'c';

                parse_rlimit(&limits[1 ], RLIMIT_DATA      , NULL, 1024, ishard);
                limits[1 ].option = 'd';

                parse_rlimit(&limits[2 ], RLIMIT_NICE      , NULL, 0   , ishard);
                limits[2 ].option = 'e';

                parse_rlimit(&limits[3 ], RLIMIT_FSIZE     , NULL, 512 , ishard);
                limits[3 ].option = 'f';

                parse_rlimit(&limits[4 ], RLIMIT_SIGPENDING, NULL, 0   , ishard);
                limits[4 ].option = 'i';

                parse_rlimit(&limits[5 ], RLIMIT_MEMLOCK   , NULL, 1024, ishard);
                limits[5 ].option = 'l';

                parse_rlimit(&limits[6 ], RLIMIT_RSS       , NULL, 1024, ishard);
                limits[6 ].option = 'm';

                parse_rlimit(&limits[7 ], RLIMIT_NOFILE    , NULL, 0   , ishard);
                limits[7 ].option = 'n';

                limits[8].limit  = get_pipesz();
                limits[8].which  = RLIMIT_PIPESZ;
                limits[8].option = 'p';

                parse_rlimit(&limits[9 ], RLIMIT_MSGQUEUE  , NULL, 1024, ishard);
                limits[9 ].option = 'q';

                parse_rlimit(&limits[10], RLIMIT_RTPRIO    , NULL, 0   , ishard);
                limits[10].option = 'r';

                parse_rlimit(&limits[11], RLIMIT_STACK     , NULL, 1024, ishard);
                limits[11].option = 's';

                parse_rlimit(&limits[12], RLIMIT_CPU       , NULL, 0   , ishard);
                limits[12].option = 't';

                parse_rlimit(&limits[13], RLIMIT_NPROC     , NULL, 0   , ishard);
                limits[13].option = 'u';

                parse_rlimit(&limits[14], RLIMIT_AS        , NULL, 1024, ishard);
                limits[14].option = 'v';

                parse_rlimit(&limits[15], RLIMIT_LOCKS     , NULL, 0   , ishard);
                limits[15].option = 'x';
                count = 16;
                goto print;
                break;
                
            case 'c':
                res2 = parse_rlimit(&limits[count], RLIMIT_CORE   , newlimit, 512 , ishard);
                break;
                
            case 'd':
                res2 = parse_rlimit(&limits[count], RLIMIT_DATA   , newlimit, 1024, ishard);
                break;
                
            case 'e':
                res2 = parse_rlimit(&limits[count], RLIMIT_NICE   , newlimit, 0   , ishard);
                //limits[count].limit = getpriority(PRIO_PROCESS, 0);
                //limits[count].which = RLIMIT_NICE;
                //res2 = 0;
                break;
                
            case 'f':
                res2 = parse_rlimit(&limits[count], RLIMIT_FSIZE  , newlimit, 512 , ishard);
                break;
                
            case 'i':
                res2 = parse_rlimit(&limits[count], RLIMIT_SIGPENDING, newlimit, 0, ishard);
                break;
                
            case 'l':
                res2 = parse_rlimit(&limits[count], RLIMIT_MEMLOCK, newlimit, 1024, ishard);
                break;
                
            case 'm':
                res2 = parse_rlimit(&limits[count], RLIMIT_RSS    , newlimit, 1024, ishard);
                break;
                
            case 'n':
                res2 = parse_rlimit(&limits[count], RLIMIT_NOFILE , newlimit, 0   , ishard);
                break;
                
            case 'p':
                if(newlimit == NULL)
                {
                    limits[count].limit  = get_pipesz();
                    limits[count].which  = RLIMIT_PIPESZ;
                    res2 = 0;
                }
                else
                {
                    fprintf(stderr, "%s: pipe buffer size: cannot modify a readonly limit\n", UTILITY);
                    res2 = 2;
                }
                break;
                
            case 'q':
                res2 = parse_rlimit(&limits[count], RLIMIT_MSGQUEUE, newlimit, 1024, ishard);
                break;
                
            case 'r':
                res2 = parse_rlimit(&limits[count], RLIMIT_RTPRIO , newlimit, 0   , ishard);
                break;
                
            case 's':
                res2 = parse_rlimit(&limits[count], RLIMIT_STACK  , newlimit, 1024, ishard);
                break;
                
            case 't':
                res2 = parse_rlimit(&limits[count], RLIMIT_CPU    , newlimit, 0   , ishard);
                break;
                
            case 'u':
                res2 = parse_rlimit(&limits[count], RLIMIT_NPROC  , newlimit, 0   , ishard);
                break;
                
            case 'v':
                res2 = parse_rlimit(&limits[count], RLIMIT_AS     , newlimit, 1024, ishard);
                break;
                
            case 'x':
                res2 = parse_rlimit(&limits[count], RLIMIT_LOCKS  , newlimit, 0   , ishard);
                break;
        }

        /* check if we parsed the option successfully or not */
        if(res2 == 0)
        {
            if(!newlimit) limits[count++].option = c;
        }
        else
        {
            res = res2;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    
    /* no rlimits are parsed */
    if(!count)
    {
        return res;
    }

    /* only one rlimit was requested. spit it out and exit */
    if(count == 1)
    {
        print_rlimit(limits[0].limit);
        return res;
    }

print:
    /* multiple rlimits were requested. format them nicely */
    for(v = 0; v < count; v++)
    {
        char *name = rlimit_name(limits[v].which);
        printf("%s%*s(-%c)  ", name, (int)(32-strlen(name)), " ", limits[v].option);
        /*
         * nice values are in the range 19 to -20, but the limit returned
         * from the Linux kernel is in the range 1 to 40, so you need to subtract
         * from 20 if you wanted to get the actual nice value.
         */
        if(limits[v].which == RLIMIT_NICE || limits[v].which == RLIMIT_RTPRIO)
        {
             printf("%lu\n", limits[v].limit);
        }
        else
        {
            print_rlimit(limits[v].limit);
        }
    }
    return res;
}
