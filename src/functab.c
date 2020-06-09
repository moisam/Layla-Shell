/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: functab.c
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
#include "cmd.h"
#include "symtab/symtab.h"
#include "builtins/builtins.h"

/*
 * The global functions table. Every defined function has an entry in
 * this table.
 */
struct symtab_s *func_table = NULL;


/*
 * Initialize the functions table. Called on shell startup.
 */
void init_functab(void)
{
    func_table = new_symtab(0);
}


/*
 * Return the function definition of the given function name, or NULL
 * if name does not refer to a defined function.
 */
struct symtab_entry_s *get_func(char *name)
{
    if(!func_table)
    {
        return NULL;
    }
    return do_lookup(name, func_table);
}


/*
 * Add the given function name to the functions table.
 *
 * Returns the entry for the newly added function, or NULL if the functions
 * table is not initialized or if the function couldn't be added to the table.
 */
struct symtab_entry_s *add_func(char *name)
{
    if(!func_table)
    {
        return NULL;
    }
    /* do not duplicate an existing entry */
    struct symtab_entry_s *entry = NULL;
    if((entry = do_lookup(name, func_table)))
    {
        /* return the existing entry */
        return entry;
    }
    /* add a new entry */
    entry = add_to_any_symtab(name, func_table);
    if(entry)
    {
        entry->val_type = SYM_FUNC;
        /*// all functions are marked for export */
    }
    return entry;
}


/*
 * Unset a function definition, removing the function from the functions table.
 * 
 * Returns 1 on success, 0 on failure.
 */
int unset_func(char *name)
{
    struct symtab_entry_s *func = get_func(name);
    if(!func)
    {
        return 0;
    }

    if(flag_set(func->flags, FLAG_READONLY))
    {
        UNSET_PRINT_ERROR(name, "readonly function");
        return 0;
    }
    
    return rem_from_symtab(func, func_table);
}


/*
 * Print the functions with the given flag (readonly or export).
 */
void print_func_attribs(unsigned int flag)
{
    /* use an alpha list to sort variables alphabetically */
    struct alpha_list_s list;
    init_alpha_list(&list);
    
    /* the flag char we'll print */
    char fchar = (flag == FLAG_EXPORT) ? 'x' : 'r';

#ifdef USE_HASH_TABLES

    if(func_table->used)
    {
        struct symtab_entry_s **h1 = func_table->items;
        struct symtab_entry_s **h2 = func_table->items + func_table->size;
        for( ; h1 < h2; h1++)
        {
            struct symtab_entry_s *entry = *h1;

#else

        struct symtab_entry_s *entry  = func_table->first;

#endif

            while(entry)
            {
                if(flag_set(entry->flags, flag))
                {
                    char *str = NULL;
                    if(!entry->val)
                    {
                        /* no val, print only the name */
                        str = alpha_list_make_str("declare -%c -f %s", fchar, entry->name);
                    }
                    else
                    {
                        /* print the name=val string */
                        char *val = quote_val(entry->val, 1, 0);
                        if(val)
                        {
                            str = alpha_list_make_str("declare -%c -f %s=%s", fchar, entry->name, val);
                            free(val);
                        }
                        else
                        {
                            str = alpha_list_make_str("declare -%c -f %s=", fchar, entry->name);
                        }
                    }
                    add_to_alpha_list(&list, str);
                }
                entry = entry->next;
            }

#ifdef USE_HASH_TABLES

        }
    }

#endif

    print_alpha_list(&list);
    free_alpha_list(&list);
}
