/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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
#include "include/cmd.h"
#include "include/debug.h"
#include "builtins/builtins.h"

/*
 * In this file we deal with some shell variables whose values change dynamically
 * when they are accessed by the user, such as $RANDOM and $SECONDS.
 *
 * These 'special' variables are non-POSIX extensions and include:
 * 
 * RANDOM        - generate a random integer, uniformly distributed between 0 
 *                 and 32767
 * SECONDS       - number of seconds since shell startup or last SECONDS assignment
 * EPOCHREALTIME - number of seconds since Unix epoch, floating point
 * EPOCHSECONDS  - number of seconds since Unix epoch, integer
 * DIRSTACK      - dirstack entries. bash's version is an array, while tcsh's 
 *                 version is similar to ours, except the variable name is 
 *                 lowercase instead of uppercase.
 * TPERIOD       - in tcsh, special alias 'periodic' is run every 'tperiod' minutes
 */
char *special_var_names[] =
{
    "RANDOM", "SECONDS", "EPOCHREALTIME", "EPOCHSECONDS", "DIRSTACK", "TPERIOD"
};

int special_var_count = 6;

/* maximum length of a variable's value (most are numeric, so we don't need a large space) */
#define MAX_VAL_LEN            15

/* last time $SECONDS was assigned to */
// int    last_sec    = 0;
double last_sec_at = 0;

/* defined in builtins/times.c */
extern double shell_start_time;


/*
 * Seed the random number generator with the current time value, and initialize
 * the other variables.
 */
void init_rand(void)
{
    srand(time(NULL));
}


/*
 * Return the value of the special variable whose name is given.
 * old_val contains the current value of the variable, which is used to 
 * calculate the new value of some variables, such as $SECONDS.
 * 
 * Returns the malloc'd var value, or NULL if the given name does not refer 
 * to one of the special variables.
 */
char *get_special_var(char *name, char *old_val)
{
    if(!name)
    {
        return NULL;
    }

    int j;
    double t;
    char buf[MAX_VAL_LEN+1];
    buf[0] = '\0';
    
    if(strcmp(name, "RANDOM") == 0)
    {
        j = rand() % 32768;
        sprintf(buf, "%d", j);
    }
    else if(strcmp(name, "SECONDS") == 0)
    {
        t = get_cur_time();
        if(!last_sec_at)
        {
            /* secs since shell started */
            t -= shell_start_time;
        }
        else
        {
            /* secs since last SECONDS assignment */
            t -= last_sec_at;
            char *endstr;
            long last_sec = strtol(old_val, &endstr, 10);
            if(*endstr)
            {
                last_sec = 0;
            }
            t += last_sec;
        }
        sprintf(buf, "%.0f", t);
    }
    else if(strcmp(name, "EPOCHREALTIME") == 0)
    {
        t = get_cur_time();
        sprintf(buf, "%.0f", t);
    }
    else if(strcmp(name, "EPOCHSECONDS") == 0)
    {
        t = get_cur_time();
        sprintf(buf, "%ld", (long)t);
    }
    else if(strcmp(name, "DIRSTACK") == 0)
    {
        return purge_dirstackp();
    }
    /* NOTE: ignore $TPERIOD, so caller will use the value it already has */
    /*
    else if(strcmp(name, "TPERIOD") == 0)
    {
        ;
    }
    */
    
    if(buf[0] == '\0')
    {
        return NULL;
    }
    
    return __get_malloced_str(buf);
}


/*
 * Set the value of the special variable name to val.
 * 
 * Returns 1 if the variable is found and set, 0 otherwise.
 */
int set_special_var(char *name, char *val)
{
    int j;
    unsigned int u;
    char *strend = NULL;

    if(!name)
    {
        return 0;
    }

    if(!val || !*val)       /* empty string */
    {
        return 1;
    }
    
    if(strcmp(name, "RANDOM") == 0)
    {
        /* assignment to $RANDOM seeds the random number generator (bash, ksh) */
        u = strtoul(val, &strend, 10);
        if(!*strend)
        {
            srand(u);
        }
    }
    else if(strcmp(name, "SECONDS") == 0)
    {
        /* if assigning to SECONDS, record the time */
        last_sec_at = get_cur_time();
    }
    /* NOTE: ignore assignments to $EPOCHREALTIME and $EPOCHSECONDS */
    /*
    else if(strcmp(name, "EPOCHREALTIME") == 0)
    {
        ;
    }
    else if(strcmp(name, "EPOCHSECONDS") == 0)
    {
        ;
    }
    */
    else if(strcmp(name, "DIRSTACK") == 0)
    {
        /* pass DIRSTACK assignments to the proper functions */
        load_dirstackp(val);
    }
    else if(strcmp(name, "TPERIOD") == 0)
    {
        j = strtol(val, &strend, 10);
        /* Start the timer ($TPERIOD is in minutes) */
        struct itimerspec its;
        if(!*strend && j > 0)
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
            PRINT_ERROR(SHELL_NAME, "failed to start timer: %s", strerror(errno));
        }
    }
    
    return 1;
}


/*
 * Retrive the string value of a symbol table entry (presumably a shell variable).
 * If name is not defined (or is null), return def_val, which can be NULL.
 * This function doesn't return empty strings (name must be set to a non-empty value).
 * You can bypass this by passing "" as the value of def_val.
 */
char *get_shell_varp(char *name, char *def_val)
{
    struct symtab_entry_s *entry = get_symtab_entry(name);
    return (entry && entry->val && entry->val[0]) ? entry->val : def_val;
}


/*
 * Retrive the integer value of a symbol table entry (presumably a shell variable).
 * If name is not defined (or is null), return def_val, which usually is passed as 0.
 */
int get_shell_vari(char *name, int def_val)
{
    return (int)get_shell_varl(name, def_val);
}


/*
 * Same, but return long (not int) values.
 */
long get_shell_varl(char *name, int def_val)
{
    struct symtab_entry_s *entry = get_symtab_entry(name);
    if(entry && entry->val && entry->val[0])
    {
        char *strend = NULL;
        long i = strtol(entry->val, &strend, 10);
        if(strend == entry->val || *strend)
        {
            return def_val;
        }
        return i;
    }
    return def_val;
}


/*
 * Quick set of var value (without needing to retrieve symtab entry ourselves).
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
    symtab_entry_setval(entry, val);
}


void set_shell_vari(char *name, int val)
{
    char buf[32];
    sprintf(buf, "%d", val);
    set_shell_varp(name, buf);
}


/*
 * Set the value of the underscore '$_' special parameter .
 */
void set_underscore_val(char *val, int set_env)
{
    struct symtab_entry_s *entry = add_to_symtab("_");
    symtab_entry_setval(entry, val);
    if(set_env)
    {
        setenv("_", val, 1);
    }
}


/*
 * Save the value of $OPTIND and reset it to NULL, so that builtin utilities can
 * call getopts() to parse their options. When a utility finishes execution, it
 * should call reset_OPTIND() to restore the variable to its previous value.
 */
static char  *saved_OPTIND_val = NULL;
static char  *saved_OPTSUB_val = NULL;
// static char **saved_agrv       = NULL;
static int    saved_argi       = 0;
static int    saved_argsub     = 0;

void save_OPTIND(void)
{
    /* get $OPTIND's entry */
    struct symtab_entry_s *entry = get_symtab_entry("OPTIND");
    
    /* set $OPTIND's value to NULL */
    saved_OPTIND_val = (entry && entry->val) ? get_malloced_str(entry->val) : NULL;
    symtab_entry_setval(entry, NULL);
    
    /* get $OPTSUB's entry */
    entry = get_symtab_entry("OPTSUB");

    /* set $OPTSUB's value to NULL */
    saved_OPTSUB_val = (entry && entry->val) ? get_malloced_str(entry->val) : NULL;
    symtab_entry_setval(entry, NULL);
    
    /* reset argi (see args.c), which is used by getopts() */
    saved_argi    = internal_argi;
    saved_argsub  = internal_argsub;
    internal_argi = 1;
    internal_argsub = 0;
}


void reset_OPTIND(void)
{
    /* get $OPTIND's entry */
    struct symtab_entry_s *entry = get_symtab_entry("OPTIND");
    
    /* set $OPTIND's value to NULL */
    symtab_entry_setval(entry, saved_OPTIND_val);
    if(saved_OPTIND_val)
    {
        free_malloced_str(saved_OPTIND_val);
        saved_OPTIND_val = NULL;
    }
    
    /* get $OPTSUB's entry */
    entry = get_symtab_entry("OPTSUB");

    /* set $OPTSUB's value to NULL */
    symtab_entry_setval(entry, saved_OPTSUB_val);
    if(saved_OPTSUB_val)
    {
        free_malloced_str(saved_OPTSUB_val);
        saved_OPTSUB_val = NULL;
    }
    
    /* reset argi (see args.c), which is used by getopts() */
    internal_argi   = saved_argi;
    internal_argsub = saved_argsub;
}
