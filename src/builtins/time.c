/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: time.c
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../parser/node.h"
#include "../backend/backend.h"

#define UTILITY     "time"

/*
 * this is the default format string used by ksh. bash uses a similar string,
 * except that field precision is 3 digits, instead of 2.
 */
#define DEFAULT_FMT "\nreal\t%2lR\nuser\t%2lU\nsys\t%2lS"

/* defined in times.c */
extern long int CLK_TCK;


double get_cur_time()
{
    struct timeval tm;
    gettimeofday(&tm, NULL);
    return (double)tm.tv_sec + (double)tm.tv_usec / 1000000.0;
}


void print_time(int l, int p, int hrs, int mins, double secs)
{
    char buf[32];
    if(l && hrs > 0)
    {
        sprintf(buf, "%%dh%%dm%%.%dfs", p);
        printf(buf, hrs, mins, secs);
    }
    else
    {
        mins += (hrs*60);
        sprintf(buf, "%%dm%%.%dfs", p);
        printf(buf, mins, secs);
    }
}


int __time(struct node_s *cmd)
// int __time(int argc, char *argv[])
{
    int     res = 0;
    int     use_posix_fmt = 0;
    clock_t st_time;
    clock_t en_time;
    struct  tms st_cpu;
    struct  tms en_cpu;
    double  rstart, rend;     /* real clock time diff in secs */
    
    /* get start time */
    st_time = times(&st_cpu);
    if(st_time == -1)
    {
        fprintf(stderr, "%s: failed to read time: %s\r\n", UTILITY, strerror(errno));
        return 1;
    }
    rstart = get_cur_time();

    res = !do_complete_command(cmd);
    
#if 0
    /****************************
     * process the arguments
     ****************************/
    int c, v = 1;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvp", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_TIME, 0, 0);
                break;
                
            case 'v':
                printf("%s", shell_ver);
                break;
                
            case 'p':
                use_posix_fmt = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;
    
    if(v < argc)
    {
        int    cargc = argc-v;
        char **cargv = &argv[v];    
        res = search_and_exec(cargc, cargv, NULL, SEARCH_AND_EXEC_DOFORK|SEARCH_AND_EXEC_DOFUNC);
    }
    
#endif

    /* get end time */
    en_time = times(&en_cpu);
    if(en_time == -1)
    {
        fprintf(stderr, "%s: failed to read time: %s\r\n", UTILITY, strerror(errno));
        return 1;
    }
    rend = get_cur_time();
    
    /* now calculate our times */
    double rtime = rend - rstart;
    
    /* we add 0.0 to force double arithmetic */
    double utime   = (en_cpu.tms_utime  - st_cpu.tms_utime + 0.0)/CLK_TCK;
    double stime   = (en_cpu.tms_stime  - st_cpu.tms_stime + 0.0)/CLK_TCK;
    double cutime  = (en_cpu.tms_cutime - st_cpu.tms_cutime+ 0.0)/CLK_TCK;
    double cstime  = (en_cpu.tms_cstime - st_cpu.tms_cstime+ 0.0)/CLK_TCK;
    utime = utime + cutime;
    stime = stime + cstime;
    /*
    fprintf(stderr, "%s(): CLK_TCK %ld\n", __func__, CLK_TCK);
    fprintf(stderr, "%s(): en_cpu.tms_utime = %ld, st_cpu.tms_utime = %ld\r\n", __func__, en_cpu.tms_utime, st_cpu.tms_utime);
    fprintf(stderr, "%s(): en_cpu.tms_stime = %ld, st_cpu.tms_stime = %ld\r\n", __func__, en_cpu.tms_stime, st_cpu.tms_stime);
    fprintf(stderr, "%s(): en_cpu.tms_cutime = %ld, st_cpu.tms_cutime = %ld\r\n", __func__, en_cpu.tms_cutime, st_cpu.tms_cutime);
    fprintf(stderr, "%s(): en_cpu.tms_cstime = %ld, st_cpu.tms_cstime = %ld\r\n", __func__, en_cpu.tms_cstime, st_cpu.tms_cstime);
    fprintf(stderr, "%s(): %ld,%ld\n", __func__, st_time, en_time);
    fprintf(stderr, "%s(): %f, %f, %f, %f\n", __func__, utime, stime, cutime, cstime);
    */
    /* calculate the seconds */
    int    umins   = utime /60;
    int    smins   = stime /60;
    int    rmins   = rtime /60;
    /* calculate the minutes */
    double usecs   = utime-(umins*60);
    double ssecs   = stime-(smins*60);
    double rsecs   = rtime-(rmins*60);
    /* calculate the hours (if it took that long) */
    int    uhrs    = umins/60; umins  -= (uhrs*60);
    int    shrs    = smins/60; smins  -= (shrs*60);
    int    rhrs    = rmins/60; rmins  -= (rhrs*60);

    /* now get the output format string */
    char *format = DEFAULT_FMT;
    if(!use_posix_fmt)
    {
        struct symtab_entry_s *entry = get_symtab_entry("TIMEFORMAT");
        if(entry)
        {
            /* if the value of $TIMEFORMAT is null, no timing info is displayed (ksh, bash) */
            if(!entry->val) return 0;
            format = entry->val;
        }
    }
    
    char *f = format;
    while(*f)
    {
        switch(*f)
        {
            case '\\':
                f++;
                switch(*f)
                {
                    case 'n':
                        putchar('\n'); break;

                    case 'r':
                        putchar('\r'); break;

                    case 't':
                        putchar('\t'); break;

                    case 'f':
                        putchar('\f'); break;

                    case 'v':
                        putchar('\v'); break;
                        
                    default:
                        putchar(*f);
                }
                break;
                
            case '%':
                f++;
                char precision = 3;
                char longfmt   = 0;
                switch(*f)
                {                    
                    case '%':
                    case '\0':
                        putchar('%');
                        break;
                        
                    case 'P':       /* CPU percentage = (U+S)/R */
                        printf("%d%%", (int)((utime+stime)/rtime)*100);
                        break;
                        
                    default:
                        if(isdigit(*f))         /* optional precision digit */
                        {
                            precision = (*f)-'0';
                            if(precision < 0) precision = 0;
                            if(precision > 3) precision = 3;
                            f++;
                        }
                        if(*f == 'l') longfmt = 1, f++;     /* optional 'l' for long output */
                        switch(*f)
                        {
                            case 'S':
                                print_time(longfmt, precision, shrs, smins, ssecs);
                                break;
                                
                            case 'U':
                                print_time(longfmt, precision, uhrs, umins, usecs);
                                break;
                                
                            case 'R':
                                print_time(longfmt, precision, rhrs, rmins, rsecs);
                                break;
                        }
                        break;
                }
                break;
                
            default:
                putchar(*f);
                break;
        }
        f++;
    }
    putchar('\n');
    /*
    fprintf(stderr, "real\t%dm%.3fs\r\n"
                    "user\t%dm%.3fs\r\n"
                    "sys \t%dm%.3fs\r\n",
                    rmins, rtime, umins, utime, smins, stime);
    */
    return res;
}
