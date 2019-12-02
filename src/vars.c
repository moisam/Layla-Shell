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

/*
 * in this file we deal with some shell variables whose values change dynamically
 * when they are accessed by the user, such as $RANDOM and $SECONDS.
 */

/* maximum length of a variable's value (most are numeric, so we don't need a large space) */
#define MAX_VAL_LEN            15

struct var_s special_vars[] =
{
    /*
     * non-POSIX extensions.
     */
    
    /* generate a random integer, uniformly distributed between 0 and 32767 */
    { "RANDOM" , NULL },

    /* number of seconds since shell startup or last SECONDS assignment */
    { "SECONDS", NULL },
    
    /* number of seconds since Unix epoch, floating point */
    { "EPOCHREALTIME", NULL },
    
    /* number of seconds since Unix epoch, integer */
    { "EPOCHSECONDS", NULL },
    
    /* 
     * dirstack entries. bash's version is an array, while tcsh's version is similar to ours,
     * except the variable name is lowercase instead of uppercase.
     */
    { "DIRSTACK", NULL },
    
    /* in tcsh, special alias 'periodic' is run every 'tperiod' minutes */
    { "TPERIOD", NULL },
};
int    special_var_count = sizeof(special_vars)/sizeof(struct var_s);

/* last time $SECONDS was assigned to */
// int    last_sec    = 0;
double last_sec_at = 0;

/* defined in builtins/time.c */
extern double st_time;

/* defined in main.c */
void SIGALRM_handler(int signum);


/*
 * seed the random number generator with the current time value, and initialize
 * the other variables.
 */
void init_rand(void)
{
    srand(time(NULL));
    int i;
    for(i = 0; i < special_var_count; i++)
    {
        special_vars[i].val = malloc(MAX_VAL_LEN+1);
    }
}


/*
 * return the value of the special variable whose name is given.
 * returns NULL if the given name does not refer to one of the special variables.
 */
char *get_special_var(char *name)
{
    if(!name)
    {
        return NULL;
    }
    int i, j;
    double t;
    /* search for the variable name */
    for(i = 0; i < special_var_count; i++)
    {
        if(strcmp(special_vars[i].name, name) == 0)
        {
            break;
        }
    }
    /* variable name not found */
    if(i == special_var_count)
    {
        return NULL;
    }

    /* get the special variable's value */
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
            if(i == 2)
            {
                sprintf(special_vars[i].val, "%.0f", t);
            }
            else
            {
                sprintf(special_vars[i].val, "%ld", (long)t);
            }
            return special_vars[i].val;
            
        case 4:             /* DIRSTACK */
            if(special_vars[i].val)
            {
                free(special_vars[i].val);
            }
            special_vars[i].val = purge_dirstackp();
            return special_vars[i].val;
            
        case 5:             /* TPERIOD */
            return special_vars[i].val;
    }
    return NULL;
}


/*
 * set the value of the special variable name to val.
 * 
 * returns 1 if the variable is found and set, 0 otherwise.
 */
int set_special_var(char *name, char *val)
{
    if(!name)
    {
        return 0;
    }
    int i, j;
    unsigned int u;
    char *strend = NULL;

    /* search for the variable name */
    for(i = 0; i < special_var_count; i++)
    {
        if(strcmp(special_vars[i].name, name) == 0)
        {
            break;
        }
    }
    /* variable name not found */
    if(i == special_var_count)
    {
        return 0;
    }
    
    if(!val || !*val)       /* set the value to an empty string */
    {
        special_vars[i].val[0] = '\0';
    }
    else
    {
        switch(i)
        {
            case 0:
                /* assignment to $RANDOM seeds the random number generator (bash, ksh) */
                u = strtoul(val, &strend, 10);
                if(strend != val)
                {
                    srand(u);
                }
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
                {
                     its.it_value.tv_sec = j*60;
                }
                else
                {
                    its.it_value.tv_sec = 0;
                }
                its.it_value.tv_nsec    = 0;
                its.it_interval.tv_sec  = its.it_value.tv_sec;
                its.it_interval.tv_nsec = its.it_value.tv_nsec;
                if(timer_settime(timerid, 0, &its, NULL) == -1)
                {
                    fprintf(stderr, "%s: failed to start timer: %s\n", SHELL_NAME, strerror(errno));
                    return 1;
                }
                /* fall through to save the value in buffer */
                __attribute__((fallthrough));
                
            default:
                if(strlen(val) > MAX_VAL_LEN)
                {
                    /* snprintf count includes the NULL-terminating byte */
                    snprintf(special_vars[i].val, MAX_VAL_LEN+1, "%s", val);
                }
                else
                {
                    sprintf(special_vars[i].val, "%s", val);
                }
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


/*
 * retrive the string value of a symbol table entry (presumably a shell variable).
 * if name is not defined (or is null), return def_val, which can be NULL.
 * this function doesn't return empty strings (name must be set to a non-empty value).
 * you can bypass this by passing "" as the value of def_val;
 */
char *get_shell_varp(char *name, char *def_val)
{
    struct symtab_entry_s *entry = get_symtab_entry(name);
    return (entry && entry->val && entry->val[0]) ? entry->val : def_val;
}


/*
 * retrive the integer value of a symbol table entry (presumably a shell variable).
 * if name is not defined (or is null), return def_val, which usually is passed as 0.
 */
int get_shell_vari(char *name, int def_val)
{
    return (int)get_shell_varl(name, def_val);
}


/*
 * same, but return long (not int) values.
 */
long get_shell_varl(char *name, int def_val)
{
    struct symtab_entry_s *entry = get_symtab_entry(name);
    if(entry && entry->val && entry->val[0])
    {
        char *strend = NULL;
        long i = strtol(entry->val, &strend, 10);
        if(strend == entry->val)
        {
            return def_val;
        }
        return i;
    }
    return def_val;
}


/*
 * quick set of var value (without needing to retrieve symtab entry ourselves).
 */
void set_shell_varp(char *name, char *val)
{
    /* get the entry */
    struct symtab_entry_s *entry = get_symtab_entry(name);
    /* add to local symbol table */
    if(!entry)
    {
        entry = add_to_symtab(name);
    }
    /* set the entry's value */
    if(entry)
    {
        symtab_entry_setval(entry, val);
    }
}
