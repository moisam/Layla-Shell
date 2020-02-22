/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include <fnmatch.h>
#include "builtins.h"
#include "../cmd.h"
#include "../debug.h"

/* current version of the shell */
char   shell_ver[] = "1.1-3";

/* useful macros for printing utilities help message */
#define SYNOPSIS        (1 << 0)    /* print usage summary (one liner) */
#define DESCRIPTION     (1 << 1)    /* print a short description */
#define HELP_BODY       (1 << 2)    /* print a longer help message */
#define MANPAGE_LIKE    (1 << 3)    /* print a manpage-like message */

void print_synopsis(struct builtin_s *utility, char *invokation_name, char *indent);
void print_help_body(struct builtin_s *utility, int indent);


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
    int flags = HELP_BODY | SYNOPSIS | DESCRIPTION;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "dhmsv", &v, 1)) > 0)
    {
        switch(c)
        {
            /* -d prints only the description */
            case 'd':
                flags = DESCRIPTION;
                break;
                
            /* -h prints help */
            case 'h':
                print_help(argv[0], &HELP_BUILTIN, 0);
                return 0;
                
            /* -m prints everything */
            case 'm':
                flags = MANPAGE_LIKE;
                break;

            /* -s prints only the synopsis */
            case 's':
                flags = SYNOPSIS;
                break;
                
            /* -v prints shell ver */
            case 'v':
                printf("%s", shell_ver);
                return 0;
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

        struct builtin_s *u = shell_builtins;
        for( ; u->name; u++)
        {
            /* print the utility's name and short explanation */
            printf("  %-10s %s\n", u->name, u->explanation);
        }
        printf("\n");

        return 0;
    }

    /* process the arguments */
    for( ; v < argc; v++)
    {
        /* add '*' to the end of the name to make it a regex pattern */
        size_t len = strlen(argv[v]);
        char arg[len+2];
        sprintf(arg, "%s%s", argv[v], argv[v][len-1] == '*' ? "" : "*");
        
        /*
         * for each argument, we check if the argument is the name of a builtin 
         * utility.. if so, we print the utility's help and move on to the next 
         * argument.. then we check the special names : and . which refer to the 
         * 'colon' and 'dot' utilities, respectively, and print the help if the 
         * argument is one of these.. lastly, if none of the above matched, we 
         * print and error message and move on to the next argument.
         */
        struct builtin_s *utility = shell_builtins;
        for( ; utility->name; utility++)
        {
            if(fnmatch(arg, utility->name, 0) == 0)
            {
                break;
            }
        }
        
        if(utility->name)
        {
            print_help(utility->name, utility, flags);
        }
        /* check for special utility colon or : */
        else if(fnmatch(arg, "colon", 0) == 0)
        {
            print_help(":", &COLON_BUILTIN, flags);
        }
        /* check for special utility dot or . */
        else if(fnmatch(arg, "dot", 0) == 0)
        {
            print_help(".", &DOT_BUILTIN, flags);
        }
        /* utility not found. print an error message and set the error return result */
        else
        {
            PRINT_ERROR("%s: unknown builtin utility: %s\n", SHELL_NAME, argv[v]);
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

void print_help(char *invokation_name, struct builtin_s *utility, int flags)
{
    /* if using the -m option, print a manpage-like help page */
    if(flags == MANPAGE_LIKE)
    {
        printf("NAME\n    %s - %s\n\n", utility->name, utility->explanation);
        printf("SYNOPSIS\n    ");
        print_synopsis(utility, invokation_name, "    ");
        printf("\n\nDESCRIPTION\n");
        printf("    This utility is used to %s\n\n    ", utility->explanation);
        print_help_body(utility, 1);
        printf("\nSEE ALSO\n    info lsh, man lsh(1)\n\n");
        printf("AUTHOR\n    Mohammed Isam <mohammed_isam1984@yahoo.com>\n\n");
        return;
    }
    
    /* not using -m, print our regular help */
    if(!flags)
    {
        flags = SYNOPSIS | DESCRIPTION | HELP_BODY;
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
        printf("\nUsage: ");
        print_synopsis(utility, invokation_name, "       ");
        printf("\n");
    }

    /* print the help message */
    if(flag_set(flags, HELP_BODY))
    {
        print_help_body(utility, 0);
    }
}


/*
 * print the given utility's synopsis by calling printf, passing it the 
 * correct number of arguments, depending on how many times the utility's 
 * name appears in the synopsis line.
 */
void print_synopsis(struct builtin_s *utility, char *invokation_name, char *indent)
{
    char *p = utility->synopsis;
    while(*p)
    {
        if(*p == '%' && p[1] == '%')
        {
            printf("%s", invokation_name);
            p++;
        }
        else
        {
            putchar(*p);
            if(*p == '\n')
            {
                printf("%s", indent);
            }
        }
        p++;
    }
}


/*
 * print the given utility's help body, preceding each line with 4 spaces
 * if indent is non-zero.
 */
void print_help_body(struct builtin_s *utility, int indent)
{
    static char *hopt = "  -h        show utility help (this page)";
    static char *vopt = "  -v        show shell version";

    if(indent)
    {
        /* print the utility's help */
        char *p = utility->help;
        while(*p)
        {
            putchar(*p);
            if(*p == '\n' && p[1])
            {
                printf("    ");
            }
            p++;
        }
    }
    else
    {
        /* print the utility's help */
        printf("\n%s", utility->help);
    }

    /* print the standard -h and -v options */
    if(utility->flags & BUILTIN_PRINT_HOPTION)
    {
        printf("%s%s\n", indent ? "    " : "", hopt);
    }

    if(utility->flags & BUILTIN_PRINT_VOPTION)
    {
        printf("%s%s\n", indent ? "    " : "", vopt);
    }
}
