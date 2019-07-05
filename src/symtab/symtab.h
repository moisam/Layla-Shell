/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: symtab.h
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

#ifndef SYMTAB_H
#define SYMTAB_H

#define USE_HASH_TABLES 1


enum symbol_type
{
    SYM_STR,
    SYM_CHR,
    SYM_FUNC,
};

struct symtab_entry_s
{
    char   *name;
    enum   symbol_type val_type;
    char   *val;
    unsigned int flags;
    struct  symtab_entry_s *next;
    /* for functions */
    struct  node_s *func_body;
    struct  symtab_s *func_symtab;
};


#ifdef USE_HASH_TABLES

#include "symtab_hash.h"

#else
    struct symtab_s
    {
        int    level;
        struct symtab_entry_s *first, *last;
    };
#endif



#define FLAG_READONLY   (1 << 0)
#define FLAG_EXPORT     (1 << 1)
#define FLAG_LOCAL      (1 << 2)
#define FLAG_ALLCAPS    (1 << 3)
#define FLAG_ALLSMALL   (1 << 4)
#define FLAG_FUNCTRACE  (1 << 5)
#define FLAG_CMD_EXPORT (1 << 6)    /* used temporarily between cmd fork and exec */

struct symtab_stack_s
{
    int    symtab_count;
    struct symtab_s *symtab_list[256];
    struct symtab_s *global_symtab, *local_symtab;
};

/* symtab.c */
//extern struct symtab_stack_s symtab_stack;
//extern int symtab_level;
#define MAX_SYMTAB	256

struct symtab_s       *new_symtab(int level);
struct symtab_s       *symtab_stack_push();
struct symtab_s       *symtab_stack_pushe(struct symtab_s *st);
struct symtab_s       *symtab_stack_pop();
void                  __rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab);
void                   rem_from_symtab(struct symtab_entry_s *entry);
struct symtab_entry_s *__add_to_symtab(char *symbol, struct symtab_s *st);
struct symtab_entry_s *add_to_symtab(char *symbol);
struct symtab_entry_s *__do_lookup(char *str, struct symtab_s *symtable);
struct symtab_entry_s *get_local_symtab_entry(char *str);
struct symtab_entry_s *get_symtab_entry(char *str);
struct symtab_s       *get_local_symtab();
struct symtab_s       *get_global_symtab();
struct symtab_stack_s *get_symtab_stack();
struct symtab_s       *symtab_clone(struct symtab_s *symtab);
void                   init_symtab();
void                   dump_local_symtab();
void                   free_symtab(struct symtab_s *symtab);
void                   symtab_entry_setval(struct symtab_entry_s *entry, char *val);
void                   merge_global(struct symtab_s *symtab);

#endif
