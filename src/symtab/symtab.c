/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: symtab.c
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
#include <string.h>
#include "../cmd.h"
#include "../parser/node.h"
#include "../parser/parser.h"
#include "../builtins/setx.h"
#include "../debug.h"
#include "symtab.h"

struct symtab_stack_s symtab_stack;     /* the symbol tables stack */
int    symtab_level;                    /* current level in the stack */


/*
 * Initialize the symbol table stack. Called on shell startup.
 * Does not return. If there is an error, the shell exits.
 */
void init_symtab(void)
{
    symtab_stack.symtab_count = 1;
    symtab_level = 0;
    struct symtab_s *global_symtab = malloc(sizeof(struct symtab_s));
    if(!global_symtab)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for global symbol table");
    }
    memset(global_symtab, 0, sizeof(struct symtab_s));
    symtab_stack.global_symtab  = global_symtab;
    symtab_stack.local_symtab   = global_symtab;
    symtab_stack.symtab_list[0] = global_symtab;
    global_symtab->level        = 0;
}


/*
 * Alloc memory for a new symbol table structure and give it the passed level.
 * Returns a pointer to the newly alloc'ed symbol table. Doesn't return in case
 * of error, as this function calls alloc_hash_table() and the latter doesn't
 * return on error.
 */
struct symtab_s *new_symtab(int level)
{
    struct symtab_s *symtab = malloc(sizeof(struct symtab_s));
    if(!symtab)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for new symbol table");
    }
    memset(symtab, 0, sizeof(struct symtab_s));
    symtab->level = level;
    return symtab;
}


/*
 * Push a symbol table on top the stack.
 */
void symtab_stack_add(struct symtab_s *symtab)
{
    symtab_stack.symtab_list[symtab_stack.symtab_count++] = symtab;
    symtab_stack.local_symtab = symtab;
}

/*
 * Create an empty symbol table and push on top the stack.
 * Returns the newly pushed symbol table.
 */
struct symtab_s *symtab_stack_push(void)
{
    struct symtab_s *st = new_symtab(++symtab_level);
    symtab_stack_add(st);
    return st;
}


/*
 * Pop the symbol table on top the stack, which we use as the local symbol
 * table. This happens when we finish executing a builtin utility or shell
 * function, in order to exit the local scope and return to the global scope.
 * Returns the popped symbol table, or NULL if the stack is empty.
 */
struct symtab_s *symtab_stack_pop(void)
{
    /* can't pop past the global table */
    if(symtab_stack.symtab_count == 0)
    {
        return NULL;
    }
    /* get the table on top the stack (the last table) */
    struct symtab_s *st = symtab_stack.symtab_list[symtab_stack.symtab_count-1];
    /* remove it from the stack */
    symtab_stack.symtab_list[--symtab_stack.symtab_count] = NULL;
    symtab_level--;
    /* empty stack. adjust symbol table pointers */
    if(symtab_stack.symtab_count == 0)
    {
        symtab_stack.local_symtab  = NULL;
        symtab_stack.global_symtab = NULL;
    }
    else
    {
        /* stack not empty. adjust the local symbol table pointer */
        symtab_stack.local_symtab = symtab_stack.symtab_list[symtab_stack.symtab_count-1];
    }
    return st;
}


/*
 * Release the memory used to store a symbol table structure, as well as the
 * memory used to store the strings of key/value pairs we have stored in
 * the table.
 */
void free_symtab(struct symtab_s *symtab)
{
    if(symtab == NULL)
    {
        return;
    }
    /* iterate through the entries list */
    struct symtab_entry_s *entry = symtab->first;
    while(entry)
    {
        /* free the key string */
        if(entry->name)
        {
            free_malloced_str(entry->name);
        }
        /* free the value string */
        if(entry->val)
        {
            free_malloced_str(entry->val);
        }
        /* if it's a function, free its function body */
        if(entry->func_body)
        {
            free_node_tree(entry->func_body);
        }
        struct symtab_entry_s *next = entry->next;
        /* free the entry itself and move to the next entry */
        free(entry);
        entry = next;
    }
    /* free the symbol table itself */
    free(symtab);
}


/*
 * Add a string to a symbol table. Creates an entry for the string, adds it to
 * the table, and then returns the new entry. In case of insufficient memory,
 * this function exits the shell instead of returning NULL.
 */
struct symtab_entry_s *add_to_any_symtab(char *symbol, struct symtab_s *st)
{
    if(!st)
    {
        return NULL;
    }
    /* alloc memory for the struct */
    struct symtab_entry_s *entry = malloc(sizeof(struct symtab_entry_s));
    if(!entry)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for new symbol table entry");
    }
    /* and initialize it */
    memset(entry, 0, sizeof(struct symtab_entry_s));
    /* get an malloc'd copy of the key string */
    entry->name = get_malloced_str(symbol);
    /* and add the entry to the table, adjusting pointers as necessary */
    if(!st->first)
    {
        st->first      = entry;
        st->last       = entry;
    }
    else
    {
        st->last->next = entry;
        st->last       = entry;
    }
    return entry;
}


/*
 * Remove an entry from a symbol table, given pointers to the entry and the
 * symbol table.
 */
int rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab)
{
    int res = 0;
    /* free the memory used by this entry's value */
    if(entry->val)
    {
        free_malloced_str(entry->val);
    }
    /* if it's a function, free the function body */
    if(entry->func_body)
    {
        free_node_tree(entry->func_body);
    }
    /* free the key string */
    free_malloced_str(entry->name);
    /* adjust the linked list's pointers */
    if(symtab->first == entry)
    {
        symtab->first = symtab->first->next;
        if(symtab->last == entry)
        {
            symtab->last = NULL;
        }
        res = 1;
    }
    else
    {
        struct symtab_entry_s *e = symtab->first;
        struct symtab_entry_s *p = NULL;
        while(e && e != entry)
        {
            p = e;
            e = e->next;
        }
        if(e == entry)
        {
            p->next = entry->next;
            res = 1;
        }
    }
    /* free the entry itself */
    free(entry);
    return res;
}


/*
 * Remove an entry from any symbol table in the stack.
 */
void rem_from_any_symtab(struct symtab_entry_s *entry)
{
    int i = symtab_stack.symtab_count-1;
    do
    {
        /*
         * starting with the local symtab, try to remove the entry.
         */
        if(rem_from_symtab(entry, symtab_stack.symtab_list[i]))
        {
            return;
        }
    } while(--i >= 0);      /* move up one level */
}


/*
 * Add a string to the local symbol table. First checks if the string is already
 * in the table or not, so as not to duplicate an entry. If not, creates an entry
 * for the string, adds it to the table, and then returns the new entry. In case of
 * insufficient memory, this function exits the shell instead of returning NULL.
 */
struct symtab_entry_s *add_to_symtab(char *symbol)
{
    if(!symbol || symbol[0] == '\0')
    {
        return NULL;
    }
    struct symtab_s *st = symtab_stack.local_symtab;
    /* do not duplicate an existing entry */
    struct symtab_entry_s *entry = NULL;
    if((entry = do_lookup(symbol, st)))
    {
        /* entry exists. return it */
        return entry;
    }
    /* entry does not exists. add it */
    entry = add_to_any_symtab(symbol, st);
    /* local var inherits value and attribs of global var of the same name (bash) */
    if(optionx_set(OPTION_LOCAL_VAR_INHERIT) && st != symtab_stack.global_symtab)
    {
        struct symtab_entry_s *entry2 = get_symtab_entry(symbol);
        if(entry2)
        {
            /* TODO: the nameref attribute (if implemented) should not be inherited */
            entry->flags = entry2->flags;
            symtab_entry_setval(entry, entry2->val);
        }
    }
    /* return the new entry */
    return entry;
}


/*
 * Search for a string in a symbol table.
 * Returns the entry for the given string, or NULL if its not found.
 */
struct symtab_entry_s *do_lookup(char *str, struct symtab_s *symtable)
{
    if(!str || !symtable)
    {
        return NULL;
    }

    /* search for the key string in the table's entries */
    struct symtab_entry_s *entry = symtable->first;
    while(entry)
    {
        /* found the string */
        if(is_same_str(entry->name, str))
        {
            /* if special or numeric var, update the var's value */
            char *val = NULL;
            if(flag_set(entry->flags, FLAG_SPECIAL_VAR))
            {
                val = get_special_var(entry->name, entry->val);
            }

            /*
            if(flag_set(entry->flags, FLAG_INTVAL) && entry->val)
            {
                val = arithm_expand(entry->val);
            }
            */
            
            if(val)
            {
                free_malloced_str(entry->val);
                entry->val = get_malloced_str(val);
                free(val);
            }

            return entry;
        }
        
        /* no match. check the next entry in the linked list */
        entry = entry->next;
    }
    
    /* string not found in the table. return NULL */
    return NULL;
}


/*
 * Search for a string in the local symbol table.
 * Returns the entry for the given string, or NULL if its not found.
 */
struct symtab_entry_s *get_local_symtab_entry(char *str)
{
    return do_lookup(str, symtab_stack.local_symtab);
}


/*
 * Search for a string in the symbol table stack, starting at the top (the local
 * symbol table), and checking every table, in turn, until we reach the bottom
 * (the global symbol table. If the entry is found at any level (any symbol table
 * in the stack), this entry is returned and the search stops. Otherwise, we check
 * the table at the higher level, and so on until we reach the global symbol table.
 *
 * Returns the entry for the given string, or NULL if its not found.
 */
struct symtab_entry_s *get_symtab_entry(char *str)
{
    int i = symtab_stack.symtab_count-1;
    do
    {
        /* start with the local symtab */
        struct symtab_s *symtab = symtab_stack.symtab_list[i];
        /* search for the key */
        struct symtab_entry_s *entry = do_lookup(str, symtab);
        /* entry found */
        if(entry)
        {
            return entry;
        }
    } while(--i >= 0);      /* move up one level */
    /* nothing found */
    return NULL;
}


/*
 * Return a pointer to the local symbol table (this changes as we change
 * scope by calling functions and builtin utilities).
 */
struct symtab_s *get_local_symtab(void)
{
    return symtab_stack.local_symtab;
}


/*
 * Return a pointer to the global symbol table (this stays the same as long
 * as the shell is running).
 */
struct symtab_s *get_global_symtab(void)
{
    return symtab_stack.global_symtab;
}


/*
 * Return a pointer to the symbol table stack.
 */
struct symtab_stack_s *get_symtab_stack(void)
{
    return &symtab_stack;
}


/*
 * Set the value string for the given entry, freeing the old entry's value (if any).
 */
void symtab_entry_setval(struct symtab_entry_s *entry, char *val)
{
    char *old_val = entry->val;

    /* new value is NULL */
    if(!val)
    {
        entry->val = NULL;
    }
    else
    {
        int free_val = 0;
        /* FLAG_ALLCAPS means we should transform all letters in value to capital */
        if(flag_set(entry->flags, FLAG_ALLCAPS))
        {
            val = __get_malloced_str(val);
            strupper(val);
            free_val = 1;
        }
        /* FLAG_ALLSMALL means we should transform all letters in value to small */
        else if(flag_set(entry->flags, FLAG_ALLSMALL))
        {
            val = __get_malloced_str(val);
            strlower(val);
            free_val = 1;
        }

        /* if special var, do whatever needs to be done in response to its new value */
        if(flag_set(entry->flags, FLAG_SPECIAL_VAR))
        {
            set_special_var(entry->name, val);
        }
        
        /* if int variable, perform arithmetic evaluation on the value (bash) */
        if(flag_set(entry->flags, FLAG_INTVAL))
        {
            val = arithm_expand(val);
            free_val = 1;
        }
        
        /* if error (e.g. arithmetic expansion failed), bail out */
        if(!val)
        {
            return;
        }

        entry->val = get_malloced_str(val);
        
        if(free_val && val)
        {
            free(val);
        }
    }

    /* free old value */
    if(old_val)
    {
        free_malloced_str(old_val);
    }
}


/*
 * Merge symbol table entries with the global symbol table. Useful for builtin
 * utilities that need to merge their local variable definitions with the global
 * pool of shell variables. This gives the illusion that builtin utilities and
 * functions defined their variables at the global level, while allowing these
 * tools to define their local variable that are not shared with the shell.
 */
void merge_global(struct symtab_s *symtab)
{
    struct symtab_entry_s *entry  = symtab->first;
    int global_scope = (get_global_symtab() == symtab);
    while(entry)
    {
        /* don't merge explicitly declared local variables */
        if(!flag_set(entry->flags, FLAG_LOCAL))
        {
            /* find the global entry for this local entry */
            struct symtab_entry_s *gentry = add_to_symtab(entry->name);
                
            /* overwrite the global entry's value with the local one */
            symtab_entry_setval(gentry, entry->val);
                
            /* set the flags */
            gentry->flags |= entry->flags;
                
            /*
             * unmark local command exports so they're not exported to 
             * other commands, but don't mark the global entry with the 
             * local flag.
             */
            //gentry->flags &= ~(FLAG_CMD_EXPORT | FLAG_LOCAL);
            gentry->flags &= ~FLAG_CMD_EXPORT;
            if(global_scope)
            {
                gentry->flags &= ~FLAG_LOCAL;
            }
        }
        /* move on to the next entry */
        entry = entry->next;
    }
}


/*
 * Dump the local symbol table, by printing the symbols, their keys and values.
 * Used in debugging the shell, as well as when we invoke `dump symtab`.
 */
void dump_local_symtab(void)
{
    struct symtab_s *symtab = symtab_stack.local_symtab;
    int i = 0;
    int indent = symtab->level << 2;
    fprintf(stderr, "%*sSymbol table [Level %d]:\n", indent, " ", symtab->level);
    fprintf(stderr, "%*s===========================\n", indent, " ");
    fprintf(stderr, "%*s  No               Symbol                    Val\n", indent, " ");
    fprintf(stderr, "%*s------ -------------------------------- ------------\n", indent, " ");
    struct symtab_entry_s *entry = symtab->first;
    while(entry)
    {
        fprintf(stderr, "%*s[%04d] %-32s '%s'\n", indent, " ", 
                i++, entry->name, entry->val);
        entry = entry->next;
    }
    fprintf(stderr, "%*s------ -------------------------------- ------------\n", indent, " ");
}
