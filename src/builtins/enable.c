/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY         "enable"

#define FLAG_DISABLED   (1 << 0)
#define FLAG_REGULAR    (1 << 1)
#define FLAG_SPECIAL    (1 << 2)


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
 * return 1 if the given builtin utility is of the given type, 0 otherwise.
 * see the enable_builtin_list() function below for the possible values of
 * the 'which' argument.
 */
#if 0
static inline int builtin_of_type(char which, struct builtin_s *utility)
{
    int is_special = flag_set(utility->flags, BUILTIN_SPECIAL_BUILTIN);
    return ((which == 's' && is_special) || (which == 'r' && !is_special) ||
            (which == 'e' && flag_set(utility->flags, BUILTIN_ENABLED)) ||
            (which == 'n' && !flag_set(utility->flags, BUILTIN_ENABLED)) ||
            (which == 'a'));
}
#endif

static inline int match_builtin(struct builtin_s *utility, int special, int regular, int disabled)
{
    int is_special = flag_set(utility->flags, BUILTIN_SPECIAL_BUILTIN);

    if((special && is_special) || (regular && !is_special))
    {
       return (disabled == !flag_set(utility->flags, BUILTIN_ENABLED));
    }

    return 0;
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
void enable_builtin_list(int which, char **names)
// void enable_builtin_list(char which, char **names)
{
    struct builtin_s *u;
    int list_special = flag_set(which, FLAG_SPECIAL);
    int list_regular = flag_set(which, FLAG_REGULAR);
    int list_disabled = flag_set(which, FLAG_DISABLED);
    
    if(names && *names)
    {
        char **p;
        for(p = names; *p; p++)
        {
            if((u = is_builtin(*p)) && match_builtin(u, list_special, list_regular, list_disabled))
            //if((u = is_builtin(*p)) && builtin_of_type(which, u))
            {
                status(u);
            }
        }
    }
    else
    {
        for(u = shell_builtins; u->name; u++)
        {
            if(match_builtin(u, list_special, list_regular, list_disabled))
            //if(builtin_of_type(which, u))
            {
                status(u);
            }
        }
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

int enable_builtin(int argc, char **argv)
{
    int v = 1, c, res = 0;
    int print_attribs = 0;
    int disable       = 0;
    int print_all     = 0;
    int spec_only     = 0;
    int reg_only      = 0;
    int print_flags   = 0;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "ahnprsv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &ENABLE_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                print_attribs = 1;
                break;
                
            case 'n':               /* disable builtin */
                disable = 1;
                print_flags |= FLAG_DISABLED;
                break;
                
            case 'a':               /* print all */
                print_all = 1;
                print_flags |= (FLAG_REGULAR | FLAG_SPECIAL);
                break;
                
            case 'r':               /* print only regular builtins */
                reg_only = 1;
                print_flags |= FLAG_REGULAR;
                print_flags &= ~FLAG_SPECIAL;
                break;
                
            case 's':               /* print only special builtins */
                spec_only = 1;
                print_flags |= FLAG_SPECIAL;
                print_flags &= ~FLAG_REGULAR;
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
        spec_only = 0;
        reg_only = 0;
        print_flags |= (FLAG_REGULAR | FLAG_SPECIAL);
    }
    
    /*
     *  if no arguments, list the utilities depending on the given option: all utilities (-a),
     *  special builtins (-s), regular builtins (-r), or only the enabled builtins (if none
     *  of the above options is provided).
     */
    if(v >= argc || print_attribs)
    {
        enable_builtin_list(print_flags ? : (FLAG_REGULAR | FLAG_SPECIAL), &argv[v]);
        /*
        enable_builtin_list(print_all ? 'a' :
                            spec_only ? 's' :
                            reg_only  ? 'r' :
                            disable   ? 'n' :
                            v >= argc ? 'e' : 'a',
                            &argv[v]);
        */
        return 0;
    }
    
    /* process the arguments. each argument gives the name of a builtin utility to print */
    int restrict_shell = startup_finished && option_set('r');
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        struct builtin_s *utility = is_builtin(arg);

        /* argument is neither a special nor a regular builtin utility */
        if(!utility)
        {
            PRINT_ERROR("%s: cannot find %s: not a shell builtin\n", UTILITY, arg);
            res = 2;
            continue;
        }
        
        int is_special = flag_set(utility->flags, BUILTIN_SPECIAL_BUILTIN);
        
        /* first check if the argument matches the given criteria */
        if((spec_only && !is_special) || (reg_only && is_special))
        {
            PRINT_ERROR("%s: not a %s shell builtin: %s\n", UTILITY, 
                        spec_only ? "special" : "regular", arg);
            res = 2;
            continue;
        }

        /* disable the builtin (the -n option) */
        if(disable)
        {
            utility->flags &= ~BUILTIN_ENABLED;
        }
        else
        {
            /* is this shell restricted? */
            if(restrict_shell)
            {
                /* bash says r-shells can't enable disabled builtins */
                PRINT_ERROR("%s: can't enable builtin: restricted shell\n", UTILITY);
                return 2;
            }
            
            /* enable the builtin */
            utility->flags |=  BUILTIN_ENABLED;
        }
    }
    return res;
}
