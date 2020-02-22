/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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

/* required macro definition for popen() and pclose() */
#define _POSIX_C_SOURCE 200809L

/*
 * fcntl.h and linux/fcntl.h are incompatible. we need the former for fcntl()
 * function, and the latter for the Linux-specific F_GETPIPE_SZ argument.
 * this is a hack, but it works.
 * NOTE: any other solution?
 */
#define HAVE_ARCH_STRUCT_FLOCK
#define HAVE_ARCH_STRUCT_FLOCK64

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sched.h>
#include <fcntl.h>
#include <linux/fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "builtins.h"
#include "../cmd.h"
#include "../debug.h"

/*
 * See the manpage at http://man7.org/linux/man-pages/man2/prlimit.2.html
 * for more info about Linux rlimits.
 */

#define UTILITY                 "ulimit"

#define RLIMIT_PIPESZ           256

#if defined(__linux__) || defined(__gnu_linux__)
#    define RLIMIT_MAXTHREADS   257
#    define RLIMIT_MAXPTYS      258
#    define RLIMIT_SOCKBUF_RCV  259
#    define RLIMIT_SOCKBUF_SEND 260
#endif


struct limits_table_s
{
    int  which;      /* which rlimit is it */
    int  div  ;      /* divide the rlimit by this number */
    char name ;      /*
                      * one char describing the option passed to ulimit
                      * on the command line.
                      */
}
limits_table[] =
{
    
#if defined(__linux__) || defined(__gnu_linux__)
    
    { RLIMIT_SOCKBUF_RCV ,    0, 'b' },
    { RLIMIT_SOCKBUF_SEND,    0, 'B' },
    
#endif
    
    { RLIMIT_CORE        ,  512, 'c' },
    { RLIMIT_DATA        , 1024, 'd' },
    { RLIMIT_NICE        ,    0, 'e' },
#define FSIZE_TABLE_ENTRY       3
    { RLIMIT_FSIZE       ,  512, 'f' },
    { RLIMIT_SIGPENDING  ,    0, 'i' },
    
#ifdef RLIMIT_KQUEUES
    
    { RLIMIT_KQUEUES     ,    0, 'k' },
    
#endif
    
    { RLIMIT_MEMLOCK     , 1024, 'l' },
    { RLIMIT_RSS         , 1024, 'm' },
    { RLIMIT_NOFILE      ,    0, 'n' },
    { RLIMIT_PIPESZ      ,    0, 'p' },
    { RLIMIT_MSGQUEUE    , 1024, 'q' },
    { RLIMIT_RTPRIO      ,    0, 'r' },
    { RLIMIT_STACK       , 1024, 's' },
    { RLIMIT_CPU         ,    0, 't' },
    { RLIMIT_NPROC       ,    0, 'u' },
    { RLIMIT_AS          , 1024, 'v' },
    { RLIMIT_LOCKS       ,    0, 'x' },
    
#if defined(__linux__) || defined(__gnu_linux__)
    
    { RLIMIT_MAXTHREADS  ,    0, 'T' },
    { RLIMIT_MAXPTYS     ,    0, 'P' },
    
#endif
    
};

int limits_count = sizeof(limits_table)/sizeof(struct limits_table_s);


/*
 * struct to represent our rlimits.
 */
struct rlim_s
{
    /* the parsed limit value */
    rlim_t limit;
    
    /* the string representation of the new limit value */
    char *newlimit;

    /* corresponding entry in the table above */
    struct limits_table_s *table_ptr;
};


#if defined(__linux__) || defined(__gnu_linux__)

/*
 * we get and set some rlimits using Linux-specific system files, which are
 * part of the /proc filetree.. as we access all files in a similar way, we
 * store the pathnames corresponding to each system resource.. we associate
 * two files for each resource: the first one we'll use to get/set the soft
 * limit, the second one the hard limit (for some resources, we use the same
 * file for both limits).
 */

struct linux_rlimit_s
{
    int which;
    char *path_soft, *path_hard;
}
linux_rlimits[] =
{
    { RLIMIT_MAXTHREADS  , "/proc/sys/kernel/threads-max"   , "/proc/sys/kernel/threads-max" },
    { RLIMIT_MAXPTYS     , "/proc/sys/kernel/pty/max"       , "/proc/sys/kernel/pty/max"     },
    { RLIMIT_SOCKBUF_RCV , "/proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_max"  },
    { RLIMIT_SOCKBUF_SEND, "/proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_max"  },
};

int linux_rlimit_count = sizeof(linux_rlimits)/sizeof(struct linux_rlimit_s);

#endif

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


#if defined(__linux__) || defined(__gnu_linux__)

long read_sys_file(char *path)
{
    char buf[strlen(path)+6];
    sprintf(buf, "$(<%s)", path);

    char *s = command_substitute(buf);
    if(!s)
    {
        PRINT_ERROR("%s: failed to read file: %s\n", UTILITY, path);
        return 0;
    }
    
    char *strend;
    long t = strtol(s, &strend, 10);
    if(*strend)
    {
        PRINT_ERROR("%s: invalid limit value: %s\n", UTILITY, s);
        t = 0;
    }
    
    free(s);
    return t;
}


long write_sys_file(char *path, char *new_max, char *limit_str)
{
    char *p = new_max;
    
    if(!p || !*p)
    {
        return 0;
    }
    
    if(*p == '-' || *p == '+')
    {
        p++;
    }
    
    while(isdigit(*p))
    {
        p++;
    }
    
    if(*p)
    {
        /* invalid value */
        PRINT_ERROR("%s: invalid limit value: %s\n", UTILITY, new_max);
        return 0;
    }

    /* check the system file exists and we can write to it */
    if(!file_exists(path))
    {
        PRINT_ERROR("%s: failed to set limit: %s\n", UTILITY, "system file does not exist");
        return 0;
    }
    
    if(access(path, W_OK) != 0)
    {
        PRINT_ERROR("%s: failed to set limit: %s\n", UTILITY, "insufficient permissions");
        return 0;
    }
    
    /* now write the new limit value to the system file */
    char buf[64];
    sprintf(buf, "$(echo %s > %s)", new_max, path);

    char *s = command_substitute(buf);
    if(!s)
    {
        PRINT_ERROR("%s: failed to write file: %s\n", UTILITY, path);
        return 0;
    }
    free(s);

    long t = read_sys_file(path);
    sprintf(buf, "%ld", t);
    if(strcmp(buf, new_max))
    {
        PRINT_ERROR("%s: failed to set limit: %s\n", UTILITY, limit_str);
        t = 0;
    }
    
    return t;
}

#endif


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
        
#ifdef RLIMIT_KQUEUES
            
        case RLIMIT_KQUEUES:
            return "maximum number of kqueues";

#endif
            
        case RLIMIT_PIPESZ:
            return "pipe buffer size (kbytes)";
            
#if defined(__linux__) || defined(__gnu_linux__)
            
        case RLIMIT_SOCKBUF_RCV:
            return "maximum socket receive buffer";
            
        case RLIMIT_SOCKBUF_SEND:
            return "maximum socket send buffer";
            
        case RLIMIT_MAXTHREADS:
            return "maximum number of threads";
            
        case RLIMIT_MAXPTYS:
            return "maximum number of pseudoterminals";
            
#endif
        
        default:
            return "unknown limit";
    }
}


/*
 * get or set the rlimit given in 'which'.. if getting, the rlimit is stored in 'rlim'..
 * if setting, 'valstr' contains the value to give to the rlimit, or one of the special
 * values 'unlimited', 'hard', or 'soft'.. the 'flags' parameter determines 
 * whether we'll manipulate the hard, soft, or both limits..
 * the 'div' parameter is a number we will divide the rlimit by (if getting the limit),
 * or multiply the rlimit by (if setting the limit).. for example, to set an rlimit to 2kb,
 * we can pass 2 as the rlimit value with a div of 1024.. to get the rlimit value back in kb,
 * we will pass 1024 as the value of div when we are retrieving the rlimit value.
 *
 * returns 0 if the rlimit is got/set successfully, non-zero otherwise.
 */

#define HARD_LIMIT      (1 << 0)
#define SOFT_LIMIT      (1 << 1)

int parse_rlimit(struct rlim_s *rlim, int which, char *valstr, int div, int flags)
{
    /* special treatment for the pipe file size rlimit */
    switch(which)
    {
        case RLIMIT_PIPESZ:
            /* can't set the pipe file size rlimit */
            if(valstr)
            {
                PRINT_ERROR("%s: pipe buffer size: cannot modify a readonly limit\n", UTILITY);
                return 2;
            }
            else
            {
                rlim->limit = get_pipesz();
                return 0;
            }
            break;
            
#if defined(__linux__) || defined(__gnu_linux__)
            
        case RLIMIT_MAXTHREADS:
        case RLIMIT_MAXPTYS:
        case RLIMIT_SOCKBUF_RCV:
        case RLIMIT_SOCKBUF_SEND:
            ;
            int i;
            for(i = 0; i < linux_rlimit_count; i++)
            {
                if(linux_rlimits[i].which == which)
                {
                    break;
                }
            }
            
            if(valstr)
            {
                char *name = rlimit_name(which);

                if(flag_set(flags, SOFT_LIMIT))
                {
                    if(write_sys_file(linux_rlimits[i].path_soft,
                                      valstr, name) == 0)
                    {
                        return 2;
                    }
                }

                if(flag_set(flags, HARD_LIMIT))
                {
                    if(write_sys_file(linux_rlimits[i].path_hard,
                                      valstr, name) == 0)
                    {
                        return 2;
                    }
                }
            }
            else
            {
                char *path = flag_set(flags, SOFT_LIMIT) ? 
                             linux_rlimits[i].path_soft : linux_rlimits[i].path_hard;
                rlim->limit = read_sys_file(path);
            }

            return 0;
            
#endif

    }

    /* process the other rlimits */
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
            if(*strend)
            {
                /* no numeric value. maybe its a special string value */
                if(strcmp(valstr, "unlimited") == 0)
                {
                    /* set to unlimited */
                    val = RLIM_INFINITY;
                }
                else if(strcmp(valstr, "soft") == 0)
                {
                    /* use the current soft limit */
                    val = limit.rlim_cur;
                }
                else if(strcmp(valstr, "hard") == 0)
                {
                    /* use the current hard limit */
                    val = limit.rlim_max;
                }
                else
                {
                    /* invalid value */
                    PRINT_ERROR("%s: invalid limit value: %s\n", UTILITY, valstr);
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
            }

            /* now set the limit */
            if(flag_set(flags, HARD_LIMIT))
            {
                limit.rlim_max = val;
            }
            
            if(flag_set(flags, SOFT_LIMIT))
            {
                limit.rlim_cur = val;
            }
            
            if(setrlimit(which, &limit) != 0)
            {
                PRINT_ERROR("%s: failed to set rlimit: %s\n", UTILITY, strerror(errno));
                return 3;
            }
        }
        /* we are getting the rlimit */
        else
        {
            rlim_t l2 = flag_set(flags, SOFT_LIMIT) ? limit.rlim_cur : limit.rlim_max;

            if(l2 == RLIM_INFINITY)
            {
                rlim->limit = RLIM_INFINITY;
            }
            else if(div)
            {
                rlim->limit = l2/div;
            }
            else
            {
                rlim->limit = l2;
            }
        }
    }
    else
    {
        /* failed to get the rlimit */
        PRINT_ERROR("%s: failed to get rlimit: %s\n", UTILITY, strerror(errno));
        return 3;
    }
    return 0;
}


/*
 * print the given rlimits.
 */
void print_rlimits(struct rlim_s *limits, int count)
{
    int v;
    /* multiple rlimits were requested. format them nicely */
    for(v = 0; v < count; v++)
    {
        if(limits[v].newlimit)
        {
            continue;
        }

        char *name = rlimit_name(limits[v].table_ptr->which);
        printf("%s%*s(-%c)  ", name, (int)(34-strlen(name)), " ", limits[v].table_ptr->name);

        /*
         * nice values are in the range 19 to -20, but the limit returned
         * from the Linux kernel is in the range 1 to 40, so you need to subtract
         * from 20 if you wanted to get the actual nice value.
         */
        if(limits[v].table_ptr->which == RLIMIT_NICE ||
           limits[v].table_ptr->which == RLIMIT_RTPRIO)
        {
             printf("%lu\n", limits[v].limit);
        }
        else
        {
            print_rlimit(limits[v].limit);
        }
    }
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
    /*
     * process the arguments.. we will first collect all requested
     * rlimits in an array, then print them out.. this is so if the
     * caller is asking for one rlimit, we will output it.. otherwise,
     * we will print a nicely formatted multi-line result indicating
     * each rlimit and its value.
     */
    struct rlim_s limits[limits_count];
    memset(limits, 0, limits_count*sizeof(struct rlim_s));

    int count = 0, all = 0;
    int i, j, v = 1, c;
    int res = 0;
    /* set both hard and soft limits by default, unless -H or -S is specified */
    int hard_or_soft = 0;
    char *newlimit;

    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "f:" : 
                "hac:d:e:f:i:l:m:n:p:q:r:s:t:u:v:x:HS"
                
#if defined(__linux__) || defined(__gnu_linux__)
                
                "b:B:P:T:"
                
#endif

#ifdef RLIMIT_KQUEUES
                
                "k:"
                
#endif
                ;
    
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, 1)) > 0)
    {
        newlimit = ((internal_optarg) && (internal_optarg != INVALID_OPTARG)) ?
                    internal_optarg : NULL;
        switch(c)
        {
            case 'h':
                print_help(argv[0], &ULIMIT_BUILTIN, 0);
                return 0;
                
            case 'H':               /* set hard limit */
                hard_or_soft |= HARD_LIMIT;
                continue;
                
            case 'S':               /* set soft limit */
                hard_or_soft |= SOFT_LIMIT;
                continue;
                
            case 'a':               /* get all limits */
                all = 1;
                break;
                
            default:
                for(i = 0; i < limits_count; i++)
                {
                    if(limits_table[i].name == c)
                    {
                        /* don't duplicate */
                        for(j = 0; j < count; j++)
                        {
                            if(limits[j].table_ptr->name == c)
                            {
                                i = j;
                                break;
                            }
                        }
                        
                        limits[j].newlimit = newlimit;
                        limits[j].table_ptr = &limits_table[i];

                        if(j == count)
                        {
                            count++;
                        }
                        break;
                    }
                }
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 1;
    }

    /*
     * if nothing indicated (no -S or -H), use both limits for setting, and 
     * the soft limit for printing (bash).
     */
    hard_or_soft = hard_or_soft ? : (SOFT_LIMIT | HARD_LIMIT);

    /* the -a option */
    if(all)
    {
        for(i = 0; i < limits_count; i++)
        {
            limits[i].newlimit = NULL;
            limits[i].table_ptr = &limits_table[i];
        }
        count = limits_count;
    }
    
    /* no rlimits are parsed */
    if(!count)
    {
        /* assume the -f option */
        limits[count  ].newlimit = argv[v];
        limits[count++].table_ptr = &limits_table[FSIZE_TABLE_ENTRY];
    }

    for(i = 0; i < count; i++)
    {
        res = parse_rlimit(&limits[i], limits[i].table_ptr->which,
                           limits[i].newlimit, limits[i].table_ptr->div,
                           hard_or_soft);
        /* check if we parsed the option successfully or not */
        if(res != 0)
        {
            return res;
        }
    }

    /* only one rlimit was requested. spit it out and exit */
    if(count == 1)
    {
        if(limits[0].newlimit == NULL)
        {
            print_rlimit(limits[0].limit);
        }
        return res;
    }

    print_rlimits(limits, count);
    return res;
}
