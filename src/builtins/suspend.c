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
 * this utility is a tcsh/bash extension. we mimic its bash working here.
 */
int suspend(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    int force = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
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
    if(c == -1) return 2;
    
    if(force) raise(SIGSTOP);
    else
    {
        /* login shells can't be suspended without the -f option (bash) */
        if(option_set('L'))
        {
            fprintf(stderr, "%s: failed to suspend: login shell\r\n", UTILITY);
            return 2;
        }
        raise(SIGSTOP);
    }
    return 0;
}
