/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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

struct symtab_stack_s symtab_stack;
int symtab_level;

void init_symtab()
{
    symtab_stack.symtab_count = 1;
    symtab_level = 0;
    struct symtab_s *global_symtab = (struct symtab_s *)malloc(sizeof(struct symtab_s));
    if(!global_symtab)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for global symbol table");
    }
    memset((void *)global_symtab, 0, sizeof(struct symtab_s));
    symtab_stack.global_symtab  = global_symtab;
    symtab_stack.local_symtab   = global_symtab;
    symtab_stack.symtab_list[0] = global_symtab;
    global_symtab->level        = 0;
}

struct symtab_s *new_symtab(int level)
{
    struct symtab_s *symtab = (struct symtab_s *)malloc(sizeof(struct symtab_s));
    if(!symtab)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for new symbol table");
    }
    memset((void *)symtab, 0, sizeof(struct symtab_s));
    symtab->level = level;
    return symtab;
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
    if(symtab == NULL) return;
    struct symtab_entry_s *entry = symtab->first;
    while(entry)
    {
        if(entry->name) free_malloced_str(entry->name);
        if(entry->val ) free_malloced_str(entry->val );
        struct symtab_entry_s *next = entry->next;
        free(entry);
        entry = next;
    }
    free(symtab);
}


struct symtab_entry_s *__add_to_symtab(char *symbol, struct symtab_s *st)
{
    struct symtab_entry_s *entry = (struct symtab_entry_s *)malloc
                                    (sizeof(struct symtab_entry_s));
    if(!entry)
    {
        exit_gracefully(EXIT_FAILURE, "Fatal error: No memory for new symbol table entry");
    }
    memset((void *)entry, 0, sizeof(struct symtab_entry_s));
    entry->name = get_malloced_str(symbol);
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

void __rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab)
{
    if(entry->val      ) free_malloced_str(entry->val   );
    if(entry->func_body) free_node_tree(entry->func_body);
    free_malloced_str(entry->name);
    if(symtab->first == entry)
    {
        symtab->first = symtab->first->next;
        if(symtab->last == entry) symtab->last = NULL;
    }
    else
    {
        struct symtab_entry_s *e = symtab->first;
        struct symtab_entry_s *p = NULL;
        while(e && e != entry) p = e, e = e->next;
        p->next = entry->next;
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
    if(!symbol || symbol[0] == '\0') return 0;
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

struct symtab_entry_s *__do_lookup(char *str, struct symtab_s *symtable)
{
    if(!str || !symtable) return (struct symtab_entry_s *)NULL;
    struct symtab_entry_s *entry = symtable->first;
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
    do {
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
    if(!val) entry->val = NULL;
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
    struct symtab_s *global = get_global_symtab();
    struct symtab_entry_s *entry  = symtab->first;
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
    struct symtab_entry_s *entry = symtab->first;
    while(entry)
    {
        fprintf(stderr, "%*s[%04d] %-32s '%s'\r\n", indent, " ", 
                i++, entry->name, entry->val);
        entry = entry->next;
    }
    fprintf(stderr, "%*s------ -------------------------------- ------------\r\n", indent, " ");
}
