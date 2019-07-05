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


int getopts(int argc, char **argv)
{
    if(argc < 3)
    {
        fprintf(stderr, "%s: missing argument(s)\r\n", UTILITY);
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

    struct symtab_entry_s *OPTIND = get_symtab_entry("OPTIND");
    if(!OPTIND)
    {
        OPTIND = add_to_symtab("OPTIND");
        symtab_entry_setval(OPTIND, "1");
    }
    int opterr = (strcmp(get_shell_varp("OPTERR", "0"), "1") == 0) ? 1 : 0;

    struct symtab_entry_s *NAME   = add_to_symtab(name    );
    struct symtab_entry_s *OPTARG = add_to_symtab("OPTARG");
    struct symtab_entry_s *param0 = get_symtab_entry("0");
    invoking_prog = param0->val;

    /* no args? use positional params */
    if(argsc == 1)
    {
        struct symtab_entry_s *entry = get_symtab_entry("#");
        int count = atoi(entry->val);
        args = (char **)malloc((count+2) * sizeof(char *));
        if(!args)
        {
            fprintf(stderr, "%s: insufficient memory to load args\r\n", UTILITY);
            return 5;
        }
        free_args = 1;
        memset((void *)args, 0, (count+2) * sizeof(char *));
        args[0] = param0->val;
        if(count)
        {
            int i;
            for(i = 1; i <= count; i++)
            {
                sprintf(buf, "%d", i);
                args[i] = get_shell_varp(buf, "");
            }
            args[i] = NULL;
        }
        argsc = count+1;
    }
    
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    if(OPTIND->val) argi = atoi(OPTIND->val);
    else            argi = 0;
    while((c = parse_args(argsc, args, optstring, &v, 0)) > 0)
    {
        char cstr[3];
        if(*optstring == '+') sprintf(cstr, "+%c", c);
        else                  sprintf(cstr,  "%c", c);
        symtab_entry_setval(NAME  , cstr);
        sprintf(buf, "%d", v);
        symtab_entry_setval(OPTIND, buf );
        if(__optarg)
        {
            if(__optarg == INVALID_OPTARG)
            {
                if(*optstring == ':' || optstring[1] == ':')    /* ':' first or after leading '+' */
                {
                    char estr[] = { __opterr, '\0' };
                    symtab_entry_setval(OPTARG, estr);
                    symtab_entry_setval(NAME  , ":" );
                }
                else
                {
                    symtab_entry_setval(OPTARG, NULL);
                    symtab_entry_setval(NAME  , "?" );
                    if(opterr)
                        fprintf(stderr, "%s: option requires an argument -- %c\r\n", invoking_prog, __opterr);
                }
            }
            else
            {
                symtab_entry_setval(OPTARG, __optarg);
            }
        }
        if(free_args) free(args);
        return 0;
    }
    /* unknown option */
    if(c == -1)
    {
        symtab_entry_setval(NAME, "?");
        if(*optstring == ':')
        {
            char estr[] = { __opterr, '\0' };
            symtab_entry_setval(OPTARG, estr);
        }
        else
        {
            if(opterr)
                fprintf(stderr, "%s: unknown option -- %c\r\n", invoking_prog, __opterr);
            symtab_entry_setval(OPTARG, NULL);
        }
        if(free_args) free(args);
        return 0;
    }
    /****************************/
    
    /* end of arguments */
    sprintf(buf, "%d", v);
    symtab_entry_setval(OPTIND, buf);
    symtab_entry_setval(NAME  , "?");
    if(free_args) free(args);
    return 2;
}
