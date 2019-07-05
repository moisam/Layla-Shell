/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../parser/node.h"
#include "../debug.h"

#define UTILITY             "export"


int is_capital(char c)
{
  if(c >= 'A' && c <= 'Z') return 1;
  return 0;
}

int is_legitimate(char c)
{
  if(c >= 'A' && c <= 'Z') return 1;
  if(c >= 'a' && c <= 'z') return 1;
  if(c == '_') return 1;
  return 0;
}

int is_shell_var(char *var)
{
    struct symtab_entry_s *entry = get_symtab_entry(var);
    if(entry) return 1;
    return -1;
}

/*
 * find the proper quoting character for the given string value.
 * then print the quoted string to stdout.
 * used by export, readonly and trap utilities.
 */
void purge_quoted_val(char *val)
{
    if(!val)
    {
        printf("\"\"");
        return;
    }
    char *v = val;
    putchar('"');
    while(*v)
    {
        switch(*v)
        {
            case '\\':
            case  '`':
            case  '$':
            case  '"':
                putchar('\\');

            default:
                putchar(*v++);
                break;
        }
    }
    putchar('"');
}

void purge_quoted(char *prefix, char *name, char *val)
{
    if(!val) printf("%s %s\r\n", prefix, name);
    else
    {
        printf("%s %s=", prefix, name);
        purge_quoted_val(val);
        printf("\r\n");
    }
}


void purge_exports(struct symtab_s *symtab)
{    
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
                purge_quoted((entry->val_type == SYM_FUNC) ? "declare -x -f" : "export", entry->name, entry->val);
                entry = entry->next;
            }
    
#ifdef USE_HASH_TABLES
    
        }
    }
        
#endif
}


int export(int argc, char *argv[])
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c, unexport = 0;
    int funcs = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hfvpn", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_EXPORT, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                if(funcs) purge_exports(func_table);
                else      purge_exports(get_global_symtab());
                return 0;
                
            case 'n':
                unexport = 1;
                break;
                
            case 'f':
                funcs = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        if(funcs) purge_exports(func_table);
        else      purge_exports(get_global_symtab());
        return 0;
    }
    
    int res = 0;
    int flags = unexport ? 0 : FLAG_EXPORT;
    struct symtab_s *globsymtab = get_global_symtab();
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        char *equals  = strchr(arg, '=');
        //char *val     = equals ? equals+1 : NULL;
        int  name_len = equals ? (equals-arg) : strlen(arg);
        char name_buf[name_len+1];
        strncpy(name_buf, arg, name_len);
        name_buf[name_len] = '\0';
        if(unexport)
        {
            /* remove the export flag if -n option was supplied */
            struct symtab_entry_s *entry = NULL;
            if(funcs) entry = get_func(name_buf);
            else      entry = __do_lookup(name_buf, globsymtab);
            if(entry) entry->flags &= ~FLAG_EXPORT;
        }
        else
        {
            if(funcs)
            {
                if(equals)
                {
                    fprintf(stderr, "%s: cannot use the '-f' option to define functions\n", UTILITY);
                    res = 2;
                }
                else
                {
                    struct symtab_entry_s *entry = get_func(name_buf);
                    if(!entry)
                    {
                        fprintf(stderr, "%s: unknown function name: %s\n", UTILITY, name_buf);
                        res = 2;
                    }
                    else entry->flags |= FLAG_EXPORT;
                }
            }
            else if(do_declare_var(arg, 1, flags, 0, SPECIAL_BUILTIN_EXPORT) != 0) res = 1;
        }
    }
    return res;
}

void do_export_table(struct symtab_s *symtab)
{
    if(!symtab) return;
    
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
                /* export global variables */
                if(flag_set(entry->flags, FLAG_EXPORT    ) ||
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
                        if(entry->val_type == SYM_FUNC)
                        {
                            char *f = cmd_nodetree_to_str(entry->func_body);
                            if(f)
                            {
                                char *s = malloc(strlen(f)+10);
                                if(!s) setenv(entry->name, "", 1);
                                else
                                {
                                    sprintf(s, "() {\r\n%s\r\n}", f);
                                    setenv(entry->name, s, 1);
                                    //exit_gracefully(0, 0);
                                    free(s);
                                }
                                free(f);
                            }
                        }
                        else setenv(entry->name, "", 1);
                    }
                    else setenv(entry->name, entry->val, 1);
                }
                entry = entry->next;
            }
                
#ifdef USE_HASH_TABLES

        }
    }
        
#endif

}

/*
 * this function is called after a new command process is forked,
 * right before it exec's, to save all export variables to the
 * environment of the new command.
 */
void do_export_vars()
{
    /*
     * we will start by reading vars from the global symbol table,
     * then work our way up to the local symbol table. this ensures
     * that locally defined variables overwrite globally defined
     * variables of the same name.
     */
    int i;
    struct symtab_stack_s *stack = get_symtab_stack();
    for(i = 0; i < stack->symtab_count; i++)
    {
        struct symtab_s *symtab = stack->symtab_list[i];
        do_export_table(symtab);
    }
    /*
     * now export the functions.
     */
    do_export_table(func_table);
}
