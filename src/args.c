/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: args.c
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
#include <string.h>
#include <errno.h>
#include "include/cmd.h"
#include "symtab/symtab.h"
#include "include/debug.h"

/* Original argv passed to the shell on startup */
char **shell_argv;

/* Original argc passed to the shell on startup */
int    shell_argc;


/*
 * Parse arguments passed to the builtin utilities.
 * 
 * Parameters are:
 *   __argc     : argc for the utility
 *   __argv     : argv for the utility
 *   __ops      : options string that the utility accepts as legal options
 *   __argi     : pointer to an integer in which we will save the index of
 *                  the next argument after the options finish, or the index 
 *                  of current argument in case the caller expects an arg-option
 *  invalid_opt : one character representing the invalid option found while
 *                  processing the options string (this is only valid when
 *                  the function returns -1)
 *  flags       : see the FLAG_ARGS_* macro definitions in cmd.h
 * 
 * Return values:
 *   -1         : in case of illegal option
 *   alpha      : the letter representing the selected option
 *   0          : options finished. index of the first operand stored in *__argi
 */
char  *internal_optarg = 0;
char   internal_opterr = 0;
int    internal_argi   = 0;
int    internal_argsub = 0;


int parse_args(int __argc, char **__argv, char *__ops, int *__argi, int flags)
{
    char *p, *p2;
    char minus_plus = '-';
    
    if(!__argv)
    {
        return -1;
    }

    if(internal_argi < 1 || internal_argi > __argc)
    {
        (*__argi) = __argc;
        return 0;
    }
    
    if(*__ops == ':')
    {
        __ops++;
    }
    
    p = __argv[internal_argi];
    if(!p || !*p)
    {
        (*__argi) = internal_argi;
        return 0;
    }
    
    if(*p != '-')
    {
        /* Only accept '+' options when option string begins with '+' */
        if(*p != '+' || *__ops != '+')
        {
            (*__argi) = internal_argi;
            return 0;
        }
        minus_plus = '+';
    }
        
    if(strcmp(p, "--") == 0)
    {
        (*__argi) = internal_argi+1;
        return 0;
    }
    
    if(internal_argsub == 0)
    {
        internal_argsub++;
    }
    
    p += internal_argsub;
    internal_optarg = NULL;
    
    if((p2 = strchr(__ops, *p)) == NULL)
    {
        internal_opterr = *p;

        if(flag_set(flags, FLAG_ARGS_PRINTERR))
        {
            PRINT_ERROR(__argv[0], "unknown option: %c%c", minus_plus, *p);
        }
        
        if(p[1] == '\0')
        {
            /* Last option in arg, move on to the next arg */
            internal_argi++;
            internal_argsub = 0;
        }
        else
        {
            /* Move on to the next option */
            internal_argsub++;
        }
        
        (*__argi) = internal_argi;
        
        if(!flag_set(flags, FLAG_ARGS_ERREXIT))
        {
            return -1;
        }

        /* POSIX says non-interactive shell should exit on Utility syntax errors */
        if(!interactive_shell)
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }

        return -1;
    }

    char c = *p;
    if(p2[1] == ':')
    {
        /* Argument-taking option */
        internal_argi++;
        internal_argsub = 0;
        
        /* Take the next argument as the option argument */
        if(internal_argi >= __argc)
        {
            internal_optarg = INVALID_OPTARG;
            internal_opterr = c;
        }
        /* Take the rest of this option string as the argument */
        else if(p[1])
        {
            internal_optarg = p+1;
        }
        /* Check the next argument is not another option */
        else if((p = __argv[internal_argi]) &&
                ((*p == '-' && p[1] != '\0') ||
                 (*p == '+' && minus_plus == '+')))
        {
            internal_optarg = INVALID_OPTARG;
            internal_opterr = *p;
        }
        else
        {
            internal_optarg = __argv[internal_argi];
            
            /* assign argi so that we can move on to the next argument */
            (*__argi) = internal_argi++;
            return (int)c;
        }
    }
    else if(p[1] == '\0')
    {
        /* Last option in arg, move on to the next arg */
        internal_argi++;
        internal_argsub = 0;
    }
    else
    {
        /* Move on to the next option */
        internal_argsub++;
    }

    (*__argi) = internal_argi;
    return (int)c;

}
