/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY     "unset"

void do_unset(char *name, struct symtab_entry_s *entry);


int unset(int argc, char *argv[])
{
    if(argc == 1) return 0;
    int i      = 0;
    int is_var = 1;
    int res    = 0;

    while(++i < argc)
    {
        char *arg = argv[i];
        if(arg[0] == '-')
        {
            if(strcmp(arg, "-v") == 0) { is_var = 1; continue; }
            if(strcmp(arg, "-f") == 0) { is_var = 0; continue; }
            fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, arg);
            return 2;
        }
        /* check we are not trying to unset one of the special variables */
        if(strlen(arg) == 1)
        {
            switch(arg[0])
            {
                case '@':
                case '*':
                case '#':
                case '?':
                case '-':
                case '$':
                case '!':
                case '0':
                    fprintf(stderr, "%s: unable to unset '%s': special parameter\r\n", UTILITY, arg);
                    res = 1;
                    continue;
            }
        }
        if(is_var)
        {
            struct symtab_entry_s *entry = get_symtab_entry(arg);
            if(!entry) continue;
            //if(entry->val_type == SYM_FUNC  ) continue;
            if(entry->flags & FLAG_READONLY )
            {
                fprintf(stderr, "%s: unable to unset '%s': readonly variable\r\n", UTILITY, arg);
                res = 1;
                continue;
            }
            if(is_special_param(entry->name))
            {
                fprintf(stderr, "%s: unable to unset '%s': special parameter\r\n", UTILITY, arg);
                res = 1;
                continue;
            }
            do_unset(arg, entry);
        }
        else
        {
            //if(entry->val_type != SYM_FUNC) continue;
            //do_unset(arg, entry);
            unset_func(arg);
        }
        if(unsetenv(arg) != 0)
        {
            fprintf(stderr, "%s: unable to unset '%s': %s\r\n", UTILITY, arg, strerror(errno));
            res = 1;
        }
    }
    return res;
}

/*
 * localvar_unset causes variables defined in previous scopes to be unset for the duration
 * of the current function call. after the call finishes, variables are unmasked, they
 * retrieve their previous values. we achieve this effect by simply adding a NULL-valued
 * entry to the local symbol table, masking the global symbol table's entry. we don't remove
 * the variable from the local symbol table as this might unmask a global variable with the
 * same name.
 * 
 * TODO: this might not give the desired effect as we will be working on our own symbol table,
 *       not our caller's (e.g. a function or script).
 */
void do_unset(char *name, struct symtab_entry_s *entry)
{
    debug (NULL);
    if(optionx_set(OPTION_LOCAL_VAR_UNSET))
    {
        if(get_local_symtab_entry(name) == entry) symtab_entry_setval(entry, NULL);
        else
        {
            add_to_symtab(name);
            symtab_entry_setval(entry, NULL);
        }
    }
    else rem_from_symtab(entry);
}
