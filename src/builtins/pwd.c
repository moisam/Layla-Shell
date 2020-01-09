/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: pwd.c
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
#include <string.h>
#include <errno.h>
#include "../cmd.h"
#include "../symtab/symtab.h"

// #define L_OPTION    1   /* handle symbolic links logically */
// #define P_OPTION    2   /* handle symbolic links physically */
#define UTILITY         "pwd"


/*
 * the pwd builtin utility (POSIX).. prints the current working directory,
 * as reflected in the $PWD shell variable.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help pwd` or `pwd -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int pwd_builtin(int argc, char **argv)
{
    /* use the -L option by default */
    int l_option = 1;
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvLP", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_PWD, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'L':
                l_option = 1;
                break;
                
            case 'P':
                l_option = 0;
                break;                
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    
    /* go POSIX-style on PWD */
    if(l_option)
    {
        char *wd = getenv("PWD");
        if(wd && *wd == '/')
        {
            char *p = wd;
            int has_dot = 0;
            
            while((p = strstr(p, "/.")))
            {
                /* we have "/./" or "/." */
                if(p[2] == '/' || p[2] == '\0')
                {
                    has_dot = 1;
                    break;
                }
                
                /* we have "/../" or "/.." */
                if(p[2] == '.' && (p[3] == '/' || p[3] == '\0'))
                {
                    has_dot = 1;
                    break;
                }

                /* not dot or dot-dot. skip it */
                p += 2;
            }
            
            if(!has_dot)
            {
                set_terminal_color(COL_WHITE, COL_DEFAULT);
                printf("%s\n", wd);
                return 0;
            }
        }
    }

    if(cwd)
    {
        free(cwd);
    }
    
    cwd = getcwd(NULL, 0);
    if(!cwd)
    {
        fprintf(stderr, "%s: failed to read current working directory: %s\n", UTILITY, strerror(errno));
        return 1;
    }
    
    set_terminal_color(COL_WHITE, COL_DEFAULT);
    printf("%s\n", cwd);
    
    /* POSIX says we shouldn't update $PWD with the -P option */
    return 0;
}
