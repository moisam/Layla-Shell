/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: whence.c
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
#include <stdlib.h>
#include <unistd.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../parser/parser.h"

#define UTILITY         "whence"


/*
 * The whence builtin utility (non-POSIX). It is a type/command-like ksh extension
 * with a slightly different set of options than both commands.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help whence` or `whence -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int whence_builtin(int argc, char **argv)
{
    int v = 1, c;
    int res = 0;
    int flags = TYPE_FLAG_PRINT_FUNCS | TYPE_FLAG_PRINT_BUILTINS;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "afhpv", &v, FLAG_ARGS_ERREXIT|FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &WHENCE_BUILTIN, 0);
                return 0;
                
            case 'a':
                flags |= TYPE_FLAG_PRINT_ALL;
                break;
                
            case 'f':
                flags &= ~TYPE_FLAG_PRINT_FUNCS;
                break;
                
            case 'p':
                flags |= TYPE_FLAG_PRINT_PATH;
                flags &= ~TYPE_FLAG_PRINT_BUILTINS;
                break;
                
            case 'v':
                /* ignored. we always print verbose output */
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* missing arguments */
    if(v >= argc)
    {
        MISSING_ARG_ERROR(UTILITY, "command name");
        return 2;
    }
    
    /* parse the arguments */
    for( ; v < argc; v++)
    {
        int res2 = print_command_type(argv[v], "whence", NULL, flags);
        if(res2)
        {
            res = res2;
        }
    }

    return res;
}
