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

char   shell_ver[] = "1.0";

#define SYNOPSIS        (1 << 0)
#define DESCRIPTION     (1 << 1)
#define HELP_BODY       (1 << 2)


int help(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c, res = 0;
    int flags = HELP_BODY|SYNOPSIS|DESCRIPTION;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvsd", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_HELP, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 's':
                flags |=  SYNOPSIS   ;
                flags &= ~DESCRIPTION;
                flags &= ~HELP_BODY  ;
                break;
                
            case 'd':
                flags |=  DESCRIPTION;
                flags &= ~SYNOPSIS   ;
                flags &= ~HELP_BODY  ;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        printf("Layla shell v%s command line help.\r\n", shell_ver);
        printf("Type 'help command' or 'command -h' to view detailed help about a specific command.\r\n\n");
        printf("Available commands are:\r\n");
        uint8_t x;
        for(x = 0; x < special_builtin_count; x++)
        {
            printf("  %s", special_builtins[x].name);
            uint8_t y;
            for(y = 0; y < (8-strlen(special_builtins[x].name)%8); y++)
                printf(" ");
            printf("%s\r\n", special_builtins[x].explanation);
        }
        for(x = 0; x < regular_builtin_count; x++)
        {
            printf("  %s", regular_builtins[x].name);
            uint8_t y;
            for(y = 0; y < (8-strlen(regular_builtins[x].name)%8); y++)
                printf(" ");
            printf("%s\r\n", regular_builtins[x].explanation);
        }
        printf("\r\n");
        return 0;
    }

    for( ; v < argc; v++)
    {
        int i;
        char *arg = argv[v];
        for(i = 0; i < regular_builtin_count; i++)
        {
            if(strcmp(arg, regular_builtins[i].name) == 0)
            {
                print_help(regular_builtins[i].name, i, 1, flags);
                break;
            }
        }
        if(i < regular_builtin_count) continue;

        for(i = 0; i < special_builtin_count; i++)
        {
            if(strcmp(arg, special_builtins[i].name) == 0)
            {
                print_help(special_builtins[i].name, i, 0, flags);
                break;
            }
        }
        if(i < special_builtin_count) continue;

        if(strcmp(argv[v], "colon") == 0)
        {
            print_help(":", SPECIAL_BUILTIN_COLON, 0, flags);
        }
        if(strcmp(argv[v], "dot"  ) == 0)
        {
            print_help(".", SPECIAL_BUILTIN_DOT  , 0, flags);
        }
        else
        {
            fprintf(stderr, "%s: unknown builtin utility: %s\r\n", SHELL_NAME, arg);
            res = 1;
        }
    }
    return res;
}

/*
 * printing utility help involves outputting the following:
 * - header line that looks like:
 *    "alias utility for Layla shell, v1.0"
 * - synopsis line (summary of usage) that looks like:
 *    "Usage: alias [-hv] [alias-name[=string] ...]"
 *     synposis can include the utility's invocation name more 
 *     than once.
 * - detailed explanation of the options and arguments.
 */
void print_help(char *invokation_name, int index, int isreg, int flags)
{
    if(!flags) flags = SYNOPSIS|DESCRIPTION|HELP_BODY;

    struct builtin_s *utility;
    if(isreg) utility = &regular_builtins[index];
    else      utility = &special_builtins[index];

    if(flag_set(flags, DESCRIPTION))
    {
        /* output the header */
        printf("%s utility for Layla shell, v%s\r\n", utility->name, shell_ver);
        /* then a line explaining what this utility does */
        printf("This utility is used to %s\r\n", utility->explanation);
    }

    if(flag_set(flags, SYNOPSIS))
    {
        /* then the synopsis (usage) line */
        printf("\r\n");
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
            
            /* only test (or [[) has this many */
            case 9:
                printf(utility->synopsis, invokation_name, invokation_name, 
                                          invokation_name, invokation_name,
                                          invokation_name, invokation_name,
                                          invokation_name, invokation_name, 
                                          invokation_name);
                break;
        }
        printf("\r\n");
    }

    if(flag_set(flags, HELP_BODY))
    {
        /* then the rest of utility's help */
        printf("\n%s", utility->help);
        /* print the standard -h and -v options */
        if(utility->flags & BUILTIN_PRINT_HOPTION)
            printf("  -h        show utility help (this page)\r\n");
        if(utility->flags & BUILTIN_PRINT_VOPTION)
            printf("  -v        show shell version\r\n");
    }
}
