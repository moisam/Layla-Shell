/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: suspend.c
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

#include <signal.h>
#include "../cmd.h"

#define UTILITY             "suspend"


/*
 * the suspend builtin utility (non-POSIX).. used to suspend the shell.
 *
 * this utility is a tcsh/bash extension. we mimic its bash working here.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help suspend` or `suspend -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int suspend(int argc, char **argv)
{
    int v = 1, c;
    int force = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvf", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_SUSPEND, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'f':
                force = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /* if the force -f flag is passed, raise the signal even if we are a login shell */
    if(force)
    {
        raise(SIGSTOP);
    }
    else
    {
        /* login shells can't be suspended without the -f option (bash) */
        if(option_set('L'))
        {
            fprintf(stderr, "%s: failed to suspend: login shell\n", UTILITY);
            return 2;
        }
        raise(SIGSTOP);
    }
    return 0;
}
