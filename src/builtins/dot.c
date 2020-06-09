/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: dot.c
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
#include <errno.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "setx.h"
#include "../debug.h"

/* defined in source.c */
int do_source_script(char *utility, char *file, int argc, char **argv);


/*
 * The dot builtin utility (POSIX). Used to execute dot scripts.
 *
 * Returns the exit status of the last command executed.
 * 
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help dot` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int dot_builtin(int argc, char **argv)
{
    /* first check we have exactly 2 args if we're in --posix mode */
    if(argc != 2 && option_set('P'))
    {
        PRINT_ERROR("%s: incorrect number of arguments\n"
                    "usage: %s file\n", argv[0], argv[0]);
        return 1;
    }

    /* then check we have enough args for the ksh-like usage */
    if(argc < 2)
    {
        PRINT_ERROR("%s: incorrect number of arguments\n"
                    "usage: %s file [args...]\n", argv[0], argv[0]);
        return 1;
    }
    
    return do_source_script(argv[0], argv[1], argc-2, &argv[2]);
}
