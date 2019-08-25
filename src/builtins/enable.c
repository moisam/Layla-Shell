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


static inline void status(struct builtin_s *builtin)
{
    printf("enable %s%s\n", 
           flag_set(builtin->flags, BUILTIN_ENABLED) ? "" : "-n ", 
           builtin->name);
}


void enable_list(char which)
{
    int i;
    switch(which)
    {
        case 's':           /* list the special builtins only */
            for(i = 0; i < special_builtin_count; i++) status(&special_builtins[i]);
            break;
            
        case 'r':           /* list the regular builtins only */
            for(i = 0; i < regular_builtin_count; i++) status(&regular_builtins[i]);
            break;
            
        case 'e':           /* list the enabled builtins only */
            for(i = 0; i < special_builtin_count; i++)
            {
                if(flag_set(special_builtins[i].flags, BUILTIN_ENABLED))
                    status(&special_builtins[i]);
            }
            for(i = 0; i < regular_builtin_count; i++)
            {
                if(flag_set(regular_builtins[i].flags, BUILTIN_ENABLED))
                    status(&regular_builtins[i]);
            }
            break;
                
        default:           /* list all builtins */
            for(i = 0; i < special_builtin_count; i++) status(&special_builtins[i]);
            for(i = 0; i < regular_builtin_count; i++) status(&regular_builtins[i]);
            break;
    }
}


int enable(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c, res = 0;
    int print_attribs = 0;
    int disable       = 0;
    int print_all     = 0;
    int spec_only     = 0;
    int reg_only      = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
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
    if(c == -1) return 2;

    if(print_all) spec_only = 0, reg_only = 0;

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
    
    for( ; v < argc; v++, builtin = NULL)
    {
        char *arg = argv[v];
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
        if(!builtin)
        {
            if(print_all || reg_only)
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
        }
        if(!builtin)
        {
            fprintf(stderr, "%s: cannot find %s: not a shell builtin\n", UTILITY, arg);
            res = 2;
            continue;
        }
        
        if(print_only) status(builtin);
        else
        {
            if(disable) builtin->flags &= ~BUILTIN_ENABLED;
            else
            {
                /* is this shell restricted? */
                if(startup_finished && option_set('r'))
                {
                    /* bash says r-shells can't enable disabled builtins */
                    fprintf(stderr, "%s: can't enable builtin: restricted shell\r\n", UTILITY);
                    return 2;
                }
                builtin->flags |=  BUILTIN_ENABLED;
            }
        }
    }
    return res;
}
