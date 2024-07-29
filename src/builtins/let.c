/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../include/debug.h"

#define UTILITY             "let"


/*
 * The let builtin utility (non-POSIX). Used to evaluate arithmetic arguments.
 *
 * let does arithmetic evaluation of its arguments in most shells (bash, ksh, ...).
 * We need to check the result of the last argument. If non-zero, return
 * zero. otherwise, return 1.
 */

int let_builtin(int argc, char **argv)
{
    int i = 1, j = 0;
    char *res;
    
    /* bash's let recognizes (and skips) leading '--' */
    if(argv[1] && strcmp(argv[1], "--") == 0)
    {
        i++;
    }
    
    /* no arguments */
    if(i >= argc)
    {
        PRINT_ERROR(UTILITY, "missing expression");
        return 1;
    }
    
    /* parse the arguments */
    for( ; i < argc; i++)
    {
        res = arithm_expand(argv[i]);
        
        /* arithm_expand() should have printed an appropriate error msg */
        if(!res)
        {
            return 1;
        }
        free(res);

        /*
         * arithm_expand() inverts the result when it sets the exit status.
         * we will just pass the result back to our caller as-is.
         */
        j = exit_status;
    }
    
    /* return 1 if the last expr evaluated to 0, return 0 otherwise */
    return j;
}
