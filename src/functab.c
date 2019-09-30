/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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

#include "cmd.h"
#include "symtab/symtab.h"

/*
 * the global functions table.. every defined function has an entry in
 * this table.
 */
struct symtab_s *func_table = NULL;


/*
 * initialize the functions table.. called on shell startup.
 */
void init_functab()
{
    func_table = new_symtab(0);
}


/*
 * return the function definition of the given function name, or NULL
 * if name does not refer to a defined function.
 */
struct symtab_entry_s *get_func(char *name)
{
    if(!func_table)
    {
        return NULL;
    }
    return __do_lookup(name, func_table);
}


/*
 * add the given function name to the functions table.
 *
 * returns the entry for the newly added function, or NULL if the functions
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
    if((entry = __do_lookup(name, func_table)))
    {
        /* return the existing entry */
        return entry;
    }
    /* add a new entry */
    entry = __add_to_symtab(name, func_table);
    if(entry)
    {
        entry->val_type = SYM_FUNC;
        /*// all functions are marked for export */
    }
    return entry;
}


/*
 * unset a function definition, removing the function from the functions table.
 */
void unset_func(char *name)
{
    struct symtab_entry_s *func = get_func(name);
    if(!func)
    {
        return;
    }
    __rem_from_symtab(func, func_table);
}
