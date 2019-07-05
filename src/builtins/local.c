/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: local.c
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
#include "../symtab/symtab.h"

#define UTILITY             "local"

int local(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_LOCAL, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        return 0;
    }
  
    /*
     * if we saved the passed variables straight away, they will go into our
     * local symbol table, which will eventually get popped off the stack when
     * we go back to do_simple_command() in backend.c, which is useless. what
     * we want is to add the variables to our caller's symbol table, e.g. a script
     * or function that wants to declare local vars. this is why we pop off our
     * local symtab, add vars to our caller's symtab, then push back our (empty)
     * symtab, which will be popped off when we return. this is similar to what we
     * do in declare.c.
     */
    struct symtab_s *symtab = symtab_stack_pop();
    if(get_local_symtab()->level == 0)
    {
        fprintf(stderr, "%s: cannot declare local variables at the global scope\r\n", UTILITY);
        symtab_stack_push(symtab);
        return 2;
    }

    int res = 0;
    for( ; v < argc; v++)
    {
        if(do_declare_var(argv[v], 0, FLAG_LOCAL, 0, SPECIAL_BUILTIN_LOCAL) != 0)
            res = 1;
    }  
    symtab_stack_push(symtab);
    return res;
}
