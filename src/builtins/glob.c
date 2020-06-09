/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: glob.c
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

#include <wchar.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "builtins.h"
#include "../cmd.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY             "glob"

/* defined in echo.c */
int process_echo_options(int argc, char **argv, char *opts, int *allow_escaped, int *supp_nl);


/*
 * The glob builtin utility (non-POSIX). Prints its arguments list in a way similar
 * to what echo does, except that glob null-terminates each argument.
 *
 * Returns 0.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help glob` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int glob_builtin(int argc, char **argv)
{
    /*
     * In bash, shopt option 'xpg_echo' is used to indicate whether escape
     * sequences are enabled by echo by default. This behavior can be overriden
     * by use of the -e and -E options (see below).
     */
    int allow_escaped = optionx_set(OPTION_XPG_ECHO);
    int supp_nl;    /* not used here (used in echo) */

    /* process the options */
    int v = process_echo_options(argc, argv, "eE", &allow_escaped, &supp_nl);

    /* print the arguments */
    int flags = allow_escaped ? FLAG_ECHO_ALLOW_ESCAPED : 0;
    flags |= FLAG_ECHO_NULL_TERM;

    do_echo(v, argc, argv, flags);

    return 0;
}
