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


/*
 * the declare builtin utility (non-POSIX).. used to define variables and print them.
 * returns 0 if the variables were defined or printed, non-zero otherwise.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help declare` or `declare -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int declare_builtin(int argc, char **argv)
{
    int v = 1, c, res = 0;
    int print_attribs = 0;
    int func_only     = 0;
    int funcname_only = 0;
    int global        = 0;
    int flags         = 0;      /* flags for printing values */
    int set_flags     = 0;      /* flags to be set */
    int unset_flags   = 0;      /* flags to be unset */
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "+hvpfFgrxlut", &v, 1)) > 0)
    {
        /* check if the options string starts with '+', which 'turns off' options */
        char plus = (argv[v][0] == '+') ? 1 : 0;
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_DECLARE, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':               /* print variables and their attributes */
                print_attribs = !plus;
                break;
                
            case 'f':               /* restrict output to function names and definitions */
                print_attribs = !plus;
                func_only     = !plus;
                break;
                
            case 'F':               /* don't print function definitions */
                print_attribs = !plus;
                funcname_only = !plus;
                break;
                
            case 'g':               /* declare on the global level (even inside functions) */
                global        = !plus;
                break;
                
            case 'x':               /* mark as export variables/functions */
                if(plus) unset_flags |= FLAG_EXPORT;
                else     set_flags   |= FLAG_EXPORT;
                break;
                
            case 'r':               /* mark as readonly variables */
                if(plus)
                {
                    fprintf(stderr, "%s: cannot use the '+r' option to remove the readonly attribute\n", UTILITY);
                    return 2;
                }
                set_flags |= FLAG_READONLY;
                break;
                
            case 'l':           /* convert variable's value to lowercase on assignment */
                if(plus)        /* turn off the lowercase flag */
                {
                    unset_flags |=  FLAG_ALLSMALL;
                }
                else            /* turn on the lowercase flag */
                {
                    set_flags   |=  FLAG_ALLSMALL;
                    set_flags   &= ~FLAG_ALLCAPS ;
                    unset_flags |=  FLAG_ALLCAPS ;
                    unset_flags &= ~FLAG_ALLSMALL;
                }
                break;
                
            case 'u':           /* convert variable's value to uppercase on assignment */
                if(plus)        /* turn off the uppercase flag */
                {
                    unset_flags |=  FLAG_ALLCAPS ;
                }
                else            /* turn on the uppercase flag */
                {
                    set_flags   |=  FLAG_ALLCAPS ;
                    set_flags   &= ~FLAG_ALLSMALL;
                    unset_flags |=  FLAG_ALLSMALL;
                    unset_flags &= ~FLAG_ALLCAPS ;
                }
                break;
                
            case 't':
                if(plus)        /* turn function tracing off for the given function */
                {
                    unset_flags |= FLAG_FUNCTRACE;
                }
                else            /* turn function tracing on */
                {
                    set_flags   |= FLAG_FUNCTRACE;
                }
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /* set the flags field according to the passed options */
    if(func_only)
    {
        flags |= FLAG_FUNC;
    }
    if(funcname_only)
    {
        flags |= FLAG_FUNC_NAME;
    }
    if(!global)
    {
        set_flags |= FLAG_LOCAL;
    }

    /* no arguments. print all variables */
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
    //struct symtab_s *symtab = symtab_stack_pop();
    int funcs = (func_only || funcname_only);

    /* loop on the arguments */
    for( ; v < argc; v++)
    {
        /* fetch the variable/function from the symbol table */
        struct symtab_entry_s *entry = funcs ? get_func(argv[v]) : get_symtab_entry(argv[v]);
        if(print_attribs)
        {
            /* variable/function not found */
            if(!entry)
            {
                char *word = funcs ? "function" : "variable";
                fprintf(stderr, "%s: unknown %s name: %s\n", UTILITY, word, argv[v]);
                res = 1;
                continue;
            }
            /* print the variable (or function) value and attributes */
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
            {
                res = 1;
            }
        }
    }
    /* push the local symbol table back on the stack */
    //symtab_stack_push(symtab);
    return res;
}


/*
 * declare a variable, setting and unsetting its attributes as requested.. positional parameters
 * ($1, $2, $3, ...) and special parameters ($0, $$, $?) cannot be set this way.
 * return value:
 *   1   if the variable is a positional paremte, a special parameter, or if any other error
 *       occurred during setting the variable's attributes (e.g. setting a readonly variable)
 *   0   if the variable is declared and it's attributes are set successfully
 */
int do_declare_var(char *arg, int set_global, int set_flags, int unset_flags, int utility)
{
    /* is this a "name=value" string? */
    char *equals  = strchr(arg, '=');
    /* if yes, get the value part */
    char *val     = equals ? equals+1 : NULL;
    /* and the name part */
    size_t name_len = equals ? (size_t)(equals-arg) : strlen(arg);
    char name_buf[name_len+1];
    int res = 0;
    strncpy(name_buf, arg, name_len);
    name_buf[name_len] = '\0';
    
    /* check which builtin utility called us, so that we can print meaningful error messages below */
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

    /* positional and special parameters can't be set like this */
    if(is_pos_param(name_buf) || is_special_param(name_buf))
    {
        res = 1;
        fprintf(stderr, "%s: error setting/unsetting '%s' is not allowed\n", who, name_buf);
    }
    /* zero or negative result from do_set() means error */
    else if((res = do_set(name_buf, val, set_global, set_flags, unset_flags)) <= 0)
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
        {
             fprintf(stderr, ": readonly variable");
        }
        fprintf(stderr, "\n");
        res = 1;
    }
    else
    {
        res = 0;
    }
    return res;
}


/*
 * print the values and attributes of variables or functions, depending on the
 * flags parameter.
 */
void purge_vars(int flags)
{
    if(flag_set(flags, FLAG_FUNC) || flag_set(flags, FLAG_FUNC_NAME))
    {
        /* all the functions are stored in one global table */
        purge_table(func_table, flags);
    }
    else
    {
        /* print the variables in all the tables in the symbol table stack */
        int i;
        struct symtab_stack_s *stack = get_symtab_stack();
        for(i = 0; i < stack->symtab_count; i++)
        {
            purge_table(stack->symtab_list[i], flags);
        }
    }
}


/*
 * print the names and values of all the variables stored in the symbol
 * table *symtab.
 */
void purge_table(struct symtab_s *symtab, int flags)
{
    if(!symtab)
    {
        return;
    }
    
    /*
     * flags can restrict the output to function names and definitions, which
     * means we won't output regular variables.
     */
    int func_only     = flag_set(flags, FLAG_FUNC     );
    int funcname_only = flag_set(flags, FLAG_FUNC_NAME);
    
    /*
     * iterate through the symbol table entries, printing each in turn.
     * how we iterate through the entries depends on how we implement
     * the symbol table struct (hashtables vs linked lists).
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
            /* for each entry... */
            while(entry)
            {
                /* if we should only output functions, not variables... */
                if(func_only || funcname_only)
                {
                    /* and if this entry is a function definition... */
                    if(entry->val_type == SYM_FUNC)
                    {
                        /* print it */
                        purge_var(entry, funcname_only);
                    }
                }
                else
                {
                    /* otherwise, print the variable name, value and attribs */
                    purge_var(entry, 0);
                }
                /* and move on to the next entry */
                entry = entry->next;
            }
                
#ifdef USE_HASH_TABLES

        }
    }
        
#endif

}


/*
 * output the value and attributes of the symbol table entry.. if the entry
 * represents a variable, its name and value are printed.. if it represents a
 * function, its function name (and definition, if name_only == zero) is printed.
 */
void purge_var(struct symtab_entry_s *entry, int name_only)
{
    /* a function for which we should print the function definition (name_only == 0) */
    if(entry->val_type == SYM_FUNC && !name_only)
    {
        if(entry->val)
        {
            /* function definition strings start with '()' (bash) */
            if(entry->val[0] == '(' && entry->val[1] == ')')
            {
                printf("%s %s\n", entry->name, entry->val);
            }
            else
            {
                /* if by any chance the '()' are missing, add them manually */
                printf("%s () { %s }\n", entry->name, entry->val);
            }
        }
        else
        {
            /*
             * if the function entry contains a nodetree of the parsed function body,
             * convert it to a string and print it.
             */
            printf("%s () { ", entry->name);
            char *f = cmd_nodetree_to_str(entry->func_body);
            if(f)
            {
                printf("%s", f);
                free(f);
            }
            printf(" }\n");
        }
    }
    /* now print the variable/function name and attribs */
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
    /*
     * if the variable name starts with '-', add the special option '--', which tells
     * declare to end option parsing and start reading arguments (if this output is
     * used as input to the shell).
     */
    if(entry->name[0] == '-') printf("-- ");
    /* print the name */
    printf("%s", entry->name);
    /* if the entry is not a function, print the value */
    if(entry->val_type != SYM_FUNC)
    {
        putchar('=');
        print_quoted_val(entry->val);
    }
    putchar('\n');
}
