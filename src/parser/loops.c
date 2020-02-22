/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: loops.c
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
#include <ctype.h>
#include <unistd.h>
#include "../cmd.h"
#include "../scanner/scanner.h"
#include "node.h"
#include "parser.h"
#include "../error/error.h"
#include "../debug.h"


/*
 * get the word list that we use in for and select loops.
 * the list includes all words starting with the given token upto the first
 * non-word token (which can be a separator operator, the 'in' keyword, etc).
 * 
 * returns the word list as a nodetree, NULL in case of error.
 */
struct node_s *get_wordlist(struct token_s *tok)
{
    /* current token is a newline or ; */
    if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
    {
        return NULL;
    }

    /* create a new node for the list */
    struct node_s *wordlist = new_node(NODE_WORDLIST);
    if(!wordlist)   /* insufficient memory */
    {
        return NULL;
    }
    wordlist->lineno = tok->lineno;
    
    /* parse tokens and add words until we have a non-word token */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_WORD)
    {
        /* create a new node for the next word */
        struct node_s *word = new_node(NODE_VAR);
        if(!word)   /* insufficient memory */
        {
            /* free the partially parsed nodetree */
            free_node_tree(wordlist);
            return NULL;
        }
        
        /* copy the name to the new node */
        set_node_val_str(word, tok->text);
        word->lineno = tok->lineno;
        add_child_node(wordlist, word);
        
        /* move on to the next token */
        tok = tokenize(tok->src);
    }

    /* return the parsed nodetree */
    return wordlist;
}


/*
 * parse a traditional, POSIX-style 'for' loop:
 * 
 *     for i in list; do compound-list; done
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_for_loop(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'for' */
    tok = tokenize(tok->src);
    
    /* we must have a name token */
    if(!is_name(tok->text))
    {
        /* 
         * check for the second form of 'for' loops:
         * 
         *    for((expr1; expr2; expr3)); do commands; done
         * 
         * this is a non-POSIX extension used by all major shells.
         */
        if(tok->text && tok->text[0] == '(' && tok->text[1] == '(' && !option_set('P'))
        {
            return parse_for_loop2(tok);
        }
        
        /* error parsing for loop */
        PARSER_RAISE_ERROR_DESC(MISSING_FOR_NAME, tok, NULL);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->type = TOKEN_NAME;

    /* create a new node for the loop */
    struct node_s *_for = new_node(NODE_FOR);
    if(!_for)
    {
        return NULL;
    }
    _for->lineno = lineno;

    /* create a new node for the name token */
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        /* free the partially parsed nodetree */
        free_node_tree(_for);
        return NULL;
    }

    /* copy the name to the new node */
    set_node_val_str(name, tok->text);
    name->lineno = tok->lineno;
    add_child_node(_for, name);
    
    /* skip the name token */
    tok = tokenize(tok->src);
    
    /* skip optional newlines */
    skip_newline_tokens();
    
    /* check for the 'in' keyword, which is optional */
    if(tok->type == TOKEN_KEYWORD_IN)
    {
        tok = tokenize(tok->src);
    
        /* get the word list after 'in' */
        struct node_s *wordlist = get_wordlist(tok);
        if(wordlist)
        {
            add_child_node(_for, wordlist);
        }
        
        /* skip newline and ; operators */
        tok = get_current_token();
        if(tok->type == TOKEN_SEMI)
        {
            tok = tokenize(tok->src);
        }
        
        /* skip optional newlines */
        skip_newline_tokens();
    }

    /* parse the loop body, which is a do group */
    tok = get_current_token();
    struct node_s *parse_group = parse_do_group(tok);
    if(parse_group)
    {
        add_child_node(_for, parse_group);
    }
    
    /* return the parsed nodetree */
    return _for;
}


/*
 * skip all characters until we find the given char.. this involves skipping 
 * over quoted strings, semicolons, and word expansion structs.
 */
char *find_char(char *str, char c)
{
    size_t i;
    int endme = 0;
    
    while(*str)
    {
        switch(*str)
        {
            case  '"':
            case '\'':
            case  '`':
                /*
                 * for quote chars, add the quote, as well as everything between this
                 * quote and the matching closing quote, to the command line.
                 */
                i = find_closing_quote(str, (*str == '"') ? 1 : 0, 0);
                str += i;
                break;
                
            case '\\':
                str++;
                break;
                
            case '{':
            case '(':
            case '[':
                /* add the opening brace and everything up to the closing brace to the buffer */
                i = find_closing_brace(str, 0);
                str += i;
                break;
                
            default:
                if(*str == c)
                {
                    endme = 1;
                    break;
                }
        }
        
        if(endme)
        {
            break;
        }
        
        str++;
    }
    
    return str;
}


/* 
 * parse the second form of 'for' loops:
 * 
 *    for((expr1; expr2; expr3)); do commands; done
 * 
 * this is a non-POSIX extension used by all major shells.
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_for_loop2(struct token_s *tok)
{
    int lineno = tok->lineno;
    
    /* check we have '((' */
    if(!tok->text || tok->text[0] != '(' || tok->text[1] != '(')
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_LEFT_PAREN);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    
    int  i = 2;
    char *p = tok->text+i;
    char *pend = tok->text+tok->text_len;
    char *expr[3] = { NULL, NULL, NULL };

    expr[0] = p;
    p = find_char(p, ';');
    
    /* check we haven't reached EOF */
    if(p >= pend)
    {
        goto eof;
    }
    expr[0] = get_malloced_strl(expr[0], 0, p-expr[0]);
    
    /* get expression #2 */
    expr[1] = ++p;
    p = find_char(p, ';');
    
    /* check we haven't reached EOF */
    if(p >= pend)
    {
        free_malloced_str(expr[0]);
        goto eof;
    }
    expr[1] = get_malloced_strl(expr[1], 0, p-expr[1]);
    
    /* get expression #3 */
    expr[2] = ++p;
    p = find_char(p, ')');
    
    /* check we haven't reached EOF */
    if(p >= pend || p[1] != ')')
    {
        free_malloced_str(expr[0]);
        free_malloced_str(expr[1]);
        goto eof;
    }
    expr[2] = get_malloced_strl(expr[2], 0, p-expr[2]);
    
    /* create the node tree */
    struct node_s *_for = new_node(NODE_FOR);
    if(!_for)       /* insufficient memory */
    {
        /* free the strings */
        for(i = 0; i < 3; i++)
        {
            if(expr[i])
            {
                free_malloced_str(expr[i]);
            }
        }
        return NULL;
    }
    _for->lineno = lineno;
    
    /* add the 3 expressions to the nodetree */
    for(i = 0; i < 3; i++)
    {
        struct node_s *node = new_node(NODE_ARITHMETIC_EXPR);
        if(!node)   /* insufficient memory */
        {
            free_node_tree(_for);
            /* free the strings */
            for(i = 0; i < 3; i++)
            {
                if(expr[i])
                {
                    free_malloced_str(expr[i]);
                }
            }
            return NULL;
        }
        node->val_type = VAL_STR;
        node->val.str  = expr[i];
        expr[i] = NULL;
        add_child_node(_for, node);
    }
    
    /* now get the loop body */
    tok = tokenize(tok->src);
    
    /* skip newline and ; operators */
    if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
    {
        tok = tokenize(tok->src);
    }
    
    /* skip optional newlines */
    skip_newline_tokens();
    tok = get_current_token();
    
    /* parse the loop body, which is a do group */
    struct node_s *parse_group = parse_do_group(tok);
    if(parse_group)
    {
        add_child_node(_for, parse_group);
    }
    
    /* return the parsed nodetree */
    return _for;
    
eof:
    PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
    EXIT_IF_NONINTERACTIVE();
    return NULL;
}


/* 
 * parse a 'select' loop (note the similarities with the for loop's code above):
 * 
 *    select i in list; do commands; done
 * 
 * this is a non-POSIX extension used by all major shells.
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_select_loop(struct token_s *tok)
{
    int lineno = tok->lineno;
    
    /* go past 'select' */
    tok = tokenize(tok->src);
    
    /* we must have a name token */
    if(!is_name(tok->text))
    {
        PARSER_RAISE_ERROR_DESC(MISSING_SELECT_NAME, tok, NULL);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->type = TOKEN_NAME;
    
    /* create a new node for the loop */
    struct node_s *select = new_node(NODE_SELECT);
    if(!select)
    {
        return NULL;
    }
    select->lineno = lineno;
    
    /* create a new node for the name token */
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        /* free the partially parsed nodetree */
        free_node_tree(select);
        return NULL;
    }
    
    /* copy the name to the new node */
    set_node_val_str(name, tok->text);
    name->lineno = tok->lineno;
    add_child_node(select, name);
    
    /* skip the name token */
    tok = tokenize(tok->src);
    
    /* skip optional newlines */
    skip_newline_tokens();
    
    /* check for the 'in' keyowrd */
    if(tok->type == TOKEN_KEYWORD_IN)
    {
        tok = tokenize(tok->src);
        /* get the word list after 'in' */
        struct node_s *wordlist = get_wordlist(tok);
        if(wordlist)
        {
            add_child_node(select, wordlist);
        }
        
        /* skip newline and ; operators */
        if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
        {
            tok = tokenize(tok->src);
        }
        
        /* skip optional newlines */
        skip_newline_tokens();
    }
    
    /* parse the loop body, which is a do group */
    tok = get_current_token();
    struct node_s *parse_group = parse_do_group(tok);
    if(parse_group)
    {
        add_child_node(select, parse_group);
    }
    
    /* return the parsed nodetree */
    return select;
}


/* 
 * parse the while clause (or loop).
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_while_loop(struct token_s *tok)
{
    int lineno = tok->lineno;
    tok->src->wstart = tok->src->curpos;
    
    /* go past 'while' */
    tok = tokenize(tok->src);
    
    /* create a new node for the while clause */
    struct node_s *_while = new_node(NODE_WHILE);
    if(!_while)
    {
        return NULL;
    }
    _while->lineno = lineno;
    
    /* parse the test clause, which ends with the 'do' keyword */
    struct node_s *compound = parse_compound_list(tok, TOKEN_KEYWORD_DO);
    if(compound)
    {
        add_child_node(_while, compound);
        
        /* parse the loop body, which is a do group */
        tok = get_current_token();
        struct node_s *parse_group = parse_do_group(tok);
        if(parse_group)
        {
            add_child_node(_while, parse_group);
        }
        
        /* return the parsed nodetree */
        return _while;
    }
    else
    {
        /* error parsing the loop body. free the partially parsed nodetree */
        free_node_tree(_while);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
}


/* 
 * parse the until clause (or loop).
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_until_loop(struct token_s *tok)
{
    int lineno = tok->lineno;
    tok->src->wstart = tok->src->curpos;
    
    /* go past 'until' */
    tok = tokenize(tok->src);
    
    /* create a new node for the until clause */
    struct node_s *_until = new_node(NODE_UNTIL);
    if(!_until)
    {
        return NULL;
    }
    _until->lineno = lineno;
    
    /* parse the test clause, which ends with the 'do' keyword */
    struct node_s *compound = parse_compound_list(tok, TOKEN_KEYWORD_DO);
    if(compound)
    {
        add_child_node(_until, compound);
        
        /* parse the loop body, which is a do group */
        tok = get_current_token();
        struct node_s *parse_group = parse_do_group(tok);
        if(parse_group)
        {
            add_child_node(_until, parse_group);
        }
        
        /* return the parsed nodetree */
        return _until;
    }
    else
    {
        /* error parsing the loop body. free the partially parsed nodetree */
        free_node_tree(_until);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
}
