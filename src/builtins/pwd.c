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
#include "../cmd.h"
#include "../symtab/symtab.h"

#define L_OPTION    1
#define P_OPTION    2

#define UTILITY         "pwd"


int pwd(int argc, char **argv)
{
    struct symtab_entry_s *entry = get_symtab_entry("PWD");
    char *PWD = get_shell_varp("PWD", NULL);
    if(!PWD || !*PWD)
    {
        fprintf(stderr, "%s: cannot read PWD variable: NULL value\r\n", UTILITY);
        return 2;
    }
    int option = L_OPTION;
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvLP", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_PWD, 1, 0);
                break;
                
            case 'v':
                printf("%s", shell_ver);
                break;
                
            case 'L':
                option = L_OPTION;
                break;
                
            case 'P':
                option = P_OPTION;
                break;                
        }
    }
    /* unknown option */
    if(c == -1) return 1;
    
    /* do POSIX-style on PWD */
    if(option == P_OPTION) goto do_p_option;
    if(*PWD == '/')
    {
        char *p = PWD;
        while(*p++)
        {
            /* we have "/." */
            if(*p == '.' && p[-1] == '/')
            {
                /* we have "/./" or "/." */
                if(p[1] == '/' || p[1] == '\0') goto do_p_option;
                /* we have "/../" or "/.." */
                if(p[1] == '.' && (p[2] == '/' || p[2] == '\0')) goto do_p_option;
            }
        }
        set_terminal_color(COL_WHITE, COL_DEFAULT);
        printf("%s\r\n", PWD);
        return 0;
    }
do_p_option:
    /*
     * TODO: we should properly process POSIX's P-option here
     */
    set_terminal_color(COL_WHITE, COL_DEFAULT);
    PWD = getcwd(NULL, 0);
    printf("%s\r\n", PWD);
    symtab_entry_setval(entry, PWD);
    return 0;
}
