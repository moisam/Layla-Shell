/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: node.c
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "../cmd.h"
#include "node.h"
#include "parser.h"
#include "../debug.h"


/*
 * create a new node and assign it the given type.
 * 
 * returns the new node's struct, or NULL on error (we can also take the extreme
 * approach and exit the shell if there's not enough memory for the node struct).
 */
struct node_s *new_node(enum node_type_e type)
{
    struct node_s *node = malloc(sizeof(struct node_s));
    if(!node)
    {
        //exit_gracefully(EXIT_FAILURE, "fatal error: Not enough memory for parser node struct");
        return NULL;
    }
    /* initialize the struct */
    memset(node, 0, sizeof(struct node_s));
    /* set the node type */
    node->type = type;
    /* return the node struct */
    return node;
}


/*
 * add a child node to a parent node.. the child is added as the last child
 * in the parent's children list.
 * 
 * returns nothing.
 */
void add_child_node(struct node_s *parent, struct node_s *child)
{
    /* sanity checks */
    if(!parent || !child)
    {
        return;
    }
    if(!parent->first_child)
    {
        /* parent has no children. add as the first child */
        parent->first_child = child;
    }
    else
    {
        /* parent has no children. add at the end of the list */
        struct node_s *sibling = last_child(parent);
        sibling->next_sibling = child;
        child->prev_sibling = sibling;
    }
    /* increment parent's child count */
    parent->children++;
}


/*
 * get the last child in the parent's children list.
 * 
 * returns the last child node, or NULL if the parent has no children.
 */
struct node_s *last_child(struct node_s *parent)
{
    /* sanity check */
    if(!parent)
    {
        return NULL;
    }
    /* parent has no children */
    if(!parent->first_child)
    {
        return NULL;
    }
    /* find the last child */
    struct node_s *child = parent->first_child;
    while(child->next_sibling)
    {
        child = child->next_sibling;
    }
    /* and return it */
    return child;
}


/*
 * set the node's value to the given integer value.
 */
void set_node_val_sint(struct node_s *node, int val)
{
    node->val_type = VAL_SINT;
    node->val.sint = val;
}


/*
 * set the node's value to the given unsigned integer value.
 */
void set_node_val_uint(struct node_s *node, unsigned int val)
{
    node->val_type = VAL_UINT;
    node->val.uint = val;
}


/*
 * set the node's value to the given long long integer value.
 */
void set_node_val_sllong(struct node_s *node, long long val)
{
    node->val_type = VAL_SLLONG;
    node->val.sllong = val;
}


/*
 * set the node's value to the given unsigned long long integer value.
 */
void set_node_val_ullong(struct node_s *node, unsigned long long val)
{
    node->val_type = VAL_ULLONG;
    node->val.ullong = val;
}


/*
 * set the node's value to the given double value.
 */
void set_node_val_sfloat(struct node_s *node, double val)
{
    node->val_type = VAL_FLOAT;
    node->val.sfloat = val;
}


/*
 * set the node's value to the given double value.
 */
void set_node_val_sdouble(struct node_s *node, double val)
{
    node->val_type = VAL_FLOAT;
    node->val.sfloat = val;
}


/*
 * set the node's value to the given long double value.
 */
void set_node_val_ldouble(struct node_s *node, long double val)
{
    node->val_type = VAL_LDOUBLE;
    node->val.ldouble = val;
}


/*
 * set the node's value to the given char value.
 */
void set_node_val_chr(struct node_s *node, char val)
{
    node->val_type = VAL_CHR;
    node->val.chr = val;
}


/*
 * set the node's value to the given string value.
 */
void set_node_val_str(struct node_s *node, char *val)
{
    node->val_type = VAL_STR;
    node->val.str = get_malloced_str(val);
}

/*
 * return a string that describes the given node type.. this is useful when we
 * are debugging the shell, or dumping the Abstract Source Tree (AST) of a parsed
 * translation unit (which the shell does when the dumpast '-d' option is set).
 * 
 * returns a string describing the given node type.
 */
char *get_node_type_str(enum node_type_e type)
{
    switch(type)
    {
        case NODE_PROGRAM          : return "NODE_PROGRAM"         ;
        case NODE_FUNCTION         : return "NODE_FUNCTION"        ;
        case NODE_SUBSHELL         : return "NODE_SUBSHELL"        ;
        case NODE_LIST             : return "NODE_LIST"            ;
        case NODE_ANDOR            : return "NODE_ANDOR"           ;
        case NODE_AND_IF           : return "NODE_AND_IF"          ;
        case NODE_OR_IF            : return "NODE_OR_IF"           ;
        case NODE_BANG             : return "NODE_BANG"            ;
        case NODE_PIPE             : return "NODE_PIPE"            ;
        case NODE_TERM             : return "NODE_TERM"            ;
        case NODE_WORDLIST         : return "NODE_WORDLIST"        ;
        case NODE_VAR              : return "NODE_VAR"             ;
        case NODE_FOR              : return "NODE_FOR"             ;
        case NODE_CASE_ITEM        : return "NODE_CASE_ITEM"       ;
        case NODE_CASE             : return "NODE_CASE"            ;
        case NODE_IF               : return "NODE_IF"              ;
        case NODE_WHILE            : return "NODE_WHILE"           ;
        case NODE_UNTIL            : return "NODE_UNTIL"           ;
        case NODE_IO_FILE          : return "NODE_IO_FILE"         ;
        case NODE_IO_HERE          : return "NODE_IO_HERE"         ;
        case NODE_IO_REDIRECT      : return "NODE_IO_REDIRECT"     ;
        case NODE_IO_REDIRECT_LIST : return "NODE_IO_REDIRECT_LIST";
        case NODE_ASSIGNMENT       : return "NODE_ASSIGNMENT"      ;
        case NODE_COMMAND          : return "NODE_COMMAND"         ;
        case NODE_SELECT           : return "NODE_SELECT"          ;
        case NODE_ARITHMETIC_EXPR  : return "NODE_ARITHMETIC_EXPR" ;
        case NODE_TIME             : return "NODE_TIME"            ;
    }
    return "UNKNOWN";
}


/*
 * similar to get_node_type_str(), this function returns a string that describes
 * the given node's val type.. this is useful when we are debugging the shell,
 * or dumping the Abstract Source Tree (AST) of a parsed translation unit
 * (which the shell does when the dumpast '-d' option is set).
 * 
 * returns a string describing the given node type.
 */
char *get_node_val_type_str(enum val_type_e type)
{
    switch(type)
    {
        case VAL_SINT   : return "VAL_SINT"   ;
        case VAL_UINT   : return "VAL_UINT"   ;
        case VAL_SLLONG : return "VAL_SLLONG" ;
        case VAL_ULLONG : return "VAL_ULLONG" ;
        case VAL_FLOAT  : return "VAL_FLOAT"  ;
        case VAL_LDOUBLE: return "VAL_LDOUBLE";
        case VAL_CHR    : return "VAL_CHR"    ;
        case VAL_STR    : return "VAL_STR"    ;
    }
    return "UNKNOWN";
}


/*
 * dump the contents of all the nodes in the given node tree.
 */
void dump_node_tree(struct node_s *root, int level)
{
    if(!root)
    {
        return;
    }
    int indent = level << 2;
    fprintf(stderr, "%*sNODE: type '%s', val_type '%s', val '", indent, " ",
            get_node_type_str(root->type),
            get_node_val_type_str(root->val_type));

    switch(root->val_type)
    {
        case VAL_SINT   :
            fprintf(stderr, "%ld" , root->val.sint   );
            break;
            
        case VAL_UINT   :
            fprintf(stderr, "%lu" , root->val.uint   );
            break;
            
        case VAL_SLLONG :
            fprintf(stderr, "%lld", root->val.sllong );
            break;
            
        case VAL_ULLONG :
            fprintf(stderr, "%llu", root->val.ullong );
            break;
            
        case VAL_FLOAT  :
            fprintf(stderr, "%f"  , root->val.sfloat );
            break;
            
        case VAL_LDOUBLE:
            fprintf(stderr, "%Lg" , root->val.ldouble);
            break;
            
        case VAL_CHR    :
            fprintf(stderr, "%c"  , root->val.chr    );
            break;
            
        case VAL_STR    :
            fprintf(stderr, "%s"  , root->val.str    );
            break;
            
        default:
            break;
    }
    fprintf(stderr, "'\n");
    struct node_s *child = root->first_child;
    while(child)
    {
        dump_node_tree(child, level+1);
        child = child->next_sibling;
    }
    fflush(stderr);
}


/*
 * free the memory used by the given nodetree.
 */
void free_node_tree(struct node_s *node)
{
    if(!node)
    {
        return;
    }
    struct node_s *child = node->first_child;
    /* free all children */
    while(child)
    {
        struct node_s *next = child->next_sibling;
        free_node_tree(child);
        child = next;
    }
    /* if the node's value is a string, free it */
    if(node->val_type == VAL_STR)
    {
        if(node->val.str)
        {
            free_malloced_str(node->val.str);
        }
    }
    /* free the node iteself */
    free(node);
}


/*
 * get the length of the node's value field, i.e. how much memory we need to alloc
 * if we wanted to make a copy of the value field.
 */
size_t get_node_len(struct node_s *root)
{
    if(!root)
    {
        return 0;
    }
    size_t len = 0;
    struct node_s *child = root->first_child;
    /* special handling of redirection nodes */
    if(child->type == NODE_IO_REDIRECT)
    {
        struct node_s *io = child->first_child;
        struct node_s *file = io->first_child;
        return strlen(file->val.str)+7;
    }
    /* determine value length according to value type */
    switch(root->val_type)
    {
        case VAL_CHR:
            len = 1;
            break;
            
        case VAL_STR:
            len = strlen(root->val.str);
            break;
            
        case VAL_SINT:
        case VAL_UINT:
            len = 10;
            break;
            
        default:
            len = 1;
    }
    return len;
}


/*
 * get the collective length of the string values of all the nodes in the
 * given nodetree.. useful when we want to reconstruct a command string out
 * of the command's parsed nodetree.
 */
size_t get_nodetree_len(struct node_s *root)
{
    if(!root)
    {
        return 0;
    }
    size_t len = root->val_type == VAL_STR ? strlen(root->val.str) : 0;
    struct node_s *child = root->first_child;
    /* get lengths of the string values of all children */
    while(child)
    {
        len += get_nodetree_len(child)+1;
        child = child->next_sibling;
    }
    /* return collective length */
    return len+1;
}

/*
 * convert a nodetree to a string, i.e. reconstruct the original command line from
 * the parsed nodetree.
 * 
 * WARNING: this function is severely defective. it can't process many commands, such
 *          as loops and if/case conditionals. as a matter of fact, we should forget
 *          this function altogether and find a better way to store the original
 *          command in the node structure itself.
 */
char *cmd_nodetree_to_str(struct node_s *root)
{
    if(!root)
    {
        return NULL;
    }
    size_t len = get_nodetree_len(root);
    if(!len)
    {
        return NULL;
    }
    char *str = malloc(len+1);
    if(!str)
    {
        return NULL;
    }
    *str = '\0';
    char buf[32];
    char *tmp;
    /*
     * pipes have their children in reverse direction.
     * we need to account for this in the loop.
     */
    int ispipe = root->type == NODE_PIPE;
    struct node_s *child = ispipe ? last_child(root) : root->first_child;
    char *separator = " ";
    if(ispipe)
    {
        separator = " | ";
    }
    else
    {
        switch(child->type)
        {
            case NODE_LIST:
                if(child->val.chr == '&')
                {
                    separator = "& ";
                }
                else
                {
                    separator = "; ";
                }
                break;
                
            case NODE_COMMAND:
                separator = "; ";
                break;
                
            default:
                break;
        }
    }
    
    /* now loop on children */
    while(child)
    {
        switch(child->type)
        {
            case NODE_IO_REDIRECT:
                if(child->val.sint > 2)
                {
                    sprintf(buf, "%ld", child->val.sint);
                    strcat(str, buf);
                }
                struct node_s *io = child->first_child;
                if(io->type == NODE_IO_FILE)
                {
                    switch(io->val.chr)
                    {
                        case IO_FILE_LESS     : strcat(str, "< " ); break;
                        case IO_FILE_LESSAND  : strcat(str, "<& "); break;
                        case IO_FILE_LESSGREAT: strcat(str, "<> "); break;
                        case IO_FILE_CLOBBER  : strcat(str, ">| "); break;
                        case IO_FILE_GREAT    : strcat(str, "> " ); break;
                        case IO_FILE_GREATAND : strcat(str, ">& "); break;
                        case IO_FILE_DGREAT   : strcat(str, ">> "); break;
                    }
                    struct node_s *file = io->first_child;
                    strcat(str, file->val.str);
                }
                else
                {
                    strcat(str, "<< ");
                    struct node_s *heredoc = io->first_child;
                    strcat(str, heredoc->val.str);
                }
                break;

            case NODE_SUBSHELL:
                strcat(str, "( ");
                tmp = cmd_nodetree_to_str(child);
                if(tmp)
                {
                    strcat(str, tmp);
                    free(tmp);
                }
                strcat(str, " )");
                break;

            case NODE_LIST:
            case NODE_TERM:
                tmp = cmd_nodetree_to_str(child);
                if(tmp)
                {
                    strcat(str, tmp);
                    free(tmp);
                }
                break;

            case NODE_COMMAND: ;
                struct node_s *child2 = child->first_child;
                while(child2)
                {
                    if(child2->val_type == VAL_STR && child2->val.str)
                    {
                        strcat(str, child2->val.str);
                        if(child2->next_sibling)
                        {
                            strcat(str, " ");
                        }
                    }
                    child2 = child2->next_sibling;
                }
                break;
        
            default:
                if(child->val_type == VAL_STR && child->val.str)
                {
                    strcat(str, child->val.str);
                }
                break;
        
        }
        child = ispipe ? child->prev_sibling : child->next_sibling;
        if(child)
        {
            strcat(str, separator);
        }
    }
    return str;
}
