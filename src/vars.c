/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: vars.c
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

/* macro definition needed to use timer_settime() */
#define _POSIX_C_SOURCE 200112L

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "cmd.h"
#include "debug.h"

#define MAX_VAL_LEN            15

struct var_s special_vars[] =
{
    /*
     * non-POSIX extensions.
     */
    
    /* generate a random integer, uniformly distributed between 0 and 32767 */
    { "RANDOM" , },

    /* number of seconds since shell startup or last SECONDS assignment */
    { "SECONDS", },
    
    /* number of seconds since Unix epoch, floating point */
    { "EPOCHREALTIME", },
    
    /* number of seconds since Unix epoch, integer */
    { "EPOCHSECONDS", },
    
    /* 
     * dirstack entries. bash's version is an array, while tcsh's version is similar to ours,
     * except the variable name is lowercase instead of uppercase.
     */
    { "DIRSTACK", },
    
    /* in tcsh, special alias 'periodic' is run every 'tperiod' minutes */
    { "TPERIOD", },
};
int    special_var_count = sizeof(special_vars)/sizeof(struct var_s);

/* last value and time assigned to SECONDS */
// int    last_sec    = 0;
double last_sec_at = 0;

/* defined in builtins/time.c */
extern double st_time;

/* defined in main.c */
void SIGALRM_handler(int signum);


void init_rand()
{
    srand(time(NULL));
    int i;
    for(i = 0; i < special_var_count; i++)
    {
        special_vars[i].val = malloc(MAX_VAL_LEN+1);
    }
}

char *get_special_var(char *name)
{
    if(!name) return NULL;
    int i, j;
    double t;
    for(i = 0; i < special_var_count; i++)
    {
        if(strcmp(special_vars[i].name, name) == 0)
            break;
    }
    if(i == special_var_count) return NULL;

    switch(i)
    {
        case 0:             /* RANDOM */
            j = rand() % 32768;
            sprintf(special_vars[i].val, "%d", j);
            return special_vars[i].val;

        case 1:             /* SECONDS */
            t = get_cur_time();
            if(!last_sec_at)
            {
                /* secs since shell started */
                t -= st_time;
            }
            else
            {
                /* secs since last SECONDS assignment */
                t -= last_sec_at;
                char *endstr;
                long last_sec = strtol(special_vars[i].val, &endstr, 10);
                t += last_sec;
            }
            sprintf(special_vars[i].val, "%.0f", t);
            return special_vars[i].val;

        case 2:             /* EPOCHREALTIME and EPOCHSECONDS */
        case 3:
            t = get_cur_time();
            if(i == 2) sprintf(special_vars[i].val, "%.0f", t);
            else       sprintf(special_vars[i].val, "%ld", (long)t);
            return special_vars[i].val;
            
        case 4:             /* DIRSTACK */
            if(special_vars[i].val) free(special_vars[i].val);
            special_vars[i].val = purge_dirstackp();
            return special_vars[i].val;
            
        case 5:             /* TPERIOD */
            return special_vars[i].val;
    }
    return NULL;
}

int set_special_var(char *name, char *val)
{
    if(!name) return 0;
    int i, j;
    unsigned int u;
    char *strend = NULL;

    for(i = 0; i < special_var_count; i++)
    {
        if(strcmp(special_vars[i].name, name) == 0)
            break;
    }
    if(i == special_var_count) return 0;
    
    if(!val || !*val) special_vars[i].val[0] = '\0';
    else
    {
        switch(i)
        {
            case 0:
                /* assignment to $RANDOM seeds the random number generator (bash, ksh) */
                u = strtoul(val, &strend, 10);
                if(strend != val) srand(u);
                break;
                
            case 2:
            case 3:
                /* ignore assignments to $EPOCHREALTIME and $EPOCHSECONDS */
                break;
                
            case 4:
                /* pass DIRSTACK assignments to the proper functions */
                load_dirstackp(val);
                break;
            
            case 5:             /* TPERIOD */
                j = strtol(val, &strend, 10);
                /* Start the timer ($TPERIOD is in minutes) */
                struct itimerspec its;
                if(strend != val && j > 0)
                     its.it_value.tv_sec = j*60;
                else its.it_value.tv_sec = 0;
                its.it_value.tv_nsec    = 0;
                its.it_interval.tv_sec  = its.it_value.tv_sec;
                its.it_interval.tv_nsec = its.it_value.tv_nsec;
                if(timer_settime(timerid, 0, &its, NULL) == -1)
                {
                    fprintf(stderr, "%s: failed to start timer: %s\r\n", SHELL_NAME, strerror(errno));
                    return 1;
                }
                /*
                 * fall through to save the value in buffer.
                 */
                
            default:
                if(strlen(val) > MAX_VAL_LEN)
                {
                    /* snprintf count includes the NULL-terminating byte */
                    snprintf(special_vars[i].val, MAX_VAL_LEN+1, "%s", val);
                }
                else
                    sprintf(special_vars[i].val, "%s", val);
                break;
        }
    }
    /* if assigning to SECONDS, record the time */
    if(i == 1)
    {
        last_sec_at = get_cur_time();
    }
    return 1;
}
