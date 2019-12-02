/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: setenv.c
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

/* macro definitions needed to use setenv() */
#define _POSIX_C_SOURCE 200112L

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

extern char **environ;

#define UTILITY             "setenv"


/*
 * when we set or change the value of an environment variable, we set or change
 * the value of the corresponding shell variable too.
 */
static inline void set_entry(char *name, char *val)
{
    struct symtab_entry_s *entry = add_to_symtab(name);
    if(name)
    {
        symtab_entry_setval(entry, val);
    }
}


/*
 * the setenv builtin utility (non-POSIX).. used to set or change the value of one or
 * more environment variables.
 *
 * the setenv utility is a tcsh non-POSIX extension. bash doesn't have it.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help setenv` or `setenv -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int setenv_builtin(int argc, char **argv)
{
    int v = 1, c;
    char separator = '\n';
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_SETENV, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* no arguments. print the list of environment variables */
    if(v >= argc)
    {
        char **p2 = environ;
        while(*p2)
        {
            printf("%s%c", *p2, separator);
            p2++;
        }
        return 0;
    }

    /* set the given environment variables */
    int res = 0;
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        char *eq  = strchr(arg, '=');
        if(!eq)
        {
            res = !setenv(arg, "", 1) ? res : 1;
            set_entry(arg, NULL);
        }
        else
        {
            char *val = eq+1;
            *eq = '\0';
            if(*val == '\0')
            {
                res = !setenv(arg, "", 1) ? res : 1;
                set_entry(arg, NULL);
            }
            else
            {
                res = !setenv(arg, val, 1) ? res : 1;
                set_entry(arg, val);
            }
            *eq = '=';
        }
    }
    return res;
}
