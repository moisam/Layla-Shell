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


/*
 * print all the readonly variables.
 */
void purge_readonlys()
{
    int i;
    /* use an alpha list to sort variables alphabetically */
    struct alpha_list_s list;
    init_alpha_list(&list);
    struct symtab_stack_s *stack = get_symtab_stack();

    for(i = 0; i < stack->symtab_count; i++)
    {
        struct symtab_s *symtab = stack->symtab_list[i];
        /*
         * for all but the local symbol table, we check the table lower down in the
         * stack to see if there is a local variable defined with the same name as
         * a global variable.. if that is the case, the local variable takes precedence
         * over the global variable, and we skip printing the global variable as we
         * will print the local variable when we reach the local symbol table.
         */

#ifdef USE_HASH_TABLES

        if(symtab->used)
        {
            struct symtab_entry_s **h1 = symtab->items;
            struct symtab_entry_s **h2 = symtab->items + symtab->size;
            for( ; h1 < h2; h1++)
            {
                struct symtab_entry_s *entry = *h1;

#else

                struct symtab_entry_s *entry  = symtab->first;

#endif

                while(entry)
                {
                    if(flag_set(entry->flags, FLAG_READONLY))
                    {
                        /* check the lower symbol tables don't have the same variable defined */
                        struct symtab_entry_s *entry2 = get_symtab_entry(entry->name);
                        /* check we got an entry that is different from this one */
                        if(entry2 != entry)
                        {
                            /* we have a local variable with the same name. skip this one */
                            entry = entry->next;
                            continue;
                        }
                        /* get the formatted name=val string */
                        char *str = NULL;
                        if(entry->val == NULL)
                        {
                            str = alpha_list_make_str("readonly %s", entry->name);
                        }
                        else
                        {
                            char *val = quote_val(entry->val, 1);
                            if(val)
                            {
                                str = alpha_list_make_str("readonly %s=%s", entry->name, val);
                                free(val);
                            }
                            else
                            {
                                str = alpha_list_make_str("readonly %s=", entry->name);
                            }
                        }
                        add_to_alpha_list(&list, str);
                    }
                    entry = entry->next;
                }

#ifdef USE_HASH_TABLES

            }
        }

#endif

    }

    print_alpha_list(&list);
    free_alpha_list(&list);
}


/*
 * the readonly builtin utility (POSIX).. used to set the readonly attribute to
 * one or more variables.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help readonly` or `readonly -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int readonly(int argc, char *argv[])
{
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "p" : "hvp";
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, 1)) > 0)
    {
        /* parse the option */
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
    if(c == -1)
    {
        return 2;
    }

    /* no arguments. print all the readonly variables */
    if(v >= argc)
    {
        purge_readonlys();
        return 0;
    }
    
    /* set the selected variables as readonly */
    int res = 0;
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        /* check if arg contains an equal sign */
        char *equals = strchr(arg, '=');
        /* if yes, get the value part */
        char *val    = equals ? equals+1 : NULL;
        /*
         *  get the variable name.. if there is an equal sign, the name is
         *  the part before the equal sign, otherwise it is the whole string.
         */
        size_t name_len = equals ? (size_t)(equals-arg) : strlen(arg);
        char name_buf[name_len+1];
        strncpy(name_buf, arg, name_len);
        name_buf[name_len] = '\0';

        /* positional and special parameters can't be set like this */
        if(is_pos_param(name_buf) || is_special_param(name_buf))
        {
            res = 1;
            fprintf(stderr, "readonly: error setting/unsetting '%s' is not allowed\n", name_buf);
            continue;
        }

        /* check for an entry anywhere in the symbol table stack */
        struct symtab_entry_s *entry = get_symtab_entry(name_buf);
        if(!entry)
        {
            /* no entry. add new one to the local symbol table */
            entry = add_to_symtab(name_buf);
        }

        if(entry)
        {
            /* set the value, if any */
            if(val)
            {
                /* make sure we're not setting an already readonly variable */
                if(flag_set(entry->flags, FLAG_READONLY))
                {
                    fprintf(stderr," readonly: cannot set %s: readonly variable\n", name_buf);
                    res = 1;
                }
                else
                {
                    symtab_entry_setval(entry, val);
                }
            }

            /* set the flags */
            entry->flags |= FLAG_READONLY;
        }
    }
    return res;
}
