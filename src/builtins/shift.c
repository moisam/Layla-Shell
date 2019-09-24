/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: shift.c
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
#include <unistd.h>
#include <string.h>
#include "../cmd.h"
#include "setx.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY     "shift"


/*
 * the shift builtin utility (POSIX).. used to shift positional arguments by a given
 * number, or by 1 if no argument is given.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help shift` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int shift(int argc, char *argv[])
{
    if(argc > 2)
    {
        fprintf(stderr, "%s: too many arguments\n", UTILITY);
        return 1;
    }
    struct symtab_entry_s *hash = get_symtab_entry("#");
    int params = atoi(hash->val);
    int shift = 1;
    if(argc == 2)
    {
        shift = atoi(argv[1]);
        if(shift > params || shift < 0)
        {
            if(optionx_set(OPTION_SHIFT_VERBOSE))
            {
                fprintf(stderr, "%s: invalid shift number: %s\n", UTILITY, argv[1]);
            }
            return 2;
        }
        if(shift == 0)
        {
            return 0;
        }
    }

    int i, j = 1+shift;
    char buf[32];
    struct symtab_entry_s *entry1, *entry2;
    for(i = 1; i <= params; i++, j++)
    {
        sprintf(buf, "%d", i);
        entry1 = get_symtab_entry(buf);
        if(!entry1)
        {
            entry1 = add_to_symtab(buf);
        }
        sprintf(buf, "%d", j);
        entry2 = get_symtab_entry(buf);
        if(!entry2)
        {
            symtab_entry_setval(entry1, NULL);
        }
        else
        {
            symtab_entry_setval(entry1, entry2->val);
        }
    }
    params -= shift;
    sprintf(buf, "%d", params);
    symtab_entry_setval(hash, buf);
    return 0;
}
