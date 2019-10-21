/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY         "getopts"


/*
 * the getopts builtin utility (POSIX).. used to process command line arguments.
 *
 * returns 0 on success, 2 if the options list finished, non-zero in case of error.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help getopts` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int getopts(int argc, char **argv)
{
    /* we need at least 3 args */
    if(argc < 3)
    {
        fprintf(stderr, "%s: missing argument(s)\n", UTILITY);
        print_help(argv[0], REGULAR_BUILTIN_GETOPTS, 1, 0);
        return 1;
    }
    char  *optstring     = argv[1];
    char  *name          = argv[2];
    char **args          = &argv[2];
    int    argsc         = argc-2;
    int    free_args     = 0;
    char  *invoking_prog = NULL;
    char   buf[12];

    /* get the current value of $OPTIND and initialize it to 1 if its unset */
    argi = 0;
    struct symtab_entry_s *OPTIND = get_symtab_entry("OPTIND");
    if(!OPTIND)
    {
        OPTIND = add_to_symtab("OPTIND");
        symtab_entry_setval(OPTIND, "1");
    }
    /* get the value of $OPTIND */
    if(OPTIND->val)
    {
        argi = atoi(OPTIND->val);
        if(argi <= 0)
        {
            argi = 0;
        }
    }
    /* get the value of $OPTERR */
    int opterr = (strcmp(get_shell_varp("OPTERR", "0"), "1") == 0) ? 1 : 0;
    /* store the name of our index variable, initialize $OPTARG, and get argv[0] value */
    struct symtab_entry_s *NAME   = add_to_symtab(name    );
    struct symtab_entry_s *OPTARG = add_to_symtab("OPTARG");
    struct symtab_entry_s *param0 = get_symtab_entry("0");
    invoking_prog = param0->val;

    /* no args? use positional params instead */
    if(argsc == 1)
    {
        int count = get_shell_vari("#", 0);
        /* we don't have any positional parameters. bail out */
        if(count <= 0)
        {
            symtab_entry_setval(OPTIND, "1");
            symtab_entry_setval(NAME  , "?");
            return 2;
        }
        /*
         *  get a copy of all positional parameters in addition to special param $0.
         *  effectively, we will be working on an array similar to a C program's argv.
         */
        args = malloc((count+2) * sizeof(char *));
        if(!args)
        {
            fprintf(stderr, "%s: insufficient memory to load args\n", UTILITY);
            return 5;
        }
        free_args = 1;
        memset(args, 0, (count+2) * sizeof(char *));
        args[0] = param0->val;
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
    while((c = parse_args(argsc, args, optstring, &v, 0)) > 0)
    {
        /*
         *  set the index variable's value to the most recent option.
         *  if the option string starts with +, we precede the option with +.
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
        symtab_entry_setval(NAME  , cstr);
        /* set $OPTIND to the index of this argument */
        sprintf(buf, "%d", v);
        symtab_entry_setval(OPTIND, buf );
        /* check if we have an option argument */
        if(__optarg)
        {
            /* invalid arg. set $OPTARG and the index variable appropriately */
            if(__optarg == INVALID_OPTARG)
            {
                /* we have ':' as the first char, or after a leading '+' */
                if(*optstring == ':' || optstring[1] == ':')
                {
                    char estr[] = { __opterr, '\0' };
                    symtab_entry_setval(OPTARG, estr);
                    symtab_entry_setval(NAME  , ":" );
                }
                else
                {
                    symtab_entry_setval(OPTARG, NULL);
                    symtab_entry_setval(NAME  , "?" );
                    /* print an error message */
                    if(opterr)
                    {
                        fprintf(stderr, "%s: option requires an argument -- %c\n", invoking_prog, __opterr);
                    }
                }
            }
            /* valid argument. save it in $OPTARG */
            else
            {
                symtab_entry_setval(OPTARG, __optarg);
            }
        }
        /* free temp memory */
        if(free_args)
        {
            free(args);
        }
        return 0;
    }
    /* unknown option */
    if(c == -1)
    {
        /* set the index variable to "?" */
        symtab_entry_setval(NAME, "?");
        /* set $OPTARG appropriately, and print an error msg if needed */
        if(*optstring == ':')
        {
            char estr[] = { __opterr, '\0' };
            symtab_entry_setval(OPTARG, estr);
        }
        else
        {
            /* print the error-causing option */
            if(opterr)
            {
                fprintf(stderr, "%s: unknown option -- %c\n", invoking_prog, __opterr);
            }
            symtab_entry_setval(OPTARG, NULL);
        }
        /* free temp memory */
        if(free_args)
        {
            free(args);
        }
        return 0;
    }
    
    /********************/
    /* end of arguments */
    /********************/
    sprintf(buf, "%d", v);
    symtab_entry_setval(OPTIND, buf);
    symtab_entry_setval(NAME  , "?");
    /* free temp memory */
    if(free_args)
    {
        free(args);
    }
    return 2;
}
