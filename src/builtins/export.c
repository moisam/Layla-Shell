/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: export.c
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

/* macro definitions needed to use setenv() */
#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../parser/node.h"
#include "../debug.h"

#define UTILITY             "export"


/*
 * print all variables and functions whose flags field contains the given
 * attribute 'attr'.. if the entry refers to a variable, 'var_prefix' is
 * printed before the entry, otherwise 'func_prefix' is used.
 */
void print_var_attribs(unsigned int attr, char *var_perfix, char *func_prefix)
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
                    if(flag_set(entry->flags, attr))
                    {
                        struct symtab_entry_s *entry2 = get_symtab_entry(entry->name);
                        /* check we got an entry that is different from this one */
                        if(entry2 != entry)
                        {
                            /* we have a local variable with the same name. skip this one */
                            entry = entry->next;
                            continue;
                        }

                        char *prefix = (entry->val_type == SYM_FUNC) ? func_prefix : var_perfix;
                        char *str = NULL;
                        if(entry->val == NULL)
                        {
                            str = alpha_list_make_str("%s %s", prefix, entry->name);
                        }
                        else
                        {
                            char *val = quote_val(entry->val, 1, 0);
                            if(val)
                            {
                                str = alpha_list_make_str("%s %s=%s", prefix, entry->name, val);
                                free(val);
                            }
                            else
                            {
                                str = alpha_list_make_str("%s %s=", prefix, entry->name);
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
 * process the list of arguments passed to export or readonly, setting the
 * values and attributes of variables/functions as appropriate.
 * if 'unexport' is set, we remove args from our tables (this happens when
 * export is invoked with the -n option).. if 'funcs' is set, args refer
 * to functions, otherwise they refer to variables.. 'flag' contains the
 * flag we'll set/remove from args, which is either the export or the 
 * readonly flag.
 * 
 * returns 0 if all variables/functions are set/removed successfully, 
 * otherwise returns 1.
 */
int process_var_attribs(char **args, int unexport, int funcs, int flag)
{
    int res = 0;
    struct symtab_s *globsymtab = get_global_symtab();
    char **p;
    char *utility = (flag == FLAG_EXPORT) ? "export" : "readonly";

    for(p = args; *p; p++)
    {
        /* check if arg contains an equal sign */
        char *arg = *p;
        char *equals  = strchr(arg, '=');
        
        /*
         *  get the variable/function name.. if there is an equal sign, the
         *  name is the part before the equal sign, otherwise it is the whole
         *  string.
         */
        size_t name_len = equals ? (size_t)(equals-arg) : strlen(arg);
        char name_buf[name_len+1];
        strncpy(name_buf, arg, name_len);
        name_buf[name_len] = '\0';

        int append = 0;
        if(name_buf[name_len-1] == '+')
        {
            name_buf[name_len-1] = '\0';
            append = SET_FLAG_APPEND;
            name_len--;
        }
        
        if(unexport)
        {
            /* -n can only appear with the export builtin */
            if(flag != FLAG_EXPORT)
            {
                continue;
            }
            
            /* remove the export flag if -n option was supplied */
            struct symtab_entry_s *entry = NULL;
            if(funcs)
            {
                /* search for name in the functions table */
                entry = get_func(name_buf);
            }
            else
            {
                /* search for name in the global symbol table */
                entry = do_lookup(name_buf, globsymtab);
            }
            
            /* unexport the entry */
            if(entry)
            {
                entry->flags &= ~flag;
            }
        }
        else
        {
            /* the -n option is not supplied */
            /* search for name in the functions table */
            if(funcs)
            {
                /* can't define functions this way */
                if(equals)
                {
                    PRINT_ERROR("%s: cannot use the '-f' option to define functions\n",
                                utility);
                    res = 2;
                }
                else
                {
                    struct symtab_entry_s *entry = get_func(name_buf);
                    if(!entry)
                    {
                        /* can't export an undefined function */
                        PRINT_ERROR("%s: unknown function name: %s\n",
                                    utility, name_buf);
                        res = 2;
                    }
                    else
                    {
                        /* export the function */
                        entry->flags |= flag;
                    }
                }
            }
            /* name is a variable */
            else
            {
                /* get the value part */
                char *val = equals ? equals+1 : NULL;
                struct symtab_entry_s *entry = do_set(name_buf, val, flag, 0, append);
                if(!entry)
                {
                    res = 1;
                }
            }
        }
    }
    return res;
}


/*
 * the export builtin utility (POSIX).. exports variables and functions so that they
 * are accessible from the environment of invoked commands and subshells.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help export` or `export -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int export_builtin(int argc, char **argv)
{
    int v = 1, c, unexport = 0;
    int print = 0, funcs = 0;      /* if set, work on the functions table */

    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "p" : "hfvpn";
    
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, 1)) > 0)
    {
        /* parse the option */
        switch(c)
        {
            case 'h':
                print_help(argv[0], &EXPORT_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* -p : print the list of exported functions or variables and return */
            case 'p':
                print = 1;
                break;
                
            /* -n: unexport a function or variable */
            case 'n':
                unexport = 1;
                break;
                
            /* -f: treat arguments as function (not variable) names */
            case 'f':
                funcs = 1;
                break;
        }
    }
    
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* no arguments or the -p option: print all the readonly variables */
    if(print || v >= argc)
    {
        if(funcs)
        {
            print_func_attribs(FLAG_EXPORT);
        }
        else
        {
            print_var_attribs(FLAG_EXPORT, "export", "declare -x -f");
        }
        return 0;
    }
    
    /* process the argument list */
    return process_var_attribs(&argv[v], unexport, funcs, FLAG_EXPORT);
}


/*
 * export the contents of the given symbol table to the enviroment of a newly
 * forked process.. we export only the variables and functions that have the
 * export flag on, or those which are declared locally (if the command is run
 * from inside a function), or variables declared as part of the command prefix.
 *
 * the force_export_all flag, if non-zero, forces export of all variables and
 * functions, even if they are not marked for export.. this flag is set when we
 * fork a subshell, so that the new process has a replica of all variables and
 * functions, local and global.
 */
void do_export_table(struct symtab_s *symtab, int force_export_all)
{
    /* sanity check */
    if(!symtab)
    {
        return;
    }
    
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
                /* force export everything */
                if(force_export_all                        ||
                   /* export global variables */
                   flag_set(entry->flags, FLAG_EXPORT    ) ||
                   /* export vars declared on the commandline before/after the command name */
                   flag_set(entry->flags, FLAG_CMD_EXPORT) ||
                   /*
                    * export locally defined vars, for example a variable declared local
                    * in a function should be exported to the commands and subshells called
                    * within that function.
                    */
                   flag_set(entry->flags, FLAG_LOCAL     ))
                {
                    if(!entry->val)
                    {
                        /* entry is an exported function */
                        if(entry->val_type == SYM_FUNC)
                        {
                            char *f = cmd_nodetree_to_str(entry->func_body);
                            if(f)
                            {
                                char *s = malloc(strlen(f)+10);
                                if(!s)
                                {
                                    setenv(entry->name, "", 1);
                                }
                                else
                                {
                                    sprintf(s, "() {\n%s\n}", f);
                                    setenv(entry->name, s, 1);
                                    //exit_gracefully(0, 0);
                                    free(s);
                                }
                                free(f);
                            }
                        }
                    }
                    else
                    {
                        /* entry is an exported variable */
                        setenv(entry->name, entry->val, 1);
                    }
                }
                /* check the next entry */
                entry = entry->next;
            }
                
#ifdef USE_HASH_TABLES

        }
    }
        
#endif

}


/*
 * this function is called after a new command process is forked, right before
 * it exec's, to save all export variables to the environment of the new
 * command.. in this case, the 'force_export_all' flag will be zero.
 *
 * the function is also called when we fork a subshell, in which case
 * 'force_export_all' will be non-zero.
 */
void do_export_vars(int force_export_all)
{
    /*
     * we will start by reading vars from the global symbol table,
     * then work our way up to the local symbol table.. this ensures
     * that locally defined variables overwrite globally defined
     * variables of the same name.
     */
    int i = 0;
    struct symtab_stack_s *stack = get_symtab_stack();
    for( ; i < stack->symtab_count; i++)
    {
        struct symtab_s *symtab = stack->symtab_list[i];
        do_export_table(symtab, force_export_all);
    }
    /*
     * now export the functions.
     */
    do_export_table(func_table, force_export_all);
}
