/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: enable.c
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
#include <string.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY         "enable"


/*
 * print a one-line message indicating the status of the builtin utility (i.e. is it
 * enabled or disabled).
 */
static inline void status(struct builtin_s *builtin)
{
    printf("enable %s%s\n", 
           flag_set(builtin->flags, BUILTIN_ENABLED) ? "" : "-n ", 
           builtin->name);
}


/*
 * print the enabled/disabled status of builtin utilities.. the printed list depends
 * on the value of the which parameter:
 *
 *   'a' : print the status of all builtin utilities (regular and special, enabled and
 *         disabled)
 *   'e' : print the status of enabled builtin utilities
 *   'r' : print the status of regular builtin utilities
 *   's' : print the status of special builtin utilities
 */
void enable_list(char which)
{
    int i;
    switch(which)
    {
        case 's':           /* list the special builtins only */
            for(i = 0; i < special_builtin_count; i++)
            {
                status(&special_builtins[i]);
            }
            break;
            
        case 'r':           /* list the regular builtins only */
            for(i = 0; i < regular_builtin_count; i++)
            {
                status(&regular_builtins[i]);
            }
            break;
            
        case 'e':           /* list the enabled builtins only */
            for(i = 0; i < special_builtin_count; i++)
            {
                if(flag_set(special_builtins[i].flags, BUILTIN_ENABLED))
                {
                    status(&special_builtins[i]);
                }
            }
            for(i = 0; i < regular_builtin_count; i++)
            {
                if(flag_set(regular_builtins[i].flags, BUILTIN_ENABLED))
                {
                    status(&regular_builtins[i]);
                }
            }
            break;
                
        default:           /* list all builtins */
            for(i = 0; i < special_builtin_count; i++)
            {
                status(&special_builtins[i]);
            }
            for(i = 0; i < regular_builtin_count; i++)
            {
                status(&regular_builtins[i]);
            }
            break;
    }
}


/*
 * the enable builtin utility (non-POSIX).. used to enable, disable and print the
 * status of different builtin utilities.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help enable` or `enable -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int enable(int argc, char **argv)
{
    int v = 1, c, res = 0;
    int print_attribs = 0;
    int disable       = 0;
    int print_all     = 0;
    int spec_only     = 0;
    int reg_only      = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "ahnprsv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_ENABLE, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                print_attribs = 1;
                break;
                
            case 'n':               /* disable builtin */
                disable       = 1;
                break;
                
            case 'a':               /* print all */
                print_all     = 1;
                break;
                
            case 'r':               /* print only regular builtins */
                reg_only      = 1;
                break;
                
            case 's':               /* print only special builtins */
                spec_only     = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    /* -a supersedes -s and -r */
    if(print_all)
    {
        spec_only = 0, reg_only = 0;
    }
    /*
     *  if no arguments, list the utilities depending on the given option: all utilities (-a),
     *  special builtins (-s), regular builtins (-r), or only the enabled builtins (if none
     *  of the above options is provided).
     */
    if(v >= argc)
    {
        enable_list(print_all ? 'a' :
                    spec_only ? 's' :
                    reg_only  ? 'r' : 'e');
        return 0;
    }
    
    int print_only = (print_all || print_attribs || reg_only || spec_only);
    struct builtin_s *builtin = NULL;
    int i;
    /* process the arguments. each argument gives the name of a builtin utility to print */
    for( ; v < argc; v++, builtin = NULL)
    {
        char *arg = argv[v];
        /* first check if the argument refers to a special builtin utility */
        if(print_all || spec_only)
        {
            for(i = 0; i < special_builtin_count; i++)
            {
                if(strcmp(special_builtins[i].name, arg) == 0)
                {
                    builtin = &special_builtins[i];
                    break;
                }
            }
        }
        /* the argument is not a special builtin utility. check if it is a regular builtin */
        if(!builtin && (print_all || reg_only))
        {
            for(i = 0; i < regular_builtin_count; i++)
            {
                if(strcmp(regular_builtins[i].name, arg) == 0)
                {
                    builtin = &regular_builtins[i];
                    break;
                }
            }
        }
        /* argument is neither a special nor a regular builtin utility */
        if(!builtin)
        {
            fprintf(stderr, "%s: cannot find %s: not a shell builtin\n", UTILITY, arg);
            res = 2;
            continue;
        }
        /* print the utility's status */
        if(print_only)
        {
            status(builtin);
        }
        else
        {
            /* disable the builtin (the -n option) */
            if(disable)
            {
                builtin->flags &= ~BUILTIN_ENABLED;
            }
            else
            {
                /* is this shell restricted? */
                if(startup_finished && option_set('r'))
                {
                    /* bash says r-shells can't enable disabled builtins */
                    fprintf(stderr, "%s: can't enable builtin: restricted shell\n", UTILITY);
                    return 2;
                }
                /* enable the builtin */
                builtin->flags |=  BUILTIN_ENABLED;
            }
        }
    }
    return res;
}
