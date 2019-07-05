/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: node.h
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

#ifndef NODE_TREE_H
#define NODE_TREE_H

// #include "symtab.h"

enum node_type_e
{
    /* program structs */
    NODE_PROGRAM,
    NODE_FUNCTION,
    /* POSIX shell constructs */
    NODE_SUBSHELL,
    NODE_LIST,
    NODE_ANDOR, NODE_AND_IF, NODE_OR_IF,
    NODE_BANG,
    NODE_PIPE,
    NODE_TERM,
    NODE_WORDLIST,
    NODE_VAR,
    NODE_FOR,
    NODE_SELECT,
    NODE_CASE_ITEM, NODE_CASE,
    NODE_IF, NODE_WHILE, NODE_UNTIL,
    NODE_IO_FILE, NODE_IO_HERE,
    NODE_IO_REDIRECT, NODE_IO_REDIRECT_LIST,
    NODE_ASSIGNMENT,
    NODE_COMMAND,
    /* non-POSIX extensions */
    NODE_ARITHMETIC_EXPR,
    NODE_TIME,
};


enum val_type_e
{
    VAL_VARPTR = 1, VAL_SINT, VAL_UINT,
    VAL_SLLONG, VAL_ULLONG,
    VAL_FLOAT, VAL_LDOUBLE,
    VAL_CHR, VAL_STR,
};

union symval_u
{
    long sint;
    unsigned long uint;
    long long sllong;
    unsigned long long ullong;
    double sfloat;
    long double ldouble;
    char chr;
    char *str;
};

struct node_s
{
    enum node_type_e type;
    enum val_type_e val_type;
    union symval_u val;

    int children;
    struct node_s *first_child;
    struct node_s *next_sibling, *prev_sibling;
    int lineno;
};

struct  node_s *new_node(enum node_type_e type);
void    add_child_node(struct node_s *parent, struct node_s *child);
//struct node_s *add_node_tree(struct node_s *parent, struct node_s *child);

//void set_node_val_ptr(struct node_s *node, struct symtab_entry_s *val);
void    set_node_val_sint(struct node_s *node, int val);
void    set_node_val_uint(struct node_s *node, unsigned int val);
void    set_node_val_sllong(struct node_s *node, long long val);
void    set_node_val_ullong(struct node_s *node, unsigned long long val);
void    set_node_val_sfloat(struct node_s *node, double val);
void    set_node_val_chr(struct node_s *node, char val);
void    set_node_val_str(struct node_s *node, char *val);
char   *get_node_type_str(enum node_type_e type);
void    dump_node_tree(struct node_s *func_body, int level);
void    free_node_tree(struct node_s *node);
// char *nodetree_to_str(struct node_s *root);
char   *cmd_nodetree_to_str(struct node_s *root);
struct  node_s *last_child(struct node_s *parent);

// extern struct node_s array_auto_bounds;

#endif
