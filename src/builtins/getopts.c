/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: getopts.c
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
#include <sys/types.h>
#include "builtins.h"
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../parser/parser.h"
#include "../include/debug.h"

#define UTILITY         "getopts"

/* useful macros for printing utilities help message (from help.c) */
#define SYNOPSIS        (1 << 0)    /* print usage summary (one liner) */


#define INIT_VAR(var, name)                                 \
do                                                          \
{                                                           \
    var = get_symtab_entry(name);                           \
    if(!var)                                                \
    {                                                       \
        var = add_to_symtab(name);                          \
    }                                                       \
    else if(flag_set(var->flags, FLAG_READONLY))            \
    {                                                       \
        READONLY_ASSIGN_ERROR(invoking_prog,                \
                              name, "variable");            \
        return 1;                                           \
    }                                                       \
} while(0)


/*
 * The getopts builtin utility (POSIX). Used to process command line arguments.
 *
 * Returns 0 on success, 2 if the options list finished, non-zero in case of error.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help getopts` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int getopts_builtin(int argc, char **argv)
{
    /* we need at least 3 args */
    if(argc < 3)
    {
        MISSING_ARGS_ERROR(UTILITY);
        print_help(argv[0], &GETOPTS_BUILTIN, SYNOPSIS);
        return 2;
    }
    
    /* we don't accept any options yet, but parse options and return error */
    int i = 1;
    for( ; i < argc; i++)
    {
        char *arg = argv[i];
        if(*arg != '-')
        {
            break;
        }
        
        OPTION_UNKNOWN_ERROR(UTILITY, arg[1] ? arg[1] : '-');
        return 2;
    }
    
    char  *optstring     = argv[i++];
    char  *name          = argv[i];
    char **args          = &argv[i];
    int    argsc         = argc-i;
    int    free_args     = 0;
    char  *invoking_prog = get_symtab_entry("0")->val;
    struct symtab_entry_s *OPTIND, *OPTSUB, *NAME;
    char   buf[12];

    /* sanity check for the given variable name */
    if(!is_name(name))
    {
        PRINT_ERROR(UTILITY, "invalid name: %s", name);
        return 2;
    }
    
    /* get the value of $OPTERR */
    int opterr = (strcmp(get_shell_varp("OPTERR", "1"), "0") == 0) ? 0 : 1;
    
    /*
     * use the 'silent' mode if we have ':' as the first char, or after 
     * a leading '+', or if $OPTERR is set to zero (bash).
     */
    int silent = (opterr == 0) || (*optstring == ':' ||
                     ((*optstring == '+' || *optstring == '-') && optstring[1] == ':'));

    /* get the current value of $OPTIND and initialize it to 1 if its unset */
    INIT_VAR(OPTIND, "OPTIND");
    INIT_VAR(OPTSUB, "OPTSUB");
    INIT_VAR(NAME  , name    );
    internal_argi = get_shell_vari("OPTIND", 1);
    internal_argsub = get_shell_vari("OPTSUB", 0);

    /* no args? use positional params instead */
    if(argsc == 1)
    {
        int count = get_shell_vari("#", 0);
        /* we don't have any positional parameters. bail out */
        if(count <= 0)
        {
            symtab_entry_setval(OPTIND, "1");
            symtab_entry_setval(OPTSUB, "0");
            symtab_entry_setval(NAME  , "?");
            return 2;
        }
        
        /*
         *  Get a copy of all positional parameters in addition to special param $0.
         *  Effectively, we will be working on an array similar to a C program's argv.
         */
        args = malloc((count+2) * sizeof(char *));
        if(!args)
        {
            INSUFFICIENT_MEMORY_ERROR(UTILITY, "load args");
            return 1;
        }
        
        free_args = 1;
        memset(args, 0, (count+2) * sizeof(char *));
        args[0] = invoking_prog;
        
        if(count)
        {
            int i = 1;
            for( ; i <= count; i++)
            {
                sprintf(buf, "%d", i);
                args[i] = get_shell_varp(buf, "");
            }
            
            /* NULL-terminate the array */
            args[i] = NULL;
        }
        argsc = count+1;
    }
    
    /****************************
     * process the options
     ****************************/
    int v = 1, c;
    int res = 0;
    if((c = parse_args(argsc, args, optstring, &v, 0)) > 0)
    {
        /*
         *  Set the index variable's value to the most recent option.
         *  If the option string starts with +, we precede the option with +.
         */
        char cstr[3];
        if(*optstring == '+')
        {
            sprintf(cstr, "+%c", c);
        }
        else
        {
            sprintf(cstr,  "%c", c);
        }

        symtab_entry_setval(NAME, cstr);
        
        /* set $OPTIND to the index of this argument */
        sprintf(buf, "%d", v);
        symtab_entry_setval(OPTIND, buf);
        sprintf(buf, "%d", internal_argsub);
        symtab_entry_setval(OPTSUB, buf);
        
        /* check if we have an option argument */
        if(internal_optarg)
        {
            /* invalid arg. set $OPTARG and the index variable appropriately */
            if(internal_optarg == INVALID_OPTARG)
            {
                /* we have ':' as the first char, or after a leading '+' */
                if(silent)
                {
                    char estr[] = { internal_opterr, '\0' };
                    symtab_entry_setval(NAME  , ":" );
                    res = do_set("OPTARG", estr, 0, 0, 0) ? 0 : 1;
                }
                else
                {
                    symtab_entry_setval(NAME  , "?" );
                    res = do_set("OPTARG", NULL, 0, 0, 0) ? 0 : 1;
                    
                    /* print an error message */
                    if(opterr)
                    {
                        OPTION_REQUIRES_ARG_ERROR(invoking_prog, internal_opterr);
                    }
                }
            }
            /* valid argument. save it in $OPTARG */
            else
            {
                res = do_set("OPTARG", internal_optarg, 0, 0, 0) ? 0 : 1;
            }
        }
    }
    else
    {
        /* end of options or unknown option */
        sprintf(buf, "%d", v);
        symtab_entry_setval(OPTIND, buf);
        sprintf(buf, "%d", internal_argsub);
        symtab_entry_setval(OPTSUB, buf);
        symtab_entry_setval(NAME  , "?");
        
        /* unknown option */
        if(c == -1)
        {
            /* set $OPTARG appropriately, and print an error msg if needed */
            if(silent)
            {
                char estr[] = { internal_opterr, '\0' };
                res = do_set("OPTARG", estr, 0, 0, 0) ? 0 : 1;
            }
            else
            {
                /* print the error-causing option */
                OPTION_UNKNOWN_ERROR(invoking_prog, internal_opterr);
                res = do_set("OPTARG", NULL, 0, 0, 0) ? 0 : 1;
            }
        }
        else
        {
            res = 2;
        }
    }
    
    /* free temp memory */
    if(free_args)
    {
        free(args);
    }

    return res;
}
