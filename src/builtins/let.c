/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: let.c
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
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY             "let"


/*
 * the let builtin utility (non-POSIX).. used to evaluate arithmetic arguments.
 *
 * let does arithmetic evaluation of its arguments in most shells (bash, ksh, ...).
 * we need to check the result of the last argument. if non-zero, return
 * zero. otherwise, return 1.
 */

int let_builtin(int argc, char **argv)
{
    /* no arguments */
    if(argc == 1)
    {
        return 1;
    }
    char *endstr;    
    char *res[argc-1];
    int i = 1, j = 0;
    /* parse the arguments */
    for( ; i < argc; i++, j++)
    {
        res[j] = arithm_expand(argv[i]);
        /* __do_arithmetic() should have printed an appropriate error msg */
        if(!res[j])
        {
            while(j--)
            {
                free(res[j]);
            }
            return 1;
        }
    }
    i = strtol(res[j-1], &endstr, 10);
    if(endstr == res[j-1])
    {
        i = 1;
    }
    while(j--)
    {
        free(res[j]);
    }
    return !i;
}
