/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: readonly.c
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY     "readonly"


#ifdef USE_HASH_TABLES

    void purge_readonlys()
    {
        int i;
        struct symtab_stack_s *stack = get_symtab_stack();
        for(i = 0; i < stack->symtab_count; i++)
        {
            struct symtab_s *symtab = stack->symtab_list[i];
            if(symtab->used)
            {
                struct symtab_entry_s **h1 = symtab->items;
                struct symtab_entry_s **h2 = symtab->items + symtab->size;
                for( ; h1 < h2; h1++)
                {
                    struct symtab_entry_s *entry = *h1;
                    while(entry)
                    {
                        if(entry->flags & FLAG_READONLY)
                            purge_quoted("readonly", entry->name, entry->val);
                        entry = entry->next;
                    }
                }
            }
        }
    }

#else

    void purge_readonlys()
    {
        int i;
        struct symtab_stack_s *stack = get_symtab_stack();
        for(i = 0; i < stack->symtab_count; i++)
        {
            struct symtab_s       *symtab = stack->symtab_list[i];
            struct symtab_entry_s *entry  = symtab->first;
            while(entry)
            {
                if(entry->flags & FLAG_READONLY)
                    purge_quoted("readonly", entry->name, entry->val);
                entry = entry->next;
            }
        }
    }

#endif


int readonly(int argc, char *argv[])
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvp", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_READONLY, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                purge_readonlys();
                return 0;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        purge_readonlys();
        return 0;
    }
    
    int res = 0;
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        if(do_declare_var(arg, 0, FLAG_READONLY, 0, SPECIAL_BUILTIN_READONLY) != 0) res = 1;
        /*
        char *equals  = strchr(arg, '=');
        char *val     = equals ? equals+1 : NULL;
        int  name_len = equals ? (equals-arg) : strlen(arg);
        char name_buf[name_len+1];
        strncpy(name_buf, arg, name_len);
        name_buf[name_len] = '\0';
        if(is_pos_param(name_buf) || is_special_param(name_buf))
        {
            res = 1;
            fprintf(stderr, "%s: error setting/unsetting '%s' is not allowed\r\n", UTILITY, arg);
        }
        else if(!__set(name_buf, val, 0, FLAG_READONLY))
        {
            res = 1;
            fprintf(stderr, "%s: error setting readonly variable '%s'\r\n", UTILITY, arg);
        }
        */
    }
    return res;
}
