/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: conditionals.c
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
 * parse a case item.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_case_item(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* case items can begin with an optional '(' */
    if(tok->type == TOKEN_OPENBRACE)
    {
        /* skip the '(' */
        tok = tokenize(tok->src);
    }
    /* create a new node for the case item */
    struct node_s *item = new_node(NODE_CASE_ITEM);
    if(!item)
    {
        return NULL;
    }
    item->lineno = lineno;
    /* loop to get the pattern list, which ends with ')' */
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_CLOSEBRACE)
    {
        /* create a new node for the next pattern */
        struct node_s *word = new_node(NODE_VAR);
        if(!word)   /* insufficient memory */
        {
            /* free the partially parsed nodetree */
            free_node_tree(item);
            return NULL;
        }
        /* copy the pattern to the new node */
        set_node_val_str(word, tok->text);
        word->lineno = tok->lineno;
        add_child_node(item, word);
        /* skip the pattern token */
        tok = tokenize(tok->src);
        /* skip pipe operators, which are used to separate patterns */
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_PIPE)
        {
            tok = tokenize(tok->src);
        }
    }
    tok->src->wstart = tok->src->curpos;
    tok = tokenize(tok->src);
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }
    /* the next command begins after the ')' char */
    if(tok->src->buffer[tok->src->wstart] == ')')
    {
        tok->src->wstart++;
    }
    /*
     * if we're running in POSIX mode, we only identify ';;' or 'esac' as
     * case item terminators.
     */
    if(option_set('P'))
    {
        if(!is_token_of_type(tok, TOKEN_DSEMI_ESAC))
        {
            /* parse the case item's compound list */
            struct node_s *compound = parse_compound_list(tok, TOKEN_DSEMI_ESAC);
            if(compound)
            {
                add_child_node(item, compound);
            }
        }
    }
    /*
     * if we're not running in POSIX mode, we identify ';;', 'esac', ';&', ';;&'
     * and ';|' as case item terminators.
     */
    else
    {
        if(!is_token_of_type(tok, TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR))
        {
            /* parse the case item's compound list */
            struct node_s *compound = parse_compound_list(tok, TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR);
            if(compound)
            {
                add_child_node(item, compound);
            }
        }
    }
    tok = get_current_token();
    /* check how this case item is terminated */
    if(tok->type == TOKEN_SEMI_AND)     /* ';&' */
    {
        set_node_val_chr(item, '&');
    }
    else if(tok->type == TOKEN_SEMI_OR ||       /* ';|' */
            tok->type == TOKEN_SEMI_SEMI_AND)   /* ';;&' */
    {
        set_node_val_chr(item, ';');
    }
    /* skip any remaining terminators */
    while(tok->type == TOKEN_DSEMI || tok->type == TOKEN_SEMI_AND)
    {
        tok = tokenize(tok->src);
    }
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok = tokenize(tok->src);
    }
    /* return the parsed nodetree */
    return item;
}


/* 
 * parse a case clause (or conditional), which can consist of one or more case items.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_case_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'case' */
    tok = tokenize(tok->src);
    /* have we reached EOF or error getting next token? */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* create a new node for the case clause */
    struct node_s *_case = new_node(NODE_CASE);
    if(!_case)
    {
        return NULL;
    }
    _case->lineno = lineno;
    /* create a new node for the name token */
    struct node_s *word = new_node(NODE_VAR);
    if(!word)
    {
        /* free the partially parsed nodetree */
        free_node_tree(_case);
        return NULL;
    }
    /* copy the name to the new node */
    set_node_val_str(word, tok->text);
    word->lineno = tok->lineno;
    add_child_node(_case, word);
    /* skip the name token */
    tok = tokenize(tok->src);
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok = tokenize(tok->src);
    }
    /* check for the 'in' keyword */
    if(tok->type != TOKEN_KEYWORD_IN)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_IN);
        /* free the partially parsed nodetree */
        free_node_tree(_case);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* skip the 'in' keyword */
    tok = tokenize(tok->src);
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok = tokenize(tok->src);
    }
    /* loop to parse the case item(s) until we hit EOF or esac, or an error occurs */
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR && tok->type != TOKEN_KEYWORD_ESAC)
    {
        /* parse the next case item */
        struct node_s *item = parse_case_item(tok);
        if(item)
        {
            add_child_node(_case, item);
        }
        tok = get_current_token();
    }
    /* the last token must be 'esac' */
    if(tok->type != TOKEN_KEYWORD_ESAC)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_ESAC);
        /* free the partially parsed nodetree */
        free_node_tree(_case);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* skip the 'esac' keyword */
    tok = tokenize(tok->src);
    /* return the parsed nodetree */
    return _case;
}


/* 
 * parse an if clause (or conditional), which can have a then, elif and else parts.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_if_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'if' */
    tok = tokenize(tok->src);
    /* create a new node for the if clause */
    struct node_s *_if = new_node(NODE_IF);
    if(!_if)
    {
        return NULL;
    }
    _if->lineno = lineno;
    /* parse the test part before 'then' */
    struct node_s *compound = parse_compound_list(tok, TOKEN_KEYWORD_THEN);
    if(compound)
    {
        add_child_node(_if, compound);
    }
    tok = get_current_token();
    /* it should end with the 'then' keyword */
    if(tok->type != TOKEN_KEYWORD_THEN)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_THEN);
        /* free the partially parsed nodetree */
        free_node_tree(_if);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->src->wstart = tok->src->curpos+1;
    /* go past the 'then' keyword */
    tok = tokenize(tok->src);
    /* parse the 'then' compound list, which can end in 'elif', 'else' or 'fi' */
    compound = parse_compound_list(tok, TOKEN_KEYWORDS_ELIF_ELSE_FI);
    if(compound && compound->children)
    {
        add_child_node(_if, compound);
    }
    else
    {
        /* missing 'else', 'elif' or 'fi' */
        PARSER_RAISE_ERROR_DESC(EXPECTED_TOKEN, tok, "expression");
        /* free the partially parsed nodetree */
        free_node_tree(compound);
        free_node_tree(_if);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok = get_current_token();
    tok->src->wstart = tok->src->curpos+1;
    /*
     * if the next token is 'elif', parse the test and compound list as a nested 'if' clause.
     * this will help us bind each 'else' to the nearest 'if'.
     */
    if(tok->type == TOKEN_KEYWORD_ELIF)
    {
        compound = parse_if_clause(tok);
        if(compound)
        {
            add_child_node(_if, compound);
        }
    }
    /* if the next token is 'else', parse the 'else' compound list */
    else if(tok->type == TOKEN_KEYWORD_ELSE)
    {
        tok = tokenize(tok->src);
        compound = parse_compound_list(tok, TOKEN_KEYWORD_FI);
        if(compound)
        {
            add_child_node(_if, compound);
        }
    }
    tok = get_current_token();
    /* normally, if conditionals end in a fi keyword */
    if(tok->type == TOKEN_KEYWORD_FI)
    {
        tok = tokenize(tok->src);
        return _if;
    }
    /*
     * if we had an 'elif' clause, token 'fi' would have been consumed
     * by it, so check the previous token.
     */
    tok = get_previous_token();
    if(tok && tok->type == TOKEN_KEYWORD_FI)
    {
        return _if;
    }
    /* token 'fi' is missing */
    tok = get_current_token();
    PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_FI);
    /* free the partially parsed nodetree */
    free_node_tree(_if);
    EXIT_IF_NONINTERACTIVE();
    return NULL;
}
