/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: logout.c
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
#include "../cmd.h"
#include "../debug.h"


/*
 * the logout builtin utility (non-POSIX).. used to logout (or exit) from a login shell.
 *
 * shouldn't return unless we have pending jobs.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help logout` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int logout_builtin(int argc, char **argv)
{
    /*
     * NOTE: we perform proper logout in the exit_gracefully() function
     *       of exit.c.
     */
    return exit_builtin(argc, argv);
}
