/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: readonly.c
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
#include "builtins.h"
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../include/debug.h"

#define UTILITY     "readonly"


/*
 * The readonly builtin utility (POSIX). Used to set the readonly attribute to
 * one or more variables.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help readonly` or `readonly -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int readonly_builtin(int argc, char **argv)
{
    int v = 1, c;
    int print = 0, funcs = 0;      /* if set, work on the functions table */

    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "p" : "hfvp";

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        /* parse the option */
        switch(c)
        {
            case 'h':
                print_help(argv[0], &READONLY_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                print = 1;
                break;
                
            /* -f: treat arguments as function (not variable) names */
            case 'f':
                funcs = 1;
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* no arguments or the -p option: print all the readonly variables */
    if(print || v >= argc)
    {
        if(funcs)
        {
            print_func_attribs(FLAG_READONLY);
        }
        else
        {
            print_var_attribs(FLAG_READONLY, "readonly", "readonly");
        }
        return 0;
    }
    
    /* set the selected variables as readonly */
    return process_var_attribs(&argv[v], 0, funcs, FLAG_READONLY);
}
