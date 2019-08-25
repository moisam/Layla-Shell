/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: times.c
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/times.h>
#include "../cmd.h"

#define UTILITY     "times"

//static   clock_t st_time;
static   clock_t en_time;
//static   struct tms st_cpu;
static   struct tms en_cpu;
long int CLK_TCK = 60;
double   st_time;

void start_clock()
{
    CLK_TCK = sysconf(_SC_CLK_TCK);
    /* can't do without the clock */
    if(CLK_TCK <= 0)
    {
        fprintf(stderr, "%s: failed to init internal clock\r\n", UTILITY);
        exit(EXIT_FAILURE);
    }
    st_time = get_cur_time();
}

int __times(int argc, char *argv[] __attribute__((unused)))
{
    if(argc > 1)
    {
        fprintf(stderr, "%s: should be called with no arguments\r\n", UTILITY);
        return 1;
    }
    en_time = times(&en_cpu);
    if(en_time == -1)
    {
        fprintf(stderr, "%s: failed to read time: %s\r\n", UTILITY, strerror(errno));
        return 1;
    }
    double utime   = ((double)en_cpu.tms_utime )/CLK_TCK;
    double stime   = ((double)en_cpu.tms_stime )/CLK_TCK;
    double cutime  = ((double)en_cpu.tms_cutime)/CLK_TCK;
    double cstime  = ((double)en_cpu.tms_cstime)/CLK_TCK;
    int    umins   = utime /60; utime  -= (umins *60);
    int    smins   = stime /60; stime  -= (smins *60);
    int    cumins  = cutime/60; cutime -= (cumins*60);
    int    csmins  = cstime/60; cstime -= (csmins*60);

    printf("%dm%.2fs %dm%.2fs\r\n%dm%.2fs %dm%.2fs\r\n",
           umins , utime , smins , stime ,
           cumins, cutime, csmins, cstime);
    return 0;
}
