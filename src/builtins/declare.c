/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: declare.c
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
#include <errno.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../parser/node.h"
#include "../debug.h"

#define UTILITY         "declare"

/* flags for purge_table() */
#define FLAG_FUNC       (1 << 0)
#define FLAG_FUNC_NAME  (1 << 1)
#define FLAG_GLOBAL     (1 << 2)

void   purge_table(struct symtab_s *symtab, int flags);
void   purge_var(struct symtab_entry_s *entry, int name_only);


int declare(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c, res = 0;
    int print_attribs = 0;
    int func_only     = 0;
    int funcname_only = 0;
    int global        = 0;
    int flags         = 0;      /* flags for printing values */
    int set_flags     = 0;      /* flags for setting values */
    int unset_flags   = 0;      /* flags for setting values */
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "+hvpfFgrxlut", &v, 1)) > 0)
    {
        char plus = (argv[v][0] == '+') ? 1 : 0;
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_DECLARE, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                print_attribs = !plus;
                break;
                
            case 'f':               /* restrict output to function names */
                print_attribs = !plus;
                func_only     = !plus;
                break;
                
            case 'F':               /* don't print function definitions */
                print_attribs = !plus;
                funcname_only = !plus;
                break;
                
            case 'g':               /* declare on global level (even inside functions) */
                global        = !plus;
                break;
                
            case 'x':               /* mark as export */
                if(plus) unset_flags |= FLAG_EXPORT;
                else     set_flags   |= FLAG_EXPORT;
                break;
                
            case 'r':               /* mark as readonly */
                if(plus)
                {
                    fprintf(stderr, "%s: cannot use the '+r' option to remove the readonly attribute\n", UTILITY);
                    return 2;
                }
                set_flags |= FLAG_READONLY;
                break;
                
            case 'l':
                if(plus)
                {
                    unset_flags |=  FLAG_ALLSMALL;
                }
                else
                {
                    set_flags   |=  FLAG_ALLSMALL;
                    set_flags   &= ~FLAG_ALLCAPS ;
                    unset_flags |=  FLAG_ALLCAPS ;
                    unset_flags &= ~FLAG_ALLSMALL;
                }
                break;
                
            case 'u':
                if(plus)
                {
                    unset_flags |=  FLAG_ALLCAPS ;
                }
                else
                {
                    set_flags   |=  FLAG_ALLCAPS ;
                    set_flags   &= ~FLAG_ALLSMALL;
                    unset_flags |=  FLAG_ALLSMALL;
                    unset_flags &= ~FLAG_ALLCAPS ;
                }
                break;
                
            case 't':
                if(plus) unset_flags |= FLAG_FUNCTRACE;
                else     set_flags   |= FLAG_FUNCTRACE;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;
    
    if(func_only    ) flags |= FLAG_FUNC;
    if(funcname_only) flags |= FLAG_FUNC_NAME;
    if(!global      ) set_flags |= FLAG_LOCAL;
    

    if(v >= argc)
    {
        purge_vars(flags);
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
     * do in local.c.
     */
    struct symtab_s *symtab = symtab_stack_pop();
    int funcs = (func_only || funcname_only);
    
    for( ; v < argc; v++)
    {
        struct symtab_entry_s *entry = funcs ? get_func(argv[v]) : get_symtab_entry(argv[v]);
        if(print_attribs)
        {
            if(!entry)
            {
                char *word = funcs ? "function" : "variable";
                fprintf(stderr, "%s: unknown %s name: %s\n", UTILITY, word, argv[v]);
                res = 1;
                continue;
            }
            if(funcs)
            {
                purge_var(entry, funcname_only);
            }
            else
            {
                purge_var(entry, 0);
            }
        }
        else
        {
            /* cannot define functions using declare -f (bash) */
            if(funcs)
            {
                fprintf(stderr, "%s: cannot use the '-f' option to define functions\n", UTILITY);
                res = 1;
            }
            else if(do_declare_var(argv[v], global, set_flags, unset_flags, REGULAR_BUILTIN_DECLARE) != 0)
                res = 1;
        }
    }
    symtab_stack_push(symtab);    
    return res;
}


int do_declare_var(char *arg, int set_global, int set_flags, int unset_flags, int utility)
{
    char *equals  = strchr(arg, '=');
    char *val     = equals ? equals+1 : NULL;
    int  name_len = equals ? (equals-arg) : strlen(arg);
    char name_buf[name_len+1];
    int res = 0;
    strncpy(name_buf, arg, name_len);
    name_buf[name_len] = '\0';
    
    char *who = "export";
    switch(utility)
    {
        case SPECIAL_BUILTIN_READONLY:
            who = "readonly";
            break;
                
        case SPECIAL_BUILTIN_EXPORT:
            who = "export";
            break;
            
        case SPECIAL_BUILTIN_LOCAL:
            who = "local";
            break;
    }

    if(is_pos_param(name_buf) || is_special_param(name_buf))
    {
        res = 1;
        fprintf(stderr, "%s: error setting/unsetting '%s' is not allowed\r\n", who, name_buf);
    }
    else if((res = __set(name_buf, val, set_global, set_flags, unset_flags)) <= 0)
    {
        char *keyword = "declaring";
        switch(utility)
        {
            case SPECIAL_BUILTIN_READONLY:
                keyword = "setting readonly";
                break;
                
            case SPECIAL_BUILTIN_EXPORT:
                keyword = "exporting";
                break;
                
            case SPECIAL_BUILTIN_LOCAL:
                //keyword = "declaring";
                break;
        }
        fprintf(stderr, "%s: error %s '%s'", who, keyword, name_buf);
        if(res == -1)
             fprintf(stderr, ": readonly variable");
        fprintf(stderr, "\r\n");
        res = 1;
    }
    else res = 0;
    return res;
}


void purge_vars(int flags)
{
    if(flag_set(flags, FLAG_FUNC) || flag_set(flags, FLAG_FUNC_NAME))
    {
        purge_table(func_table, flags);
    }
    else
    {
        int i;
        struct symtab_stack_s *stack = get_symtab_stack();
        for(i = 0; i < stack->symtab_count; i++)
        {
            struct symtab_s *symtab = stack->symtab_list[i];
            purge_table(symtab, flags);
        }
    }
}


void purge_table(struct symtab_s *symtab, int flags)
{
    if(!symtab) return;
    
    int func_only     = flag_set(flags, FLAG_FUNC     );
    int funcname_only = flag_set(flags, FLAG_FUNC_NAME);
    
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
                if(func_only || funcname_only)
                {
                    if(entry->val_type == SYM_FUNC)
                    {
                        purge_var(entry, funcname_only);
                    }
                }
                else
                {
                    purge_var(entry, 0);
                }
                entry = entry->next;
            }
                
#ifdef USE_HASH_TABLES

        }
    }
        
#endif

}


void purge_var(struct symtab_entry_s *entry, int name_only)
{
    if(entry->val_type == SYM_FUNC && !name_only)
    {
        if(entry->val)
        {
            if(entry->val[0] == '(' && entry->val[1] == ')')
            {
                printf("%s %s\r\n", entry->name, entry->val);
            }
            else
            {
                printf("%s () { %s }\r\n", entry->name, entry->val);
            }
        }
        else
        {
            printf("%s () { ", entry->name);
            char *f = cmd_nodetree_to_str(entry->func_body);
            if(f)
            {
                //purge_quoted_val(f);
                printf("%s", f);
                free(f);
            }
            printf(" }\r\n");
        }
    }
    printf("declare ");
    if(flag_set(entry->flags, FLAG_EXPORT))
    {
        printf("-x ");
    }
    if(flag_set(entry->flags, FLAG_READONLY))
    {
        printf("-r ");
    }
    if(entry->val_type == SYM_FUNC) printf("-f ");
    if(entry->name[0] == '-') printf("-- ");
    printf("%s", entry->name);
    if(entry->val_type != SYM_FUNC)
    //if(!name_only)
    {
        putchar('=');
        /*
        if(entry->val_type == SYM_FUNC)
        {
            char *f = cmd_nodetree_to_str(entry->func_body);
            if(f)
            {
                purge_quoted_val(f);
                free(f);
            }
            else
            {
                printf("\"\"");
            }
        }
        else */ purge_quoted_val(entry->val);
    }
    putchar('\n');
}
