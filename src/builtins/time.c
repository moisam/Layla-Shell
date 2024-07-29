/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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
#include "builtins.h"
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../parser/node.h"
#include "../backend/backend.h"
#include "../include/debug.h"

#define UTILITY     "time"

/*
 * This is the default format string used by ksh. bash uses a similar string,
 * except that field precision is 3 digits, instead of 2.
 */
#define DEFAULT_FMT "\nreal\t%2lR\nuser\t%2lU\nsys\t%2lS"

/* defined in times.c */
extern long int CLK_TCK;


/*
 * Get the current time in seconds.
 */
double get_cur_time(void)
{
    struct timeval tm;
    gettimeofday(&tm, NULL);
    return (double)tm.tv_sec + (double)tm.tv_usec / 1000000.0;
}


/*
 * Print the minutes and seconds specified in the mins and secs parameters,
 * respectively. If l != 0 and hrs > 0, we also print the hours. The p
 * parameter represents the percision, or the number of decimal places to use
 * when we print the seconds.
 */
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


/*
 * The time builtin utility (POSIX). Used to execute commands and time their execution.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help time` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int time_builtin(struct source_s *src, struct node_s *cmd)
{
    int     res = 0;
    /* use POSIX format by default if running in the --posix mode */
    int     use_posix_fmt = option_set('P');
    clock_t st_time;          /* start time (before executing the command) */
    clock_t en_time;          /* end time (after executing the command) */
    struct  tms st_cpu;
    struct  tms en_cpu;
    double  rstart, rend;     /* real clock time diff in secs */
    
    /* if time is called with no arguments, we print the shell's times information (zsh) */
    if(!cmd)
    {
        return do_builtin_internal(times_builtin, 1, NULL);
    }
    
    /* get start time */
    st_time = times(&st_cpu);
    if(st_time == -1)
    {
        PRINT_ERROR(UTILITY, "failed to read time: %s", strerror(errno));
        return 1;
    }
    rstart = get_cur_time();

    /* execute the command(s) */
    res = !do_list(src, cmd, NULL);
    
    /* get end time */
    en_time = times(&en_cpu);
    if(en_time == -1)
    {
        PRINT_ERROR(UTILITY, "failed to read time: %s", strerror(errno));
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
        /*
         * ksh and bash use the $TIMEFORMAT variable to determine the format of time's output.
         * zsh uses the value of the $TIMEFMT variable. we'll follow the first.
         */
        struct symtab_entry_s *entry = get_symtab_entry("TIMEFORMAT");
        if(entry)
        {
            /* if the value of $TIMEFORMAT is null, no timing info is displayed (ksh, bash) */
            if(!entry->val)
            {
                return 0;
            }
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
                        putchar('\n');
                        break;

                    case 'r':
                        putchar('\r');
                        break;

                    case 't':
                        putchar('\t');
                        break;

                    case 'f':
                        putchar('\f');
                        break;

                    case 'v':
                        putchar('\v');
                        break;
                        
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
                            if(precision < 0)
                            {
                                precision = 0;
                            }
                            if(precision > 3)
                            {
                                precision = 3;
                            }
                            f++;
                        }
                        if(*f == 'l')
                        {
                            longfmt = 1, f++;     /* optional 'l' for long output */
                        }
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
    return res;
}
