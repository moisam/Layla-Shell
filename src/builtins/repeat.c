/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: repeat.c
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY         "repeat"


/*
 * the repeat builtin utility (non-POSIX).. used to execute commands a number
 * of times.
 *
 * the repeat utility is a tcsh non-POSIX extension. bash doesn't have it.
 *
 * returns the exit status of the last command executed.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help repeat` or `repeat -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int repeat(int argc, char **argv)
{
    int v = 1, c;
    int count = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_REPEAT, 0, 0);
                return 0;
                
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
    
    /* missing arguments */
    if(v >= argc)
    {
        fprintf(stderr, "%s: missing argument: count\n", UTILITY);
        return 2;
    }

    /* get the repeat count */
    char *strend = NULL;
    count = strtol(argv[v], &strend, 10);
    if(strend == argv[v] || count < 0)
    {
        fprintf(stderr, "%s: missing argument: invalid count: %s\n", UTILITY, argv[v]);
        return 2;
    }

    /* we should have at least one command to execute */
    if(++v >= argc)
    {
        fprintf(stderr, "%s: missing argument: command name\n", UTILITY);
        return 2;
    }
    
    int    cargc = argc-v;
    char **cargv = &argv[v];
    v = 0;
    /* push a local symbol table on top of the stack */
    symtab_stack_push();
    /* execute the command(s) */
    while(count-- > 0)
    {
        v = search_and_exec(cargc, cargv, NULL, SEARCH_AND_EXEC_DOFORK|SEARCH_AND_EXEC_DOFUNC);
    }
    /* free the local symbol table */
    free_symtab(symtab_stack_pop());
    return v;
}
