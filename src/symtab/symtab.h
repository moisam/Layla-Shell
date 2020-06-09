/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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

/*
 * Use hash tables to implement the symbol table struct. Remove this macro,
 * or set it to 0 if you want to use the linked lists implementation instead.
 */
#define USE_HASH_TABLES 1

/* the type of a symbol table entry's value */
enum symbol_type
{
    SYM_STR ,
    SYM_FUNC,
};

/* the symbol table entry structure */
struct symtab_entry_s
{
    char     *name;                   /* key */
    enum      symbol_type val_type;   /* type of value */
    char     *val;                    /* value */
    unsigned  int flags;              /* flags like readonly, export, ... */
    struct    symtab_entry_s *next;   /* pointer to the next entry */
    struct    node_s *func_body;      /* for functions, the nodetree of the function body */
};


#ifdef USE_HASH_TABLES

/* we are using hash tables to implement the symbol table struct */
#include "symtab_hash.h"

#else
/* we are using linked lists to implement the symbol table struct */
    struct symtab_s
    {
        int    level;
        struct symtab_entry_s *first, *last;
    };
#endif


/* values for the flags field of struct symtab_entry_s */
#define FLAG_EXPORT         (1 << 0)    /* export entry to forked commands */
#define FLAG_READONLY       (1 << 1)    /* entry is read only */
#define FLAG_CMD_EXPORT     (1 << 2)    /* used temporarily between cmd fork and exec */
#define FLAG_LOCAL          (1 << 3)    /* entry is local (to script or function) */
#define FLAG_ALLCAPS        (1 << 4)    /* convert value to capital letters when assigned */
#define FLAG_ALLSMALL       (1 << 5)    /* convert value to small letters when assigned */
#define FLAG_FUNCTRACE      (1 << 6)    /* enable function tracing (bash, ksh) */
#define FLAG_INTVAL         (1 << 7)    /* assign only integer values (bash) */
#define FLAG_SPECIAL_VAR    (1 << 8)    /* special shell variable, e.g. $TPERIOD or $RANDOM */
#define FLAG_TEMP_VAR       (1 << 9)    /* temp var (used during arithmetic expansion) */

/* the symbol table stack structure */
#define MAX_SYMTAB	256      /* maximum allowed symbol tables in the stack */
struct symtab_stack_s
{
    int    symtab_count;                            /* number of tables in the stack */
    struct symtab_s *symtab_list[MAX_SYMTAB];       /* pointers to the tables */
    struct symtab_s *global_symtab, *local_symtab;  /*
                                                     * pointers to the local
                                                     * and global symbol tables
                                                     */
};

/* symtab.c (or symtab_hash.c, depending on the used implementation) */
struct symtab_s       *new_symtab(int level);
void                   symtab_stack_add(struct symtab_s *symtab);
struct symtab_s       *symtab_stack_push(void);
struct symtab_s       *symtab_stack_pop(void);
int                    rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab);
void                   rem_from_any_symtab(struct symtab_entry_s *entry);
struct symtab_entry_s *add_to_any_symtab(char *symbol, struct symtab_s *st);
struct symtab_entry_s *add_to_symtab(char *symbol);
struct symtab_entry_s *do_lookup(char *str, struct symtab_s *symtable);
struct symtab_entry_s *get_local_symtab_entry(char *str);
struct symtab_entry_s *get_symtab_entry(char *str);
struct symtab_s       *get_local_symtab(void);
struct symtab_s       *get_global_symtab(void);
struct symtab_stack_s *get_symtab_stack(void);
void                   init_symtab(void);
void                   dump_local_symtab(void);
void                   free_symtab(struct symtab_s *symtab);
void                   symtab_entry_setval(struct symtab_entry_s *entry, char *val);
void                   merge_global(struct symtab_s *symtab);

#endif
