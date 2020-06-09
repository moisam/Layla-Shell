/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
 * Create a new node and assign it the given type.
 * 
 * Returns the new node's struct, or NULL on error (we can also take the extreme
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
 * Add a child node to a parent node. The child is added as the last child
 * in the parent's children list.
 * 
 * Returns nothing.
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
 * Get the last child in the parent's children list.
 * 
 * Returns the last child node, or NULL if the parent has no children.
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
 * Set the node's value to the given integer value.
 */
void set_node_val_sint(struct node_s *node, long val)
{
    node->val_type = VAL_SINT;
    node->val.sint = val;
}


/*
 * Set the node's value to the given unsigned integer value.
 */
void set_node_val_uint(struct node_s *node, unsigned long val)
{
    node->val_type = VAL_UINT;
    node->val.uint = val;
}


/*
 * Set the node's value to the given long long integer value.
 */
void set_node_val_sllong(struct node_s *node, long long val)
{
    node->val_type = VAL_SLLONG;
    node->val.sllong = val;
}


/*
 * Set the node's value to the given unsigned long long integer value.
 */
void set_node_val_ullong(struct node_s *node, unsigned long long val)
{
    node->val_type = VAL_ULLONG;
    node->val.ullong = val;
}


/*
 * Set the node's value to the given double value.
 */
void set_node_val_sfloat(struct node_s *node, double val)
{
    node->val_type = VAL_FLOAT;
    node->val.sfloat = val;
}


/*
 * Set the node's value to the given double value.
 */
void set_node_val_sdouble(struct node_s *node, double val)
{
    node->val_type = VAL_FLOAT;
    node->val.sfloat = val;
}


/*
 * Set the node's value to the given long double value.
 */
void set_node_val_ldouble(struct node_s *node, long double val)
{
    node->val_type = VAL_LDOUBLE;
    node->val.ldouble = val;
}


/*
 * Set the node's value to the given char value.
 */
void set_node_val_chr(struct node_s *node, char val)
{
    node->val_type = VAL_CHR;
    node->val.chr = val;
}


/*
 * Set the node's value to the given string value.
 */
void set_node_val_str(struct node_s *node, char *val)
{
    node->val_type = VAL_STR;
    node->val.str = get_malloced_str(val);
}

/*
 * Return a string that describes the given node type. This is useful when we
 * are debugging the shell, or dumping the Abstract Source Tree (AST) of a parsed
 * translation unit (which the shell does when the dumpast '-d' option is set).
 * 
 * Returns a string describing the given node type.
 */
char *get_node_type_str(enum node_type_e type)
{
    switch(type)
    {
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
        case NODE_COPROC           : return "NODE_COPROC"          ;
    }
    return "UNKNOWN";
}


/*
 * Similar to get_node_type_str(), this function returns a string that describes
 * the given node's val type. This is useful when we are debugging the shell,
 * or dumping the Abstract Source Tree (AST) of a parsed translation unit
 * (which the shell does when the dumpast '-d' option is set).
 * 
 * Returns a string describing the given node type.
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
 * Dump the contents of all the nodes in the given node tree.
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
 * Free the memory used by the given nodetree.
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
 * The following functions convert different AST's to string format.
 */


#define INIT_NODETREE_BUF_SIZE      2048

#define CHECKED_NODETREE_APPEND(str, len)   \
if(!buf_append(&nodetree_buf, &nodetree_ptr,\
    &nodetree_buf_size, &nodetree_buf_len,  \
    (str), (len)))                          \
{                                           \
    return 0;                               \
}

#define CHECKED_HEREDOCS_APPEND(str, len)   \
if(!buf_append(&heredoc_buf, &heredoc_ptr,  \
    &heredoc_buf_size, &heredoc_buf_len,    \
    (str), (len)))                          \
{                                           \
    return 0;                               \
}
    

char  *nodetree_buf = NULL;
char  *nodetree_ptr = NULL;
size_t nodetree_buf_size = 0;
size_t nodetree_buf_len  = 0;

char  *heredoc_buf = NULL;
char  *heredoc_ptr = NULL;
size_t heredoc_buf_size = 0;
size_t heredoc_buf_len  = 0;


static inline int buf_append(char **buf, char **buf_ptr,
                             size_t *buf_size, size_t *buf_len,
                             char *str, size_t str_len)
{
    if((*buf) == NULL)
    {
        (*buf) = malloc(INIT_NODETREE_BUF_SIZE);
        if(!(*buf))
        {
            return 0;
        }
        (*buf_ptr) = (*buf);
        (*buf_size) = INIT_NODETREE_BUF_SIZE;
        (*buf_len) = 0;
    }
    else if(((*buf_len) + str_len) >= (*buf_size))
    {
        if(!ext_cmdbuf(buf, buf_size, *buf_size))
        {
            return 0;
        }
        (*buf_ptr) = (*buf) + (*buf_len);
    }
    
    sprintf((*buf_ptr), "%s", str);
    (*buf_ptr) += str_len;
    (*buf_len) += str_len;
    
    return 1;
}


void add_heredocs_to_tree(void)
{
    if(heredoc_buf_len == 0)
    {
        return;
    }
    
    if(nodetree_ptr[-1] != '\n')
    {
        if(!buf_append(&nodetree_buf, &nodetree_ptr,
            &nodetree_buf_size, &nodetree_buf_len, "\n", 1))
        {
            return;
        }
    }
    
    if(!buf_append(&nodetree_buf, &nodetree_ptr,
        &nodetree_buf_size, &nodetree_buf_len, heredoc_buf, heredoc_buf_len))
    {
        return;
    }
    
    if(nodetree_ptr[-1] != '\n')
    {
        if(!buf_append(&nodetree_buf, &nodetree_ptr,
            &nodetree_buf_size, &nodetree_buf_len, "\n", 1))
        {
            return;
        }
    }
    
    heredoc_buf_len = 0;
    heredoc_buf[0] = '\0';
}


int func_tree_to_str(struct node_s *node)
{
    if(node->val_type != VAL_STR)
    {
        return 0;
    }
    
    /* make sure we don't have heredocs from the last command */
    add_heredocs_to_tree();

    CHECKED_NODETREE_APPEND(node->val.str, strlen(node->val.str));
    CHECKED_NODETREE_APPEND("()\n{\n", 5);
    
    if(!cmd_nodetree_to_str(node->first_child, 0))
    {
        return 0;
    }
    
    /* make sure we don't have heredocs from the last command */
    add_heredocs_to_tree();
    
    CHECKED_NODETREE_APPEND("}\n", 2);

    return 1;
}


int list_tree_to_str(struct node_s *node)
{
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }

    while(child)
    {
        if(!cmd_nodetree_to_str(child, 0))
        {
            return 0;
        }
        
        /* make sure we don't have heredocs from the last command */
        add_heredocs_to_tree();

        if(nodetree_ptr[-1] != '\n')
        {
            CHECKED_NODETREE_APPEND("\n", 1);
        }

        child = child->next_sibling;
    }
    
    if(node->val_type == VAL_CHR)
    {
        if(node->val.chr == '&' && nodetree_ptr[-1] != '\n')
        {
            CHECKED_NODETREE_APPEND(" &", 2);
        }

        if(nodetree_ptr[-1] != '\n')
        {
            CHECKED_NODETREE_APPEND("\n", 1);
        }
    }
    
    return 1;
}


int andor_tree_to_str(struct node_s *node)
{
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }
    
    while(child)
    {
        struct node_s *child2 = NULL;
        
        if(child->type == NODE_AND_IF)
        {
            CHECKED_NODETREE_APPEND(" && ", 4);
            child2 = child->first_child;
        }
        else if(child->type == NODE_OR_IF)
        {
            CHECKED_NODETREE_APPEND(" || ", 4);
            child2 = child->first_child;
        }
        else
        {
            child2 = child;
        }
        
        if(!child2 || !cmd_nodetree_to_str(child2, 0))
        {
            return 0;
        }
        
        child = child->next_sibling;
    }
    
    return 1;
}


int pipe_tree_to_str(struct node_s *node)
{
    struct node_s *child = last_child(node);
    if(!child)
    {
        return 0;
    }
    
    while(child)
    {
        if(!cmd_nodetree_to_str(child, 0))
        {
            return 0;
        }

        child = child->prev_sibling;
        
        if(child)
        {
            CHECKED_NODETREE_APPEND(" | ", 3);
        }
    }
    
    return 1;
}


int word_nodes_to_str(struct node_s *node)
{
    int first = 1;
    struct node_s *child = node->first_child;
    
    if(!child)
    {
        return 0;
    }
    
    while(child)
    {
        if(!first)
        {
            CHECKED_NODETREE_APPEND(" ", 1);
        }
        
        if(child->val_type == VAL_STR && child->val.str)
        {
            char *str = child->val.str;
            CHECKED_NODETREE_APPEND(str, strlen(str));
        }
        else if(child->type == NODE_IO_REDIRECT)
        {
            if(!io_redirect_tree_to_str(child))
            {
                return 0;
            }
        }
        
        first = 0;
        child = child->next_sibling;
    }
    
    return 1;
}


int simple_cmd_tree_to_str(struct node_s *node)
{
    return word_nodes_to_str(node);
}


int io_redirect_tree_to_str(struct node_s *node)
{
    if(node->val.sint > 2)
    {
        char buf[8];
        sprintf(buf, "%ld", node->val.sint);
        CHECKED_NODETREE_APPEND(buf, strlen(buf));
    }

    node = node->first_child;
    if(!node)
    {
        return 0;
    }
    
    if(node->type == NODE_IO_FILE)
    {
        switch(node->val.chr)
        {
            case IO_FILE_LESS     : CHECKED_NODETREE_APPEND("<" , 1); break;
            case IO_FILE_LESSAND  : CHECKED_NODETREE_APPEND("<&", 2); break;
            case IO_FILE_LESSGREAT: CHECKED_NODETREE_APPEND("<>", 2); break;
            case IO_FILE_CLOBBER  : CHECKED_NODETREE_APPEND(">|", 2); break;
            case IO_FILE_GREAT    : CHECKED_NODETREE_APPEND(">" , 1); break;
            case IO_FILE_GREATAND : CHECKED_NODETREE_APPEND(">&", 2); break;
            case IO_FILE_DGREAT   : CHECKED_NODETREE_APPEND(">>", 2); break;
        }
        
        node = node->first_child;
        if(!node)
        {
            return 0;
        }
        
        char *str = node->val.str;
        CHECKED_NODETREE_APPEND(str, strlen(str));
    }
    else    /* heredoc or here-string */
    {
        /* is it a here-string? */
        if(node->val.chr == IO_HERE_STR)
        {
            CHECKED_NODETREE_APPEND("<<<", 3);
            
            node = node->first_child;                   /* here-string */
            if(!node)
            {
                return 0;
            }

            CHECKED_NODETREE_APPEND(node->val.str, strlen(node->val.str));
        }
        else        /* here-doc */
        {
            CHECKED_NODETREE_APPEND("<<", 2);
            
            node = node->first_child;                   /* heredoc body */
            if(!node)
            {
                return 0;
            }
            
            struct node_s *node2 = node->next_sibling;  /* delimiter word */
            if(!node2)
            {
                return 0;
            }
            
            char *str = node->val.str;
            char *str2 = node2->val.str;
            size_t len = strlen(str2);

            CHECKED_NODETREE_APPEND(str2, len);
            
            CHECKED_HEREDOCS_APPEND(str, strlen(str));
            CHECKED_HEREDOCS_APPEND(str2, len);
            CHECKED_HEREDOCS_APPEND("\n", 1);
        }
    }
    
    return 1;
}


int io_redirect_list_tree_to_str(struct node_s *node)
{
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }
    
    while(child)
    {
        if(!io_redirect_tree_to_str(child))
        {
            return 0;
        }
        
        child = child->next_sibling;
    }
    
    return 1;
}


int subshell_tree_to_str(struct node_s *node)
{
    CHECKED_NODETREE_APPEND("( ", 2);
    
    if(!cmd_nodetree_to_str(node->first_child, 0))
    {
        return 0;
    }
    
    CHECKED_NODETREE_APPEND(" )\n", 3);
    
    return 1;
}


int do_done_to_str(struct node_s *child)
{
    /* the DO keyword */
    CHECKED_NODETREE_APPEND("\ndo\n", 4);
    
    /* the loop body */
    if(child)
    {
        if(!cmd_nodetree_to_str(child, 0))
        {
            return 0;
        }
        
        /* make sure we don't have heredocs from the last command */
        add_heredocs_to_tree();
        
        /* the DONE keyword */
        if(nodetree_ptr[-1] != '\n')
        {
            CHECKED_NODETREE_APPEND("\n", 1);
        }
        
        CHECKED_NODETREE_APPEND("done", 4);
        
        /* the optional redirection list */
        child = child->next_sibling;
        if(child)
        {
            if(!io_redirect_list_tree_to_str(child))
            {
                return 0;
            }
        }
    }
    else
    {
        CHECKED_NODETREE_APPEND("done", 4);
    }
    
    if(nodetree_ptr[-1] != '\n')
    {
        CHECKED_NODETREE_APPEND("\n", 1);
    }
    
    return 1;
}


int while_until_tree_to_str(struct node_s *node)
{
    /* make sure we don't have heredocs from the last command */
    add_heredocs_to_tree();

    /* the loop header */
    if(node->type == NODE_WHILE)
    {
        CHECKED_NODETREE_APPEND("while ", 6);
    }
    else
    {
        CHECKED_NODETREE_APPEND("until ", 6);
    }
    
    /* the test clause */
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }
    
    if(!cmd_nodetree_to_str(child, 0))
    {
        return 0;
    }
    
    /* make sure we don't have heredocs from the last command */
    add_heredocs_to_tree();
    
    /* the loop body */
    return do_done_to_str(child->next_sibling);
}


int arithm_for_tree_to_str(struct node_s *node)
{
    /* the rest of the loop header */
    CHECKED_NODETREE_APPEND("((", 2);
    
    /* the loop clauses */
    struct node_s *child = node->first_child;
    int i = 0;
    static char *sep[] = { ";", ";", "))" };
    static int sep_len[] = { 1, 1, 2 };
    
    for( ; i < 3; i++)
    {
        if(!child || child->type != NODE_ARITHMETIC_EXPR)
        {
            return 0;
        }

        char *str = child->val.str;
        CHECKED_NODETREE_APPEND(str, strlen(str));
        CHECKED_NODETREE_APPEND(sep[i], sep_len[i]);
        
        child = child->next_sibling;
    }
    
    /* the loop body */
    return do_done_to_str(child);
}


int for_tree_to_str(struct node_s *node)
{
    /* make sure we don't have heredocs from the last command */
    add_heredocs_to_tree();
    
    /* the loop header */
    CHECKED_NODETREE_APPEND("for ", 4);
    
    /* the test clause */
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }
    
    if(child->type == NODE_ARITHMETIC_EXPR)
    {
        return arithm_for_tree_to_str(node);
    }
    
    /* the index variable */
    char *str = child->val.str;
    CHECKED_NODETREE_APPEND(str, strlen(str));

    /* the word list */
    child = child->next_sibling;
    
    if(child->type == NODE_WORDLIST)
    {
        CHECKED_NODETREE_APPEND(" in ", 4);
        
        if(!word_nodes_to_str(child))
        {
            return 0;
        }
        
        child = child->next_sibling;
    }
    
    /* the loop body */
    return do_done_to_str(child);
}


char *cmd_nodetree_to_str(struct node_s *node, int is_root)
{
    if(!node)
    {
        return NULL;
    }
    
    /* clear buf */
    if(is_root)
    {
        if(nodetree_buf)
        {
            nodetree_buf[0] = '\0';
            nodetree_buf_len = 0;
            nodetree_ptr = nodetree_buf;
        }

        if(heredoc_buf)
        {
            heredoc_buf[0] = '\0';
            heredoc_buf_len = 0;
            heredoc_ptr = heredoc_buf;
        }
        
        /* empty command tree */
        /*
         * we call __get_malloced_str() because the other call to cmd_nodetree_to_str()
         * doesn't call get_malloced_str(), which will confuse our caller when it wants
         * to free the returned string.
         */
        if(!node)
        {
            return __get_malloced_str("(no command)");
        }
    }
    
    int (*func)(struct node_s *node) = NULL;
    
    switch(node->type)
    {
        case NODE_FUNCTION:
            func = func_tree_to_str;
            break;

        case NODE_LIST:
        case NODE_TERM:
            func = list_tree_to_str;
            break;
            
        case NODE_COMMAND:
            func = simple_cmd_tree_to_str;
            break;
            
        case NODE_SUBSHELL:
            func = subshell_tree_to_str;
            break;
            
        case NODE_PIPE:
            func = pipe_tree_to_str;
            break;
            
        case NODE_ANDOR:
            func = andor_tree_to_str;
            break;
            
        case NODE_WHILE:
        case NODE_UNTIL:
            func = while_until_tree_to_str;
            break;
            
        case NODE_FOR:
            func = for_tree_to_str;
            break;
    }

    if(!func || !func(node))
    {
        return NULL;
    }
    
    if(nodetree_ptr[-1] != '\n')
    {
        CHECKED_NODETREE_APPEND("\n", 1);
    }
    
    if(is_root)
    {
        /* add 2 for '\0' and possible '\n' before the heredocs */
        char *str = malloc(nodetree_buf_len+heredoc_buf_len+2);
        if(!str)
        {
            return NULL;
        }
        
        strcpy(str, nodetree_buf);

        if(heredoc_buf_len)
        {
            str[nodetree_buf_len] = '\n';
            strcpy(str+nodetree_buf_len+1, heredoc_buf);
        }
        
        return str;
    }
    else
    {
        return nodetree_buf;
    }
}
