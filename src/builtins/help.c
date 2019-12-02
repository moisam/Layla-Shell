/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: help.c
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
#include "../cmd.h"
#include "../debug.h"

/* current version of the shell */
char   shell_ver[] = "1.1-2";

/* useful macros for printing utilities help message */
#define SYNOPSIS        (1 << 0)    /* print usage summary (one liner) */
#define DESCRIPTION     (1 << 1)    /* print a short description */
#define HELP_BODY       (1 << 2)    /* print a longer help message */


/*
 * the help builtin utility (non-POSIX).. used to print useful help and how-to
 * messages for the shell's builtin utilities.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help help` or `help -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int help_builtin(int argc, char **argv)
{
    int v = 1, c, res = 0;
    int flags = HELP_BODY|SYNOPSIS|DESCRIPTION;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvsd", &v, 1)) > 0)
    {
        switch(c)
        {
            /* -h prints help */
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_HELP, 1, 0);
                return 0;
                
            /* -v prints shell ver */
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* -s prints only the synopsis */
            case 's':
                flags |=  SYNOPSIS   ;
                flags &= ~DESCRIPTION;
                flags &= ~HELP_BODY  ;
                break;
                
            /* -d prints only the description */
            case 'd':
                flags |=  DESCRIPTION;
                flags &= ~SYNOPSIS   ;
                flags &= ~HELP_BODY  ;
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* no arguments. print a generic help message */
    if(v >= argc)
    {
        printf("Layla shell v%s command line help.\n", shell_ver);
        printf("Type 'help command' or 'command -h' to view detailed help about a specific command.\n\n");
        printf("Available commands are:\n");
        int x, y, z;
        for(x = 0; x < special_builtin_count; x++)
        {
            /* print the utility's name */
            printf("  %s", special_builtins[x].name);
            /* pad it to 8-char margin */
            z = 8-strlen(special_builtins[x].name)%8;
            for(y = 0; y < z; y++)
            {
                printf(" ");
            }
            /* and print the short explanation */
            printf("%s\n", special_builtins[x].explanation);
        }
        for(x = 0; x < regular_builtin_count; x++)
        {
            /* print the utility's name */
            printf("  %s", regular_builtins[x].name);
            /* pad it to 8-char margin */
            z = 8-strlen(regular_builtins[x].name)%8;
            for(y = 0; y < z; y++)
            {
                printf(" ");
            }
            /* and print the short explanation */
            printf("%s\n", regular_builtins[x].explanation);
        }
        printf("\n");
        return 0;
    }

    /* process the arguments */
    for( ; v < argc; v++)
    {
        int i;
        char *arg = argv[v];
        /*
         * for each argument, we check if the argument is the name of a regular
         * builtin utility.. if so, we print the utility's help and move on to
         * the next argument.. otherwise, we check if the argument is a special
         * builtin utility and print the help if it is.. then we check the
         * special names : and . which refer to the 'colon' and 'dot' utilities,
         * respectively, and print the help if the argument is one of these..
         * lastly, if none of the above matched, we print and error message and
         * move on to the next argument.
         */
        /* check the regular builtins table */
        for(i = 0; i < regular_builtin_count; i++)
        {
            if(strcmp(arg, regular_builtins[i].name) == 0)
            {
                print_help(regular_builtins[i].name, i, 1, flags);
                break;
            }
        }
        /* check if the argument name was found in the regular builtins table */
        if(i < regular_builtin_count)
        {
            continue;
        }

        /* check the special builtins table */
        for(i = 0; i < special_builtin_count; i++)
        {
            if(strcmp(arg, special_builtins[i].name) == 0)
            {
                print_help(special_builtins[i].name, i, 0, flags);
                break;
            }
        }
        /* check if the argument name was found in the special builtins table */
        if(i < special_builtin_count)
        {
            continue;
        }
        /* check for special utility colon or : */
        if(strcmp(argv[v], "colon") == 0)
        {
            print_help(":", SPECIAL_BUILTIN_COLON, 0, flags);
        }
        /* check for special utility dot or . */
        if(strcmp(argv[v], "dot"  ) == 0)
        {
            print_help(".", SPECIAL_BUILTIN_DOT  , 0, flags);
        }
        /* utility not found. print an error message and set the error return result */
        else
        {
            fprintf(stderr, "%s: unknown builtin utility: %s\n", SHELL_NAME, arg);
            res = 1;
        }
    }
    return res;
}


/*
 * printing builtin utility help messages involves outputting the following:
 *
 * - a header line that looks like:
 *
 *    "alias utility for Layla shell, v1.0"
 *
 * - a synopsis line (summary of usage) that looks like:
 *
 *    "Usage: alias [-hv] [alias-name[=string] ...]"
 *     synposis can include the utility's invocation name more 
 *     than once.
 *
 * - a detailed explanation of the utility's options and arguments.
 */

void print_help(char *invokation_name, int index, int isreg, int flags)
{
    if(!flags)
    {
        flags = SYNOPSIS|DESCRIPTION|HELP_BODY;
    }

    /* get the utility info */
    struct builtin_s *utility;
    if(isreg)
    {
        utility = &regular_builtins[index];
    }
    else
    {
        utility = &special_builtins[index];
    }

    /* print the description */
    if(flag_set(flags, DESCRIPTION))
    {
        /* output the header */
        printf("%s utility for Layla shell, v%s\n", utility->name, shell_ver);
        /* then a line explaining what this utility does */
        printf("This utility is used to %s\n", utility->explanation);
    }

    /* print the synopsis */
    if(flag_set(flags, SYNOPSIS))
    {
        /* then the synopsis (usage) line */
        printf("\n");
        /*
         * call printf, passing it the correct number of arguments, depending on how
         * many times the utility's name appears in the synopsis line.
         */
        switch(utility->synopsis_name_count)
        {
            case 0:
                printf("%s", utility->synopsis);
                break;
            
            case 1:
                printf(utility->synopsis, invokation_name);
                break;
            
            case 2:
                printf(utility->synopsis, invokation_name, invokation_name);
                break;
            
            case 3:
                printf(utility->synopsis, invokation_name, invokation_name, 
                                          invokation_name);
                break;
            
            case 4:
                printf(utility->synopsis, invokation_name, invokation_name, 
                                          invokation_name, invokation_name);
                break;
            
            case 5:
                printf(utility->synopsis, invokation_name, invokation_name, 
                                          invokation_name, invokation_name,
                                          invokation_name);
                break;
            
            case 6:
                printf(utility->synopsis, invokation_name, invokation_name, 
                                          invokation_name, invokation_name,
                                          invokation_name, invokation_name);
                break;
            
            /* only test (or [[) has this many mentions of its name in the synopsis */
            case 9:
                printf(utility->synopsis, invokation_name, invokation_name, 
                                          invokation_name, invokation_name,
                                          invokation_name, invokation_name,
                                          invokation_name, invokation_name, 
                                          invokation_name);
                break;
        }
        printf("\n");
    }

    /* print the help message */
    if(flag_set(flags, HELP_BODY))
    {
        /* then the rest of utility's help */
        printf("\n%s", utility->help);
        /* print the standard -h and -v options */
        if(utility->flags & BUILTIN_PRINT_HOPTION)
        {
            printf("  -h        show utility help (this page)\n");
        }
        if(utility->flags & BUILTIN_PRINT_VOPTION)
        {
            printf("  -v        show shell version\n");
        }
    }
}
