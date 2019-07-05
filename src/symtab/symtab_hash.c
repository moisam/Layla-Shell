/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: symtab_hash.c
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

struct symtab_stack_s symtab_stack;
int symtab_level;

/*
 * all defined in string_hash.c.
 */
extern const uint32_t fnv1a_prime;
extern const uint32_t fnv1a_seed ;
extern uint32_t fnv1a_hash_byte(unsigned char b, uint32_t hash); 
extern uint32_t fnv1a(char *text, uint32_t hash);

/*
 * TODO: If you want to use another hashing algorithm,
 *       change the function call to fnv1a() to any other 
 *       function of your liking.
 */
uint32_t calc_symhash(struct symtab_s *table, char *text)
{
    if(!table || !text) return 0;
    return fnv1a(text, fnv1a_seed) % table->size;
}

/*****************************************
 * Functions for manipulating hash tables.
 *****************************************/

struct symtab_s *alloc_hash_table()
{
    struct symtab_s *table = malloc(sizeof(struct symtab_s));
    if(!table)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for global symbol table");
    }
    table->size  = HASHTABLE_INIT_SIZE;
    table->used  = 0;
    size_t itemsz = HASHTABLE_INIT_SIZE * sizeof(struct symtab_entry_s *);
    table->items = malloc(itemsz);
    if(!table->items)
    {
        free(table);
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for global symbol table");
    }
    memset(table->items, 0, itemsz);
    return table;
}

void init_symtab()
{
    struct symtab_s *table = alloc_hash_table();
    table->level = 0;
    symtab_stack.symtab_count   = 1;
    symtab_level                = 0;
    symtab_stack.global_symtab  = table;
    symtab_stack.local_symtab   = table;
    symtab_stack.symtab_list[0] = table;
}

struct symtab_s *new_symtab(int level)
{
    struct symtab_s *table = alloc_hash_table();
    table->level = level;
    return table;
}

void symtab_stack_add(struct symtab_s *symtab)
{
    symtab_stack.symtab_list[symtab_stack.symtab_count++] = symtab;
    symtab_stack.local_symtab = symtab;
}

/* create an empty symbol table and push on top of stack */
struct symtab_s *symtab_stack_push()
{
    struct symtab_s *st = new_symtab(++symtab_level);
    symtab_stack_add(st);
    return st;
}

/* push an existing symbol table on top of stack */
struct symtab_s *symtab_stack_pushe(struct symtab_s *st)
{
    ++symtab_level;
    symtab_stack_add(st);
    return st;
}

struct symtab_s *symtab_stack_pop()
{
    /* can't pop past the global table */
    if(symtab_stack.symtab_count == 0) return (struct symtab_s *)NULL;
    struct symtab_s *st = symtab_stack.symtab_list[symtab_stack.symtab_count-1];
    symtab_stack.symtab_list[--symtab_stack.symtab_count] = 0;
    symtab_level--;
    if(symtab_stack.symtab_count == 0)
    {
        symtab_stack.local_symtab  = (struct symtab_s *)NULL;
        symtab_stack.global_symtab = (struct symtab_s *)NULL;
    }
    else symtab_stack.local_symtab = symtab_stack.symtab_list[symtab_stack.symtab_count-1];
    return st;
}

void free_symtab(struct symtab_s *symtab)
{
    if(!symtab || !symtab->items) return;
    if(symtab->used)
    {
        struct symtab_entry_s **h1 = symtab->items;
        struct symtab_entry_s **h2 = symtab->items + symtab->size;
        for( ; h1 < h2; h1++)
        {
            struct symtab_entry_s *entry = *h1;
            while(entry)
            {
                struct symtab_entry_s *next = entry->next;
                if(entry->name) free_malloced_str(entry->name);
                if(entry->val ) free_malloced_str(entry->val );
                if(entry->func_body) free_node_tree(entry->func_body);
                free(entry);
                entry = next;
            }
        }
    }
    free(symtab->items);
    free(symtab);
}


struct symtab_entry_s *__add_to_symtab(char *symbol, struct symtab_s *st)
{
    if(!st) return NULL;
    struct symtab_entry_s *entry = (struct symtab_entry_s *)malloc
                                    (sizeof(struct symtab_entry_s));
    if(!entry)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for new symbol table entry");
    }
    memset((void *)entry, 0, sizeof(struct symtab_entry_s));
    entry->name = get_malloced_str(symbol);
    /*
     * we are acting on the premise that a newly added variable
     * is bound to be accessed sooner than later, this is why we
     * add the new variable to the start of the linked list for 
     * that bucket. Think about it, when you declare a variable,
     * you are probably going to use it soon, right? Or maybe wrong,
     * but this is how we do it here :)
     */
    int index = calc_symhash(st, symbol);
    entry->next = st->items[index];
    st->items[index] = entry;
    st->used++;
    return entry;
}

void __rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab)
{
    int index = calc_symhash(symtab, entry->name);
    struct symtab_entry_s *e = symtab->items[index];
    struct symtab_entry_s *p = NULL;
    while(e)
    {
        if(e == entry)
        {
            /* if this is the first item in the list, adjust the table
             * pointer to point to the next item. Otherwise, ajust
             * the previous item's pointer to point to the next item.
             */
            if(!p) symtab->items[index] = e->next;
            else   p->next = e->next;
            /* free the memory used by this entry */
            if(entry->val) free_malloced_str(entry->val);
            if(entry->func_body) free_node_tree(entry->func_body);
            free_malloced_str(entry->name);
            symtab->used--;
            break;
        }
        p = e, e = e->next;
    }
    free(entry);
}

void rem_from_symtab(struct symtab_entry_s *entry)
{
    struct symtab_s *symtab = symtab_stack.local_symtab;
    __rem_from_symtab(entry, symtab);
}

struct symtab_entry_s *add_to_symtab(char *symbol)
{
    if(!symbol || *symbol == '\0') return 0;
    struct symtab_s *st = symtab_stack.local_symtab;

    /* do not duplicate an existing entry */
    struct symtab_entry_s *entry = NULL;
    if((entry = __do_lookup(symbol, st))) return entry;
    entry = __add_to_symtab(symbol, st);
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
    return entry;
}

struct symtab_entry_s *__do_lookup(char *str, struct symtab_s *symtab)
{
    if(!str || !symtab) return (struct symtab_entry_s *)NULL;
    int index = calc_symhash(symtab, str);
    struct symtab_entry_s *entry = symtab->items[index];
    while(entry)
    {
        if(is_same_str(entry->name, str)) return entry;
        entry = entry->next;
    }
    return (struct symtab_entry_s *)NULL;
}

struct symtab_entry_s *get_local_symtab_entry(char *str)
{
    return __do_lookup(str, symtab_stack.local_symtab);
}

struct symtab_entry_s *get_symtab_entry(char *str)
{
    int i = symtab_stack.symtab_count-1;
    do
    {
        struct symtab_s *symtab = symtab_stack.symtab_list[i];
        struct symtab_entry_s *entry = __do_lookup(str, symtab);
        if(entry) return entry;
    } while(--i >= 0);
    return (struct symtab_entry_s *)NULL;
}

struct symtab_s *get_local_symtab()
{
    return symtab_stack.local_symtab;
}

struct symtab_s *get_global_symtab()
{
    return symtab_stack.global_symtab;
}

struct symtab_stack_s *get_symtab_stack()
{
    return &symtab_stack;
}

void symtab_entry_setval(struct symtab_entry_s *entry, char *val)
{
    if(entry->val) free_malloced_str(entry->val);
    if(!val)
    {
        entry->val = NULL;
    }
    else
    {
        char *val2 = get_malloced_str(val);
        entry->val = val2;
        if     (flag_set(entry->flags, FLAG_ALLCAPS )) strupper(val2);
        else if(flag_set(entry->flags, FLAG_ALLSMALL)) strlower(val2);
    }
}

struct symtab_s *symtab_clone(struct symtab_s *symtab)
{
    if(!symtab) return (struct symtab_s *)NULL;
    struct symtab_s *clone = new_symtab(symtab->level);
    memcpy((void *)clone, (void *)symtab, sizeof(struct symtab_s));
    return clone;
}

/*
 * merge symbol table entries with the global symbol table.
 * useful for builtin utilities that need to merge their
 * local variable definitions with the global pool of shell vars.
 */
void merge_global(struct symtab_s *symtab)
{
    /*
     * instead of grabbing the global symbol table and directly add entries to it, we grab the
     * directly enclosing symbol table (i.e. the local symbol table) and work on it. this way we
     * can have multiple levels of indirection as the stack grows when commands call other commands.
     * when a command finishes execution, its symtab is merged with its caller's symtab. the latter
     * is merged with the caller's, and so on until the last command (the first to be executed) finishes
     * execution, in which case its symbol table is merged with the global symbol table.
     */
    //struct symtab_s *global = get_global_symtab();
    struct symtab_s *global = get_local_symtab();
    if(symtab->used)
    {
        struct symtab_entry_s **h1 = symtab->items;
        struct symtab_entry_s **h2 = symtab->items + symtab->size;
        for( ; h1 < h2; h1++)
        {
            struct symtab_entry_s *entry = *h1;
            while(entry)
            {
                /* don't merge explicitly declared local variables */
                if(!flag_set(entry->flags, FLAG_LOCAL))
                {
                    /*
                     * check for assignments to special variables like RANDOM, SECONDS and DIRSTACK. these
                     * assignments, when done as part of a builtin utility invocation, must be added to the
                     * current shell's execution environment. that's why we save them separately, otherwise
                     * they become just regular symbol table entries.
                     */
                    int i;
                    for(i = 0; i < special_var_count; i++)
                    {
                        if(strcmp(special_vars[i].name, entry->name) == 0) break;
                    }
                    if(i < special_var_count) set_special_var(entry->name, entry->val);
                    else
                    {
                        struct symtab_entry_s *gentry = __do_lookup(entry->name, global);
                        if(!gentry) gentry = __add_to_symtab(entry->name, global);
                        if(gentry)
                        {
                            /* don't overwrite readonly variables */
                            if(!flag_set(gentry->flags, FLAG_READONLY)) symtab_entry_setval(gentry, entry->val);
                        }
                    }
                }
                entry = entry->next;
            }
        }
    }
}

char *get_symbol_type_str(enum symbol_type type)
{
    switch(type)
    {
        case SYM_STR:    return "SYM_STR" ;
        case SYM_CHR:    return "SYM_CHR" ;
        case SYM_FUNC:   return "SYM_FUNC";
    }
    return "UNKNOWN";
}

void dump_local_symtab()
{
    struct symtab_s *symtab = symtab_stack.local_symtab;
    int i = 0;
    int indent = symtab->level << 2;
    fprintf(stderr, "%*sSymbol table [Level %d]:\r\n", indent, " ", symtab->level);
    fprintf(stderr, "%*s===========================\r\n", indent, " ");
    fprintf(stderr, "%*s  No               Symbol                    Val\r\n", indent, " ");
    fprintf(stderr, "%*s------ -------------------------------- ------------\r\n", indent, " ");
    if(symtab->used)
    {
        struct symtab_entry_s **h1 = symtab->items;
        struct symtab_entry_s **h2 = symtab->items + symtab->size;
        for( ; h1 < h2; h1++)
        {
            struct symtab_entry_s *entry = *h1;
            while(entry)
            {
                fprintf(stderr, "%*s[%04d] %-32s '%s'\r\n", indent, " ", 
                        i++, entry->name, entry->val);
                entry = entry->next;
            }
        }
    }
    fprintf(stderr, "%*s------ -------------------------------- ------------\r\n", indent, " ");
}
