/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: unlimit.c
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

#define UTILITY         "unlimit"

char *rlim_option(char *name);

/* 
 * the list of all ulimit resource limit options.. if any resources are
 * added/removed in ulimit, they should be updated here also.
 */
static char *all_rlim[] =
{
    "-c", "-d", "-e", "-f", "-i", "-l", "-m", "-n",
    "-p", "-q", "-r", "-s", "-t", "-u", "-v", "-x",
};


/*
 * set all rlimit to 'unlimited'.. if 'ishard' is non-zero, the hard limits
 * are removed, otherwise the soft limits are removed.. if 'ignore_err' is
 * zero, the function stops at the first error, otherwise it tries to remove
 * all limits.
 *
 * returns 0 on success, non-zero otherwise.
 */
int unlimit_all(int ishard, int ignore_err)
{
    int j, i = sizeof(all_rlim)/sizeof(char *);
    char *op = ishard ? "-H" : "-S";
    char *argv[] = { "ulimit", op, NULL, "unlimited", NULL };
    j = 0;
    while(i-- >= 0)
    {
        argv[2] = all_rlim[i];
        j = __ulimit(4, argv);
        if(j && !ignore_err)
        {
            break;
        }
    }
    return j;
}


/*
 * the unlimit builtin utility (non-POSIX).. used to set rlimits to unlimited (don't
 * confuse this with ulimit).
 *
 * the unlimit utility is a tcsh non-POSIX extension. bash doesn't have it.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help unlimit` or `unlimit -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int unlimit(int argc, char **argv)
{
    int v = 1, c;
    int ignore_err = 0, ishard = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvHfSa", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_UNLIMIT, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                break;
                
            case 'f':
                ignore_err = 1;
                break;
                
            /* remove hard limits (only root can do that) */
            case 'H':
                ishard = 1;
                break;
                
            /* remove soft limits */
            case 'S':
                ishard = 0;
                break;
            
            /* remove all limits */
            case 'a':
                return unlimit_all(ishard, ignore_err);
        }
    }
    /* we accept unknown options, as they might be ulimit options passed to us */
    //if(c == -1) return 2;

    /* missing arguments */
    if(v >= argc)
    {
        fprintf(stderr, "%s: missing argument: resource name\n", UTILITY);
        return 2;
    }

    /* process the arguments */
    char *op = ishard ? "-H" : "-S";
    c = 0;
    for( ; v < argc; v++)
    {
        char *op2 = rlim_option(argv[v]);
        if(!op2)
        {
            fprintf(stderr, "%s: unknown resource name: %s\n", UTILITY, argv[v]);
            if(ignore_err)
            {
                continue;
            }
            else
            {
                return 2;
            }
        }
        
        if(op2[1] == 'a')
        {
            c = unlimit_all(ishard, ignore_err);
        }
        else
        {
            char *argv2[] = { "ulimit", op, op2, "unlimited", NULL };
            c = __ulimit(4, argv2);
        }
        if(c && !ignore_err)
        {
            return 2;
        }
    }
    return c;
}


/*
 * get the ulimit utility option corresponding to the given resource name.
 */
char *rlim_option(char *name)
{
    if(strcmp(name, "core"    ) == 0 || strcmp(name, "-c") == 0) return "-c";
    if(strcmp(name, "data"    ) == 0 || strcmp(name, "-d") == 0) return "-d";
    if(strcmp(name, "nice"    ) == 0 || strcmp(name, "-e") == 0) return "-e";
    if(strcmp(name, "file"    ) == 0 || strcmp(name, "-f") == 0) return "-f";
    if(strcmp(name, "signal"  ) == 0 || strcmp(name, "-i") == 0) return "-i";
    if(strcmp(name, "mlock"   ) == 0 || strcmp(name, "-l") == 0) return "-l";
    if(strcmp(name, "rss"     ) == 0 || strcmp(name, "-m") == 0) return "-m";
    if(strcmp(name, "fd"      ) == 0 || strcmp(name, "-n") == 0) return "-n";
    if(strcmp(name, "buffer"  ) == 0 || strcmp(name, "-p") == 0) return "-p";
    if(strcmp(name, "message" ) == 0 || strcmp(name, "-q") == 0) return "-q";
    if(strcmp(name, "rtprio"  ) == 0 || strcmp(name, "-r") == 0) return "-r";
    if(strcmp(name, "stack"   ) == 0 || strcmp(name, "-s") == 0) return "-s";
    if(strcmp(name, "cputime" ) == 0 || strcmp(name, "-t") == 0) return "-t";
    if(strcmp(name, "userproc") == 0 || strcmp(name, "-u") == 0) return "-u";
    if(strcmp(name, "virtmem" ) == 0 || strcmp(name, "-v") == 0) return "-v";
    if(strcmp(name, "flock"   ) == 0 || strcmp(name, "-x") == 0) return "-x";
    if(strcmp(name, "all"     ) == 0 || strcmp(name, "-a") == 0) return "-a";
    return NULL;
}
