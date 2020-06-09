/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: local.c
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

#include <stdio.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY             "local"


/*
 * The local builtin utility (non-POSIX). Used to declare local variables.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help local` or `local -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int local_builtin(int argc, char **argv)
{
    /*
     * If we saved the passed variables straight away, they will go into our
     * local symbol table, which will eventually get popped off the stack when
     * we go back to do_simple_command() in backend.c, which is useless. What
     * we want is to add the variables to our caller's symbol table, e.g. a script
     * or function that wants to declare local vars. This is why we pop off our
     * local symtab, add vars to our caller's symtab, then push back our (empty)
     * symtab, which will be popped off when we return. This is similar to what we
     * do in declare.c.
     */
    struct symtab_s *symtab = symtab_stack_pop();
    if(get_local_symtab()->level == 0)
    {
        PRINT_ERROR("%s: cannot declare local variables at the global scope\n", UTILITY);
        symtab_stack_add(symtab);
        return 2;
    }

    int res = do_declare(argc, argv, 0);

    symtab_stack_add(symtab);
    return res;
}
