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

struct symtab_s *func_table = NULL;


void init_functab()
{
    func_table = new_symtab(0);
}

struct symtab_entry_s *get_func(char *name)
{
    if(!func_table) return NULL;
    return __do_lookup(name, func_table);
}

struct symtab_entry_s *add_func(char *name)
{
    if(!func_table) return NULL;
    /* do not duplicate an existing entry */
    struct symtab_entry_s *entry = NULL;
    if((entry = __do_lookup(name, func_table))) return entry;
    entry = __add_to_symtab(name, func_table);
    if(entry) entry->val_type = SYM_FUNC;
    return entry;
}

void unset_func(char *name)
{
    struct symtab_entry_s *func = get_func(name);
    if(!func) return;
    __rem_from_symtab(func, func_table);
}
