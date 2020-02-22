/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: unset.c
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

/* macro definitions needed to use unsetenv() */
#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY     "unset"

#define UNSET_PRINT_ERROR(arg, msg) \
    PRINT_ERROR("%s: unable to unset '%s': %s\n", UTILITY, arg, msg);


/*
 * the unset builtin utility (non-POSIX).. used to unset shell variables.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help unset` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int unset_builtin(int argc, char **argv)
{
    int vars = 0, funcs = 0, both = 1;
    int c, res = 0, v = 1;

    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "fv" : "fhv";

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &UNSET_BUILTIN, 0);
                return 0;

            case 'f':
                funcs = 1;
                both = 0;
                break;

            case 'v':
                vars = 1;
                both = 0;
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /*
     * pop our local symbol table, so that we will unset variables in the caller's
     * scope.. this will give the caller the intended behaviour, as calling unset
     * should unset variables in the caller's scope, not in the unset utility's scope.
     */
    //struct symtab_s *symtab = symtab_stack_pop();
    
    for( ; v < argc; v++)
    {
        char *arg = argv[v];

        /* ignore empty arguments */
        if(!arg || !*arg)
        {
            continue;
        }

        /* remove the shell variable with the given name */
        if(vars || both)
        {
            struct symtab_entry_s *entry;
            if(is_special_param(arg))
            {
                UNSET_PRINT_ERROR(arg, "special parameter");
                res = 1;
                continue;
            }

            if(is_pos_param(arg))
            {
                UNSET_PRINT_ERROR(arg, "positional parameter");
                res = 1;
                continue;
            }
            
            /*
             * 'localvar_unset' causes variables defined in previous scopes to be unset for the duration
             * of the current function call.. after the call finishes, variables are unmasked, they
             * retrieve their previous values.. we achieve this effect by simply adding a NULL-valued
             * entry to the local symbol table, masking the global symbol table's entry.. we don't remove
             * the variable from the local symbol table as this might unmask a global variable with the
             * same name.
             */
            if(optionx_set(OPTION_LOCAL_VAR_UNSET))
            {
                if((entry = get_local_symtab_entry(arg)) == NULL)
                {
                    entry = add_to_symtab(arg);
                }

                if(entry->flags & FLAG_READONLY)
                {
                    READONLY_ASSIGN_ERROR(UTILITY, arg);
                    //UNSET_PRINT_ERROR(arg, "readonly variable");
                    res = 1;
                    continue;
                }
                symtab_entry_setval(entry, NULL);
                continue;
            }
            else
            {
                if((entry = get_symtab_entry(arg)))
                {
                    if(flag_set(entry->flags, FLAG_READONLY))
                    {
                        READONLY_ASSIGN_ERROR(UTILITY, arg);
                        //UNSET_PRINT_ERROR(arg, "readonly variable");
                        res = 1;
                        continue;
                    }
                    rem_from_any_symtab(entry);
                
                    /* now remove the variable/function definition from the environment */
                    unsetenv(arg);
                    continue;
                }
            }
        }

        /* remove the shell function with the given name */
        if(funcs || both)
        {
            if(!unset_func(arg))
            {
                //UNSET_PRINT_ERROR(arg, "unknown function");
                //res = 1;
                continue;
            }
            /* now remove the variable/function definition from the environment */
            unsetenv(arg);
        }
    }
    /*
     * restore our local symbol table, so that do_simple_command() or whoever called us
     * can pop the local symbol table correctly.
     */
    //symtab_stack_push(symtab);
    return res;
}
