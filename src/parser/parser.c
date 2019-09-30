/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: parser.c
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
#include "../error/error.h"
#include "../debug.h"
#include "../builtins/setx.h"
#include "node.h"
#include "parser.h"

/*********************************************/
/* top-down, recursive descent syntax parser */
/*********************************************/

/* pointer to the current function definition we're parsing */
struct symtab_entry_s *current_func = (struct symtab_entry_s *)NULL;

/* dummy struct to indicate func definitions in src */
struct node_s __node_func_def;
struct node_s *node_func_def = &__node_func_def;

/* flag to indicate a parsing error */
int parser_err = 0;


/*
 * substitute a token whose text we identified as an alias word
 * with the value of that alias.. the *alias parameter tells us the
 * alias word we're substituting, so that we won't enter an infinite
 * loop where we keep substituting an alias by itself.. for example,
 * the alias: ls='ls --color=auto', when processed, results in the same
 * alias word 'ls'.. if we tried to substitute it again, we'll loop
 * infinitely.
 *
 * returns 1 if the alias was successfully substituted, 0 otherwise.
 */
int substitute_alias(char *buf, char *alias)
{
    char   *alias_val;
    int     name_len ;
    int     val_len  ;
    int     res = 1;
    char   *p = buf, *p2, *p3;
    size_t  i;
    int     sep = 1;
    while(*p)
    {
        switch(*p)
        {
            /* separator chars terminate words */
            case '(':
            case ')':
            case '{':
            case '}':
            case '[':
            case ']':
            case '|':
            case ';':
            case '&':
            case '!':
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                sep = 1;
                p++;
                break;

            /* non-separator chars that can also terminate words */
            case '\\':
            case '-':
            case '+':
            case '=':
                sep = 0;
                p++;
                break;

            case '\'':
            case '"':
            case '`':
                /* if the quote is escaped */
                if(p != buf && p[-1] == '\\')
                {
                    /* skip escaped quotes */
                    p++;
                }
                else
                {
                    /* otherwise, find closing quote and skip the string between the quotes */
                    i = find_closing_quote(p, 0);
                    if(i)
                    {
                        p = p+i;
                    }
                    else
                    {
                        p++;
                    }
                }
                sep = 0;
                break;

            default:
                /* skip all chars that can be part of the alias word */
                p2 = p;
                while(*p2)
                {
                    if(isalnum(*p2) || *p2 == '_' || *p2 == '!' || *p2 == '%' || *p2 == ',' || *p2 == '@')
                    {
                        /* valid chars */
                        p2++;
                    }
                    else
                    {
                        /* invalid chars. break the loop */
                        break;
                    }
                }
                /* we haven't moved forward, meaning the first char is invalid for an alias name */
                if(p2 == p)
                {
                    p++;
                    break;
                }
                /* only consider possible aliases if they come after a separator token */
                if(!sep)
                {
                    p = p2;
                    break;
                }
                /* get a copy of that alias word */
                name_len  = p2-p;
                char a[name_len+1];
                strncpy(a, p, name_len);
                a[name_len] = '\0';
                /*
                 * prevent alias recursion by making sure we're not processing the same alias
                 * again (see the 'ls' example in the comment at the beginning of this function).
                 */
                if(alias && strcmp(a, alias) == 0)
                {
                    p = p2;
                    res = 0;
                    sep = 0;
                    break;
                }
                /* get the aliased value */
                alias_val = parse_alias(a);
                /* no aliased value, or value is empty, or value is the same alias name */
                if(!alias_val || strcmp(a, alias_val) == 0)
                {
                    /* don't bother substituting anything. keep the alias name as-is */
                    p = p2;
                    res = 0;
                    sep = 0;
                    break;
                }
                val_len = strlen(alias_val);
                /* give some room for the alias value */
                p2 = p+name_len;
                p3 = p+val_len ;
                while((*p3++ = *p2++))
                {
                    ;
                }
                /* now copy the value */
                strncpy(p, alias_val, val_len);
                /* adjust our pointer */
                p = p3-1;
                sep = 0;
                break;
        }
    }
    return res;
}


/*
 * NOTE: we should only check the first word of a command for
 *       possible alias substitution. also, we should check if the
 *       last char of an alias substitution is space, in which case
 *       we will need to check the next word for alias substitution.
 */
void check_word_for_alias_substitution(struct token_s *tok)
{
    if(valid_alias_name(tok->text))
    {
        char *a = get_malloced_str(tok->text);
        if(a)
        {
            if(substitute_alias(tok->text, NULL))
            {
                substitute_alias(tok->text, a);
            }
            free_malloced_str(a);
            tok->text_len = strlen(tok->text);
        }
    }
}


/*
 * return the command string of the command being parsed.
 */
char *get_cmdwords(struct token_s *tok, int wstart)
{
    if(!tok || !tok->src)
    {
        return NULL;
    }
    struct source_s *src = tok->src;
    /* find the end of the command string */
    int wend = tok->linestart + tok->charno - tok->text_len;
    /* at the very first word we would have a -ve position */
    if(wstart < 0)
    {
        wstart = 0;
    }
    /* skip spaces before the command string */
    while(isspace(src->buffer[wstart]))
    {
        wstart++;
    }
    /* ignore spaces after the command string */
    while(isspace(src->buffer[wend]))
    {
        wend--;
    }
    /* make sure we get the last char in the command string */
    while(src->buffer[wend] && !isspace(src->buffer[wend]))
    {
        wend++;
    }
    /* invalid pointers */
    if(wstart >= wend)
    {
        return NULL;
    }
    /* return the malloc'd command string */
    return get_malloced_strl(src->buffer, wstart, wend-wstart);
}


/*
 * parse a complete command that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_complete_command(struct token_s *tok)
{
    /* 
     * a complete commmand is a list, followed by optional & or ; operators
     * and/or newlines.
     */
    struct node_s *node = parse_list(tok);
    if(!node)
    {
        return NULL;
    }
    tok = get_current_token();
    /* skip & and ; operators */
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
    {
        tok = tokenize(tok->src);
    }
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok = tokenize(tok->src);
    }
    /* return the parsed nodetree */
    return node;
}

/*
 * parse a command list that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_list(struct token_s *tok)
{
    int wstart = tok->src->wstart;
    char *cmdline;
    /* a list consists of one or more AND-OR lists, separated by & or ; operators */
    struct node_s *node = parse_and_or(tok);
    if(!node)
    {
        return NULL;
    }
    tok = get_current_token();

    int type = tok->type;
    tok->src->wstart = tok->src->curpos;
    /* we have & or ; operator. skip and parse the next list */
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
    {
        tok = tokenize(tok->src);
        tok->src->wstart++;
    }
    else
    {
        /* list not ending in & or ;. retutn the list */
        return node;
    }
    /* reached EOF? */
    if(tok->type == TOKEN_EOF)
    {
        return node;
    }
    /* error getting the next token. abort parsing */
    if(tok->type == TOKEN_ERROR)
    {
        free_node_tree(node);
        return NULL;
    }
    /* create a new node for the list */
    struct node_s *list = new_node(NODE_LIST);
    if(!list)
    {
        free_node_tree(node);
        return NULL;
    }
    /* remember whether its a sequential (ending in ;) or asynchronous (ending in &) list */
    set_node_val_chr(list, type == TOKEN_AND ? '&' : ';');
    add_child_node(list, node);
    list->lineno = node->lineno;
    /***************************************
     * NOTE: this is a heuristic, not part of 
     *       the POSIX Shell Grammar.
     *       is it CORRECT???
     ***************************************/
    if(tok->type == TOKEN_NEWLINE)
    {
        cmdline = get_cmdwords(tok, wstart);
        set_node_val_str(list, cmdline);
        free_malloced_str(cmdline);
        /* skip optional newlines */
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        {
            tok = tokenize(tok->src);
        }
        return list;
    }
    /* parse the second list */
    node = parse_list(tok);
    if(node)
    {
        add_child_node(list, node);
    }
    /* save the command line */
    cmdline = get_cmdwords(tok, wstart);
    set_node_val_str(list, cmdline);
    free_malloced_str(cmdline);
    /* return the list */
    return list;
}


/*
 * parse an AND-OR list that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_and_or(struct token_s *tok)
{
    int wstart = tok->src->wstart;
    /*
     * an AND-OR list consists of one or more pipelines, joined by && or
     * || operators.
     */
    struct node_s *and_or = NULL;
    struct node_s *node   = parse_pipeline(tok);
    enum token_type last_type = 0;
loop:
    if(!node)
    {
        /* return the AND-OR list we've got so far */
        if(and_or)
        {
            return and_or;
        }
        /* error parsing the first pipeline. abort parsing */
        return NULL;
    }
    tok = get_current_token();
    int type = tok->type;
    if(tok->type == TOKEN_AND_IF || tok->type == TOKEN_OR_IF)
    {
        /* we have && or ||. skip and continue parsing the next pipeline */
        tok->src->wstart = tok->src->curpos+1;
        tok = tokenize(tok->src);
        /* skip optional newlines */
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        {
            tok = tokenize(tok->src);
        }
    }
    else
    {
        /* we don't have && or ||. finish parsing the list */
        if(and_or)
        {
            struct node_s *child = new_node(last_type == TOKEN_AND_IF ? 
                                            NODE_AND_IF : NODE_OR_IF);
            if(!child)
            {
                free_node_tree(and_or);
                return NULL;
            }
            add_child_node(child , node );
            add_child_node(and_or, child);
            char *cmdline = get_cmdwords(tok, wstart);
            set_node_val_str(and_or, cmdline);
            free_malloced_str(cmdline);
            return and_or;
        }
        return node;
    }
    /* reached EOF or error getting the next token */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
        if(!and_or)
        {
            if(node)
            {
                /* free the partially parsed nodetree */
                free_node_tree(node);
            }
        }
        else
        {
            /* free the partially parsed nodetree */
            free_node_tree(and_or);
        }
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* first child ever */
    if(!and_or)
    {
        /* create a new node for the AND-OR list */
        and_or = new_node(NODE_ANDOR);
        if(!and_or)     /* insufficient memory */
        {
            free_node_tree(node);
            return NULL;
        }
        /* add the pipeline to the AND-OR list */
        add_child_node(and_or, node);
        and_or->lineno = node->lineno;
    }
    else
    {
        /* second and subsequent pipelines in the AND-OR list */
        struct node_s *child = new_node(last_type == TOKEN_AND_IF ? 
                                        NODE_AND_IF : NODE_OR_IF);
        if(!child)      /* insufficient memory */
        {
            free_node_tree(and_or);
            free_node_tree(node  );
            return NULL;
        }
        /* add the pipeline to the AND-OR list */
        add_child_node(child , node );
        add_child_node(and_or, child);
        child->lineno = node->lineno;
    }
    last_type = type;
    /* parse the next pipeline */
    node = parse_pipeline(tok);
    goto loop;
}


/*
 * parse a pipeline that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_pipeline(struct token_s *tok)
{
    int has_bang = 0;
    /* pipeline starts with a bang '!' keyword */
    if(tok->type == TOKEN_KEYWORD_BANG)
    {
        /* remember that and skip the bang */
        has_bang = 1;
        tok = tokenize(tok->src);
    }
    /*
     * a pipeline consists of a pipe sequence, optionally preceded by a bang.
     * parse the pipe sequence.
     */
    struct node_s *node = parse_pipe_sequence(tok);
    if(!node)
    {
        return NULL;
    }
    /* pipeline has a bang */
    if(has_bang)
    {
        struct node_s *bang = new_node(NODE_BANG);
        if(!bang)       /* insufficient memory */
        {
            free_node_tree(node);
            return NULL;
        }
        add_child_node(bang, node);
        bang->lineno = node->lineno;
        node = bang;
    }
    /* return the pipeline nodetree */
    return node;
}


/*
 * parse a pipe sequence that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_pipe_sequence(struct token_s *tok)
{
    struct node_s *pipe = NULL;
    struct node_s *node;
    /* save the start of this line */
    int wstart = tok->src->wstart;
    /*
     * pipeline consists of one or more command, separated by the pipe operator '|',
     * or the non-POSIX operator '|&'.. parse the next command.
     */
    while((node = parse_command(tok)))
    {
        /* func definitions are skipped for now */
        if(node == node_func_def)
        {
            tok = get_current_token();
            /* save the start of this line */
            tok->src->wstart = tok->src->curpos;
            /* skip optional newlines */
            while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            {
                tok = tokenize(tok->src);
            }
            continue;
        }
        tok = get_current_token();
        /* we have a pipe operator ('|' or '|&') */
        if(tok->type == TOKEN_PIPE || tok->type == TOKEN_PIPE_AND)
        {
            enum token_type type = tok->type;
            tok->src->wstart = tok->src->curpos;
            tok = tokenize(tok->src);
            /* skip optional newlines */
            while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            {
                tok->src->wstart = tok->src->curpos;
                tok = tokenize(tok->src);
            }
            /* add implicit redirection of '2>&1' if the '|&' pipe operator was used */
            if(type == TOKEN_PIPE_AND)
            {
                struct node_s *io = io_file_node(2, IO_FILE_GREATAND, "1", node->lineno);
                if(io)
                {
                    add_child_node(node, io);
                }
            }
        }
        else
        {
            /* end of the pipe sequence. return the parsed nodetree */
            if(pipe)
            {
                char *cmdline = get_cmdwords(tok, wstart);
                struct node_s *child = pipe->first_child;
                node->next_sibling   = child;
                child->prev_sibling  = node;
                pipe->first_child    = node;
                set_node_val_str(pipe, cmdline);
                free_malloced_str(cmdline);
                return pipe;
            }
            else
            {
                //set_node_val_str(node, cmdline);
                //free_malloced_str(cmdline);
                return node;
            }
        }
        /* reached EOF or error parsing */
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
            /* free partially parsed nodetree */
            free_node_tree(node);
            if(pipe)
            {
                free_node_tree(pipe);
            }
            /* exit in error if non-interactive shell. return NULL if interactive */
            EXIT_IF_NONINTERACTIVE();
            return NULL;
        }
        /* first command in the pipe sequence */
        if(!pipe)
        {
            pipe = new_node(NODE_PIPE);
            if(!pipe)       /* insufficient memory */
            {
                free_node_tree(node);
                return NULL;
            }
            pipe->lineno = node->lineno;
        }
        /* add commands to the pipe sequence in reverse order (last command first) */
        struct node_s *child = pipe->first_child;
        node->next_sibling   = child;
        if(child)
        {
            child->prev_sibling  = node;
        }
        pipe->first_child = node;
    }
    /* failed to parse pipe sequence */
    if(!node)
    {
        if(pipe)
        {
            /* free partially parsed nodetree */
            free_node_tree(pipe);
        }
        return NULL;
    }
    return node;
}


/*
 * skip list separators '&' and ';'.
 * 
 * returns nothing.
 */
void parse_separator(struct token_s *tok)
{
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }
}


/*
 * parse a term (list) that starts with the given token, and stop when we get
 * the stop_at token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_term(struct token_s *tok, enum token_type stop_at)
{
    /* a term consists of one or more AND-OR lists. parse the first list */
    struct node_s *node = parse_and_or(tok);
    if(!node)
    {
        return NULL;
    }
    tok = get_current_token();
    enum token_type type = tok->type;
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI || tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
        /* skip optional newlines */
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        {
            tok->src->wstart = tok->src->curpos;
            tok = tokenize(tok->src);
        }
        /* stop if the current token is the list terminator */
        if(is_token_of_type(tok, stop_at))
        {
            return node;
        }
    }
    else
    {
        return node;
    }
    /* reached EOF */
    if(tok->type == TOKEN_EOF)
    {
        return node;
    }
    /* error getting the next token */
    if(tok->type == TOKEN_ERROR)
    {
        /* free the partially parsed nodetree */
        free_node_tree(node);
        return NULL;
    }
    /* create a new node for the term */
    struct node_s *term = new_node(NODE_TERM);
    if(!term)       /* insufficient memory */
    {
        /* free the partially parsed nodetree */
        free_node_tree(node);
        return NULL;
    }
    /* check how the term is terminated */
    if(type == TOKEN_AND)       /* term ends in '&' */
    {
        set_node_val_chr(term, '&');
    }
    else if(type == TOKEN_SEMI) /* term ends in ';' */
    {
        set_node_val_chr(term, ';');
    }
    else                        /* default case */
    {
        set_node_val_chr(term, ';');
    }
    add_child_node(term, node);
    term->lineno = node->lineno;
    /* return the term nodetree */
    return term;
}


/*
 * parse a compound list that starts with the given token, and stop when we get
 * the stop_at token.. for example, we can call this function to parse a do-done
 * compound list, in which case stop_at will be the 'done' keyword.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_compound_list(struct token_s *tok, enum token_type stop_at)
{
    /* create a new node for the list */
    struct node_s *list = new_node(NODE_LIST);
    if(!list)
    {
        return NULL;
    }
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }
    /* reached EOF or error parsing */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        return NULL;
    }
    /* keep parsing until we get the stop_at token */
    while(!is_token_of_type(tok, stop_at))
    {
        /* parse the next term (list) */
        struct node_s *node = parse_term(tok, stop_at);
        if(!node)       /* parsing error or input finished */
        {
            struct token_s *tok2 = get_previous_token();
            if(tok2 && is_token_of_type(tok2, stop_at))
            {
                /* input finished. return the parsed list */
                if(list->children)
                {
                    return list;
                }
            }
            /* parsing error. free partially parsed nodetree and return NULL */
            if(list->children == 0)
            {
                free_node_tree(list);
            }
            return NULL;
        }
        /* add parsed term to the list */
        add_child_node(list, node);
        tok = get_current_token();
        parse_separator(tok);
        /* parse optional separators */
        tok = get_current_token();
        /* make sure we haven't reached EOF or an error token */
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            break;
        }
    }
    /* return the parsed nodetree */
    return list;
}


/*
 * parse a subshell that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_subshell(struct token_s *tok)
{
    /* go past '(' */
    tok = tokenize(tok->src);
    /* create a new node for the subshell */
    struct node_s *shell = new_node(NODE_SUBSHELL);
    if(!shell)
    {
        return NULL;
    }
    /* parse the compound list that forms the subshell's body */
    struct node_s *node = parse_compound_list(tok, TOKEN_CLOSEBRACE);
    tok = get_current_token();
    /* current token should be a closing brace ')' */
    if(tok->type != TOKEN_CLOSEBRACE)
    {
        /* missing ')' */
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_CLOSEBRACE);
        free_node_tree(shell);
        free_node_tree(node);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* skip the ')' */
    tok = tokenize(tok->src);
    add_child_node(shell, node);
    shell->lineno = node->lineno;
    /* return the parsed nodetree */
    return shell;
}


/*
 * get the word list that we use in for and select loops, and case conditionals.
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
 * parse a do group that starts with the 'do' keyword, contains a compound
 * list, and ends with the 'done' keyword.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_do_group(struct token_s *tok)
{
    /* first token must be 'do' */
    if(tok->type != TOKEN_KEYWORD_DO)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_DO);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->src->wstart = tok->src->curpos;
    tok = tokenize(tok->src);
    /* parse the compound list */
    struct node_s *_do = parse_compound_list(tok, TOKEN_KEYWORD_DONE);
    tok = get_current_token();
    /* last token must be 'done' */
    if(tok->type != TOKEN_KEYWORD_DONE)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_DONE);
        /* free the partially parsed nodetree */
        free_node_tree(_do);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* skip the 'done' keyword */
    tok = tokenize(tok->src);
    /* return the parsed nodetree */
    return _do;
}


/* 
 * parse the brace group (commands enclosed in curly brackets).
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_brace_group(struct token_s *tok)
{
    /* go past '{' */
    tok = tokenize(tok->src);
    /* parse the compound list inside the braces */
    struct node_s *node = parse_compound_list(tok, TOKEN_KEYWORD_RBRACE);
    struct token_s *tok2;
    tok = get_current_token();
    /*
     * if we have a nested function that ends right before the current one, i.e. something like:
     *   f1 () {
     *     f2 () {
     *        ...
     *     }
     *   }
     * the second closing brace would have been consumed in do_command(), this is why we need
     * to check the previous token if the current token is not a TOKEN_KEYWORD_RBRACE.
     */
    if(tok->type != TOKEN_KEYWORD_RBRACE && (tok2 = get_previous_token()) && tok2->type != TOKEN_KEYWORD_RBRACE)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_RBRACE);
        free_node_tree(node);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* make sure the brace group ends with a closing right brace */
    if(tok->type == TOKEN_KEYWORD_RBRACE)
    {
        /* skip he '}' */
        tok = tokenize(tok->src);
    }
    /* return the parsed nodetree */
    return node;
}


/* 
 * parse the compound command that starts with the given token by calling the
 * appropriate delegate function to handle parsing the compound.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_compound_command(struct token_s *tok)
{
    switch(tok->type)
    {
        case TOKEN_KEYWORD_LBRACE: return parse_brace_group  (tok);
        case TOKEN_OPENBRACE     : return parse_subshell     (tok);
        case TOKEN_KEYWORD_FOR   : return parse_for_clause   (tok);
        case TOKEN_KEYWORD_SELECT: return parse_select_clause(tok);
        case TOKEN_KEYWORD_CASE  : return parse_case_clause  (tok);
        case TOKEN_KEYWORD_IF    : return parse_if_clause    (tok);
        case TOKEN_KEYWORD_WHILE : return parse_while_clause (tok);
        case TOKEN_KEYWORD_UNTIL : return parse_until_clause (tok);
        default                  : return NULL;
    }
}


/*
 * check if the given token is a compound word (one that introduces a compound
 * command, such as for, case, if, ...).
 * 
 * returns 1 if tok is a compound word, 0 otherwise.
 */
static inline int is_compound_keyword(struct token_s *tok)
{
    /* POSIX-defined compound words */
    if((tok->type == TOKEN_KEYWORD_LBRACE) ||       /* '{' */
       (tok->type == TOKEN_OPENBRACE     ) ||       /* '(' */
       (tok->type == TOKEN_KEYWORD_FOR   ) ||       /* 'for' */
       (tok->type == TOKEN_KEYWORD_CASE  ) ||       /* 'case' */
       (tok->type == TOKEN_KEYWORD_IF    ) ||       /* 'if' */
       (tok->type == TOKEN_KEYWORD_WHILE ) ||       /* 'while' */
       (tok->type == TOKEN_KEYWORD_UNTIL ))         /* 'until' */
    {
        return 1;
    }
    /* non-POSIX compound word (only identified if POSIX mode is off) */
    if((tok->type == TOKEN_KEYWORD_SELECT) && !option_set('P'))
    {
        return 1;
    }
    /* token is not a compound word */
    return 0;
}


/* 
 * parse the function body that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_function_body(struct token_s *tok)
{
    /* parse function body (which is just a compound command */
    struct node_s *compound = parse_compound_command(tok);
    /* error parsing or empty/missing function body */
    if(!compound)
    {
        return NULL;
    }
    tok = get_current_token();
    /* parse optional redirection list */
    struct node_s *redirect = parse_redirect_list(tok);
    if(redirect)
    {
        add_child_node(compound, redirect);
    }
    /* return the parsed nodetree */
    return compound;
}


/*
 * parse the function definition that starts with the given token.
 * the 'using_keyword' flag tells us if the 'function' keyword was used in 
 * defining this function, such as:
 *       funcion name { ... }
 * 
 * instead of:
 *       name() { ... }
 * 
 * in the former case, the braces are optional, while they are mandatory
 * in the latter case. the former is an extension, while the latter is POSIX.
 * 
 * returns the special node_func_def node to indicate the function body was
 * successfully parsed and added to the global functions table, or NULL on errors.
 */
struct node_s *parse_function_definition(struct token_s *tok, int using_keyword)
{
    enum token_type err_tok;
    /* special builtin utility names cannot be used as function names */
    if(is_special_builtin(tok->text))
    {
        PARSER_RAISE_ERROR_DESC(INVALID_FUNC_NAME, tok, tok->text);
        set_exit_status(1, 0);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* add the function name to the functions table */
    struct symtab_entry_s *func = add_func(tok->text);
    if(!func)
    {
        return NULL;
    }
    tok = get_current_token();
    int wstart = tok->src->curpos;
    /* POSIX-style of function definitions */
    if(tok->type == TOKEN_OPENBRACE)
    {
        /* skip the '(' */
        tok = tokenize(tok->src);
        /* '(' should be followed by ')' */
        if(tok->type != TOKEN_CLOSEBRACE)
        {
            /* missing ')' */
            err_tok = TOKEN_CLOSEBRACE;
            goto err;
        }
        /* skip the ')' */
        tok = tokenize(tok->src);
    }
    /* if not using the function keyword, a missing '(' is an error */
    else if(!using_keyword)
    {
        err_tok = TOKEN_OPENBRACE;
        goto err;
    }
    /* skip optional newlines */
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok = tokenize(tok->src);
    }
    /* parse the function body */
    struct node_s *body = parse_function_body(tok);
    if(body)
    {
        func->func_body = body;
    }
    func->val_type = SYM_FUNC;
    tok = get_current_token();
    /*
     * our offset is relative to the start of the function, not the whole script. this will not
     * help us in getting the command string of this function (everything between the opening and
     * closing brace. we need to manipulate the offsets in order to get the correct offset for
     * each command parsed as part of the function definition.
     */
    int l = tok->linestart, c = tok->charno;
    tok->linestart = tok->src->curpos; tok->charno = 0;
    char *cmdline = get_cmdwords(tok, wstart);
    tok->linestart = l; tok->charno = c;
    symtab_entry_setval(func, cmdline);
    free_malloced_str(cmdline);
    set_exit_status(0, 0);
    /*
     * we are not going to execute the function body right now, so return a dummy nodetree
     * to let the caller know we parsed the function definition successfully.
     */
    return node_func_def;
    
err:
    PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, err_tok);
    set_exit_status(1, 0);
    rem_from_symtab(func);
    EXIT_IF_NONINTERACTIVE();
    return NULL;
}


/* 
 * parse the simple command that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_simple_command(struct token_s *tok)
{
    /* create a new node for the command */
    struct node_s *cmd = new_node(NODE_COMMAND);
    if(!cmd)
    {
        return NULL;
    }
    cmd->lineno = tok->lineno;
    /*
     * flag to indicate if we need to check the following word for alias substitution.
     */
    int alias_next_word = 0;
    /*
     *  parse the command prefix.. command prefixes can contain I/O redirections
     *  and assignment words.
     */
    //int has_prefix = 0;
    /* do we have a command prefix? */
    if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER || tok->type == TOKEN_ASSIGNMENT_WORD)
    {
        //has_prefix = 1;
        //tok = get_current_token();
        /* parse all the redirections and assignments in the command prefix */
        while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
        {
            if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
            {
                struct node_s *redirect = parse_io_redirect(tok);
                if(redirect)
                {
                    add_child_node(cmd, redirect);
                }
                /* stop parsing on redirection errors */
                if(parser_err)
                {
                    free_node_tree(cmd);
                    return NULL;
                }
            }
            else if(tok->type == TOKEN_ASSIGNMENT_WORD)
            {
                /* add the assignment word to the command */
                struct node_s *assign = new_node(NODE_ASSIGNMENT);
                if(assign)
                {
                    set_node_val_str(assign, tok->text);
                    assign->lineno = tok->lineno;
                    add_child_node(cmd, assign);
                }
            }
            else
            {
                break;
            }
            tok = tokenize(tok->src);
        }
    }
    /* parse the command word proper */
    if(tok->type != TOKEN_WORD)
    {
        /* no command word */
        if(!cmd->children)
        {
            /* empty command */
            free(cmd);
            cmd = NULL;
        }
        else
        {
            char *cmdline = get_cmdwords(tok, tok->src->wstart);
            set_node_val_str(cmd, cmdline);   /* get the command line */
            free_malloced_str(cmdline);
        }
        /* return the parsed command prefix */
        return cmd;
    }
    /* create a new node for the command word */
    struct node_s *word = new_node(NODE_VAR);
    if(!word)
    {
        /* free the partially parsed nodetree */
        free_node_tree(cmd);
        return NULL;
    }
    /* check the command word for aliases */
    if(optionx_set(OPTION_EXPAND_ALIASES))
    {
        /* check first word for alias substitution */
        check_word_for_alias_substitution(tok);
        /* check if we need to alias-substitute the following word */
        if(tok->text[tok->text_len-1] == ' ')
        {
            alias_next_word = 1;
        }
    }
    set_node_val_str(word, tok->text);
    word->lineno = tok->lineno;
    /* is this a test or '[[' command? */
    add_child_node(cmd, word);
    int istest = (strcmp(tok->text, "[[") == 0) ? 2 :
                 (strcmp(tok->text, "[" ) == 0) ? 1 : 0;
    /*
    if(has_prefix)
    {
        tok = tokenize(tok->src);
    }
    else
    {
        tok = get_current_token();
    }
    */
    tok = tokenize(tok->src);
    /* reached EOF */
    if(tok->type == TOKEN_EOF)
    {
        return cmd;
    }
    /* error getting next token */
    if(tok->type == TOKEN_ERROR)
    {
        /* free the partially parsed nodetree */
        free_node_tree(cmd);
        return NULL;
    }
    /* parse the command suffix */
    struct node_s *last = last_child(cmd);
    do
    {
        /* parse all the redirections and assignments in the command suffix */
        if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
        {
            struct node_s *redirect = parse_io_redirect(tok);
            if(redirect)
            {
                /*
                 * check for the non-POSIX bash redirection extensions of {var}<&N and
                 * {var}>&N. the {var} part would have been added as the previous node.
                 */
                if(last && last->type == NODE_VAR)
                {
                    char *s = last->val.str;
                    /* must begin with '{' and end with '}' */
                    if(s[0] == '{' && s[strlen(s)-1] == '}')
                    {
                        set_node_val_str(redirect, s);
                        redirect->next_sibling = last->next_sibling;
                        redirect->prev_sibling = last->prev_sibling;
                        if(cmd->first_child == last)
                        {
                            cmd->first_child = redirect;
                        }
                        else
                        {
                            redirect->prev_sibling->next_sibling = redirect;
                        }
                        free_node_tree(last);
                    }
                    else
                    {
                        add_child_node(cmd, redirect);
                    }
                }
                else
                {
                    add_child_node(cmd, redirect);
                }
                last = redirect;
            }
            else
            {
                /* stop parsing on redirection errors */
                if(parser_err)
                {
                    free_node_tree(cmd);
                    return NULL;
                }
            }
            tok = get_current_token();
            continue;
        }
        /* we have a separator token */
        else if(is_separator_tok(tok->type))
        {
            /* finish parsing the command, unless its a 'test' command */
            if(!istest)
            {
                break;
            }
            /* the test command accepts !, &&, ||, (, ) */
            if(tok->type != TOKEN_KEYWORD_BANG && tok->type != TOKEN_AND_IF && 
               tok->type != TOKEN_OR_IF && tok->type != TOKEN_CLOSEBRACE &&
               tok->type != TOKEN_OPENBRACE)
            {
                break;
            }
        }
        /*
         * variable assignments in command suffixes are not POSIX-defined,
         * but most shells accept them (in place of the obsolete -k option),
         * and so do we.
         */
        else if(tok->type == TOKEN_ASSIGNMENT_WORD)
        {
            /* add the assignment word to the command */
            struct node_s *assign = new_node(NODE_ASSIGNMENT);
            if(assign)
            {
                set_node_val_str(assign, tok->text);
                assign->lineno = tok->lineno;
                add_child_node(cmd, assign);
                last = assign;
            }
            tok = tokenize(tok->src);
            continue;
        }
        /* normal word (command argument) */
        struct node_s *word = new_node(NODE_VAR);
        if(!word)
        {
            /* free the partially parsed nodetree */
            free_node_tree(cmd);
            return NULL;
        }
        /* check the command word for aliases */
        if(alias_next_word)
        {
            /* check first word for alias substitution */
            check_word_for_alias_substitution(tok);
            /* check if we need to alias-substitute the following word */
            if(tok->text[tok->text_len-1] == ' ')
            {
                alias_next_word = 1;
            }
            else
            {
                alias_next_word = 0;
            }
        }
        set_node_val_str(word, tok->text);
        word->lineno = tok->lineno;
        add_child_node(cmd, word);
        last = word;
        tok = tokenize(tok->src);
        /*
         * test command, when invoked as '[' or '[[', must end with a matching ']' or ']]'.
         * we test this in order not to overshoot while reading tokens, so that commands like
         * this:
         *    [ -f $X ] && echo "X is a file" || echo "X is not a file"
         * will work.
         */
        if(istest == 2 && strcmp(word->val.str, "]]") == 0)
        {
            break;
        }
        else if(istest == 1 && strcmp(word->val.str, "]") == 0)
        {
            break;
        }
    } while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR);
    /* check if we have successfully parsed the command */
    if(cmd)
    {
        char *cmdline = get_cmdwords(tok, tok->src->wstart);
        set_node_val_str(cmd, cmdline);   /* get the command line */
        free_malloced_str(cmdline);
    }
    /* return the parsed nodetree */
    return cmd;
}


/* 
 * parse the simple or compound command that starts with the given token by
 * calling the appropriate delegate function to handle parsing the compound.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_command(struct token_s *tok)
{
    /* skip newline and ; operators */
    while(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
    {
        /* save the start of this line */
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(src);
    }
    /* reached EOF or error getting the next token */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        return NULL;
    }
    /* we have a compound command */
    if(is_compound_keyword(tok))
    {
        /* parse it */
        struct node_s *compound = parse_compound_command(tok);
        if(!compound)
        {
            return NULL;
        }
        tok = get_current_token();
        /* parse the optional redirection list */
        struct node_s *redirect = parse_redirect_list(tok);
        if(redirect)
        {
            add_child_node(compound, redirect);
        }
        /* return the parsed nodetree */
        return compound;
    }
    /* we have a command preceded by the 'time' special keyword */
    else if(tok->type == TOKEN_KEYWORD_TIME)
    {
        /* create a special node for the timed command */
        struct node_s *t = new_node(NODE_TIME);
        if(!t)
        {
            return NULL;
        }
        tok->src->wstart = tok->src->curpos;
        /* skip the time keyword */
        tok = tokenize(tok->src);
        /*
         * if nothing comes after the 'time' keyword, return it as the backend will still
         * execute the time command.
         */
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            return t;
        }
        /* parse the timed command */
        struct node_s *cmd = parse_command(tok);
        if(cmd)
        {
            add_child_node(t, cmd);
        }
        /* return the parsed nodetree */
        return t;
    }
    /* get a duplicate of the current token */
    struct token_s *tok2 = dup_token(tok);
    if(!tok2)
    {
        return NULL;
    }

    /* save a temporary copy of our source struct's pointers */
    struct source_s src2;
    src2.curline = tok->src->curline;
    src2.curchar = tok->src->curchar;
    src2.curpos = tok->src->curpos;
    src2.curlinestart = tok->src->curlinestart;
    src2.wstart = tok->src->wstart;

    /* skip the current token so we can check the token after it */
    tok = tokenize(tok->src);
    /* if we reached EOF, we should get an EOF token, not an ERROR token */
    if(tok->type == TOKEN_ERROR)
    {
        free_token(tok2);
        return NULL;
    }
    /* 
     * alternative, non-POSIX function definition using the
     * 'function' keyword.
     */
    else if(tok2->type == TOKEN_KEYWORD_FUNCTION && !option_set('P'))
    {
        /* get rid of the (optional) opening brace */
        free(tok2);
        tok2 = dup_token(tok);
        if(!tok2)
        {
            return NULL;
        }
        int l = tok->src->curline, c = tok->src->curchar;
        tok2->src->curline = 1;
        tok2->src->curchar = 1;
        tok = tokenize(tok->src);
        /* parse the function definition */
        struct node_s *func = parse_function_definition(tok2, 1);
        tok->src->curline += l;
        tok->src->curchar += c;
        free_token(tok2);
        /* return the parsed nodetree */
        return func;
    }
    /* POSIX way of defining functions */
    else if(is_name(tok2->text) && !is_special_builtin(tok2->text) &&
       tok->type != TOKEN_EOF && tok->type == TOKEN_OPENBRACE)
    {
        int l = tok->src->curline, c = tok->src->curchar;
        tok->src->curline = 1;
        tok->src->curchar = 1;
        /* parse the function definition */
        struct node_s *func = parse_function_definition(tok2, 0);
        tok->src->curline += l;
        tok->src->curchar += c;
        free_token(tok2);
        /* return the parsed nodetree */
        return func;
    }
    /* we have a simple command */
    /*
     *  as we have read a token ahead, return the source pointers to the beginning
     *  of the first token, so that parse_simple_command() will parse correctly.
     */
    tok->src->curline = src2.curline;
    tok->src->curchar = src2.curchar;
    tok->src->curpos = src2.curpos;
    tok->src->curlinestart = src2.curlinestart;
    tok->src->wstart = src2.wstart;
    /* parse the simple command */
    struct node_s *cmd = parse_simple_command(tok2);
    free_token(tok2);
    /* return the parsed nodetree */
    return cmd;
}


/* 
 * parse the complete translation unit, command by command.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_translation_unit()
{
    /* skip any leading whitespace chars */
    skip_white_spaces(src);
    /* save the start of this line */
    src->wstart = src->curpos;
    /*
     * the -n option means read commands but don't execute them.
     * only effective in non-interactive shells (POSIX says 
     * interactive shells may safely ignore it). this option
     * is good for checking a script for syntax errors.
     */
    int noexec = (option_set('n') && !option_set('i'));
    int i      = src->curpos;
    struct token_s  *tok  = tokenize(src);
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
    {
        /* skip comments and newlines */
        if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
        {
            i = src->curpos;
            /* save the start of this line */
            src->wstart = src->curpos;
            tok = tokenize(tok->src);
        }
        else
        {
            break;
        }
    }
    /* reached EOF or error getting next token */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        return NULL;
    }
    /* first time ever? keep a track of the first char of the current command line */
    if(i < 0)
    {
        i = 0;
    }
    /* create a new node for the translation unit */
    struct node_s *root = new_node(NODE_PROGRAM);
    int save_hist = option_set('i') && src->srctype == SOURCE_STDIN;
    parser_err = 0;
    /* loop parsing commands */
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
    {
        /* parse the next command */
        struct node_s *cmd = parse_complete_command(tok);
        tok = get_current_token();
        if(!cmd)
        {
            break;
        }
        if(parser_err)
        {
            free_node_tree(cmd);
            break;
        }
        /* are we going to execute this command? */
        if(!noexec)
        {
            if(!cmd->lineno)
            {
                cmd->lineno = src->curline;
            }
            add_child_node(root, cmd);
            /* add command to the history list and echo it (if -v option is set) */
            switch(cmd->type)
            {
                case NODE_TIME:
                    /* the real command is the first child of the 'time' node */
                    if(cmd->first_child)
                    {
                        cmd = cmd->first_child;
                        /* fall through to the next case */
                    }
                    else
                    {
                        /* 'time' word with no timed command */
                        if(save_hist)
                        {
                            save_to_history("time");
                        }
                        if(option_set('v'))
                        {
                            fprintf(stderr, "time\n");
                        }
                        break;
                    }
                    /* fall through to the next case */
                    __attribute__((fallthrough));
                    
                case NODE_COMMAND:
                case NODE_LIST:
                    if(cmd->val.str)
                    {
                        if(save_hist)
                        {
                            save_to_history(cmd->val.str);
                        }
                        if(option_set('v'))
                        {
                            fprintf(stderr, "%s\n", cmd->val.str);
                        }
                        break;
                    }
                    /* fall through to the next case */
                    __attribute__((fallthrough));
                    
                default:
                    ;
                    int j = src->curpos-(tok->text_len);
                    while(src->buffer[j] == '\n')
                    {
                        j--;
                    }
                    /* copy command line to buffer */
                    char buf[j-i+1];
                    int k = 0;
                    do
                    {
                        buf[k++] = src->buffer[i++];
                    } while(i < j);
                    buf[k] = '\0';
                    if(save_hist)
                    {
                        save_to_history(buf);
                    }
                    if(option_set('v'))
                    {
                        fprintf(stderr, "%s\n", buf);
                    }
                    break;
            }
        }
        /* skip optional newline and comment tokens */
        while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
        {
            if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
            {
                tok = tokenize(tok->src);
            }
            else
            {
                break;
            }
        }
        /* reached EOF or error getting next token */
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            break;
        }
        /* keep a track of the first char of the current command line */
        i = src->curpos-(tok->text_len);
        while(src->buffer[i] == '\n')
        {
            i++;
        }
        /* save the start of this line */
        src->wstart = src->curpos-(tok->text_len);
    }
    /* nothing useful found */
    if(!root->children)
    {
        free_node_tree(root);
        root = NULL;
    }
    /* return the parsed nodetree */
    return root;
}
