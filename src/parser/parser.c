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
#include "node.h"
#include "parser.h"
#include "../error/error.h"
#include "../debug.h"

/*********************************************/
/* top-down, recursive descent syntax parser */
/*********************************************/

struct symtab_entry_s *current_func = (struct symtab_entry_s *)NULL;
/* dummy struct to indicate func definitions in src */
struct node_s __node_func_def;
struct node_s *node_func_def = &__node_func_def;
int parser_err = 0;

/* defined in ../cmdline.c */
extern char *stdin_filename;

/*
 * return a ready-made I/O redirection node. useful when parsing non-POSIX
 * operators, such as '|&', which equates to '2>&1 |'. in this case, the pipe
 * is handled normally, but the implicit redirection needs an additional node,
 * which this function provides.
 * arguments:
 *   fd      : file descriptor of redirected file (0, 1, 2, ...).
 *   type    : node type, such as IO_FILE_GREAT, IO_FILE_LESSGREAT, ...
 *   namestr : string containing the part following the redirection operator,
 *             i.e. file path or file descriptor.
 *   lineno  : the source file line number to assign to the new node.
 */
struct node_s *io_file_node(int fd, char type, char *namestr, int lineno)
{
    struct node_s *io = new_node(NODE_IO_REDIRECT);
    if(!io) return NULL;
    io->lineno = lineno;
    set_node_val_sint(io, fd);
    struct node_s *file = new_node(NODE_IO_FILE);
    if(!file)
    {
        free_node_tree(io);
        return NULL;
    }
    file->lineno = lineno;
    set_node_val_chr(file, type);
    add_child_node(io, file);
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        free_node_tree(io);
        return NULL;
    }
    set_node_val_str(name, namestr);
    name->lineno = lineno;
    add_child_node(file, name);
    return io;
}


char *get_cmdwords(struct token_s *tok, int wstart)
{
    if(!tok || !tok->src) return NULL;
    struct source_s *src = tok->src;
    //int wstart = src->wstart;
    //int wend   = src->curpos;
    int wend   = tok->linestart+tok->charno-tok->text_len;
    /* at the very first word we would have a -ve position */
    if(wstart < 0) wstart = 0;
    while(isspace(src->buffer[wstart])) wstart++;
    while(isspace(src->buffer[wend  ])) wend--  ;
    if(!isspace(src->buffer[wend])) wend++;
    if(wstart >= wend) return NULL;
    return get_malloced_strl(src->buffer, wstart, wend-wstart);
}


struct node_s *parse_complete_command(struct token_s *tok)
{
    struct node_s *node = parse_list(tok);
    if(!node) return NULL;
    tok = get_current_token();
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
        tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    return node;
}

struct node_s *parse_list(struct token_s *tok)
{
    int wstart = tok->src->wstart;
    char *cmdline;
    struct node_s *node = parse_and_or(tok);
    if(!node) return NULL;
    tok = get_current_token();

    int type = tok->type;
    tok->src->wstart = tok->src->curpos;
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
    {
        tok = tokenize(tok->src);
        tok->src->wstart++;
    }
    else return node;

    if(tok->type == TOKEN_EOF) return node;
    if(tok->type == TOKEN_ERROR)
    {
        free_node_tree(node);
        return NULL;
    }
    struct node_s *list = new_node(NODE_LIST);
    if(!list)
    {
        free_node_tree(node);
        return NULL;
    }
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
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            tok = tokenize(tok->src);
        return list;
    }
    node = parse_list(tok);
    if(node) add_child_node(list, node);
    cmdline = get_cmdwords(tok, wstart);
    set_node_val_str(list, cmdline);
    free_malloced_str(cmdline);
    return list;
}

struct node_s *parse_and_or(struct token_s *tok)
{
    int wstart = tok->src->wstart;
    struct node_s *and_or = NULL;
    struct node_s *node   = parse_pipeline(tok);
    enum token_type last_type = 0;
loop:
    if(!node)
    {
        if(and_or) return and_or;
        return NULL;
    }
    tok = get_current_token();
    int type = tok->type;
    if(tok->type == TOKEN_AND_IF || tok->type == TOKEN_OR_IF)
    {
        tok->src->wstart = tok->src->curpos+1;
        tok = tokenize(tok->src);
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            tok = tokenize(tok->src);
    }
    else
    {
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
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        /*
        fprintf(stderr, "shell: missing pipeline after %s\n",
                get_token_description(tok->type));
        */
        PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
        if(!and_or)
        {
            if(node) free_node_tree(node);
        }
        else free_node_tree(and_or);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    /* first child ever */
    if(!and_or)
    {
        and_or = new_node(NODE_ANDOR);
        if(!and_or)
        {
            free_node_tree(node);
            return NULL;
        }
        add_child_node(and_or, node);
        and_or->lineno = node->lineno;
    }
    else
    {
        struct node_s *child = new_node(last_type == TOKEN_AND_IF ? 
                                        NODE_AND_IF : NODE_OR_IF);
        if(!child)
        {
            free_node_tree(and_or);
            free_node_tree(node  );
            return NULL;
        }
        add_child_node(child , node );
        add_child_node(and_or, child);
        child->lineno = node->lineno;
    }
    last_type = type;
    node   = parse_pipeline(tok);
    goto loop;
}

struct node_s *parse_pipeline(struct token_s *tok)
{
    int has_bang = 0;
    if(tok->type == TOKEN_KEYWORD_BANG)
    {
        has_bang = 1;
        tok = tokenize(tok->src);
    }
    struct node_s *node = parse_pipe_sequence(tok);
    if(!node) return NULL;
    if(has_bang)
    {
        struct node_s *bang = new_node(NODE_BANG);
        if(!bang)
        {
            free_node_tree(node);
            return NULL;
        }
        add_child_node(bang, node);
        bang->lineno = node->lineno;
        node = bang;
    }
    return node;
}

struct node_s *parse_pipe_sequence(struct token_s *tok)
{
    struct node_s *pipe = NULL;
    struct node_s *node;
    /* save the start of this line */
    //tok->src->wstart = tok->src->curpos;
    int wstart = tok->src->wstart;
loop:
    node = parse_command(tok);
    /* func definitions are skipped for now */
    if(node == node_func_def)
    {
        tok = get_current_token();
        /* save the start of this line */
        tok->src->wstart = tok->src->curpos;
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            tok = tokenize(tok->src);
        goto loop;
    }
    /* failed to parse pipe sequence */
    if(!node)
    {
        if(pipe) free_node_tree(pipe);
        return NULL;
    }
    tok = get_current_token();
    if(tok->type == TOKEN_PIPE || tok->type == TOKEN_PIPE_AND)
    {
        enum token_type type = tok->type;
        tok = tokenize(tok->src);
        tok->src->wstart = tok->src->curpos;
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        {
            tok->src->wstart = tok->src->curpos;
            tok = tokenize(tok->src);
        }
        /* add implicit redirection of '2>&1' if the '|&' pipe operator was used */
        if(type == TOKEN_PIPE_AND)
        {
            struct node_s *io = io_file_node(2, IO_FILE_GREATAND, "1", node->lineno);
            if(io) add_child_node(node, io);
        }
    }
    else
    {
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
    
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
        free_node_tree(node);
        if(pipe) free_node_tree(pipe);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    if(!pipe)
    {
        pipe = new_node(NODE_PIPE);
        if(!pipe)
        {
            free_node_tree(node);
            return NULL;
        }
        pipe->lineno = node->lineno;
    }
    struct node_s *child = pipe->first_child;
    node->next_sibling   = child;
    if(child) child->prev_sibling  = node;
    pipe->first_child    = node;
    goto loop;
}

void parse_separator(struct token_s *tok)
{
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }

    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }
}

struct node_s *parse_term(struct token_s *tok, enum token_type stop_at)
{
    struct node_s *node = parse_and_or(tok);
    if(!node) return NULL;
    tok = get_current_token();
    enum token_type type = tok->type;
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI || tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        {
            tok->src->wstart = tok->src->curpos;
            tok = tokenize(tok->src);
        }
        if(is_token_of_type(tok, stop_at)) return node;
    }
    else return node;
    if(tok->type == TOKEN_EOF) return node;
    if(tok->type == TOKEN_ERROR)
    {
        free_node_tree(node);
        return NULL;
    }
    struct node_s *term = new_node(NODE_TERM);
    if(!term)
    {
        free_node_tree(node);
        return NULL;
    }
    if(type == TOKEN_AND)       set_node_val_chr(term, '&');
    else if(type == TOKEN_SEMI) set_node_val_chr(term, ';');
    else                        set_node_val_chr(term, ';');
    add_child_node(term, node);
    term->lineno = node->lineno;
    return term;
}

struct node_s *parse_compound_list(struct token_s *tok, enum token_type stop_at)
{
    struct node_s *list = new_node(NODE_LIST);
    if(!list) return NULL;
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
    }
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) return NULL;
    while(!is_token_of_type(tok, stop_at))
    {
        struct node_s *node = parse_term(tok, stop_at);
        if(!node)
        {
            struct token_s *tok2 = get_previous_token();
            if(tok2 && is_token_of_type(tok2, stop_at))
            {
                if(list->children) return list;
            }
            if(list->children == 0) free_node_tree(list);
            return NULL;
        }
        add_child_node(list, node);
        tok = get_current_token();
        parse_separator(tok);
        tok = get_current_token();
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) break;
    }
    return list;
}

struct node_s *parse_subshell(struct token_s *tok)
{
    /* go past '(' */
    tok = tokenize(tok->src);
    struct node_s *shell = new_node(NODE_SUBSHELL);
    if(!shell) return NULL;
    struct node_s *node = parse_compound_list(tok, TOKEN_CLOSEBRACE);
    tok = get_current_token();
    if(tok->type != TOKEN_CLOSEBRACE)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_CLOSEBRACE);
        free_node_tree(shell);
        free_node_tree(node);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok = tokenize(tok->src);
    add_child_node(shell, node);
    shell->lineno = node->lineno;
    return shell;
}

static inline int is_name(char *str)
{
    /* names start with alpha char or an underscore... */
    if(!isalpha(*str) && *str != '_') return 0;
    /* ...and contain alphanumeric chars and/or underscores */
    while(*++str)
    {
        if(!isalnum(*str) && *str != '_') return 0;
    }
    return 1;
}

struct node_s *get_wordlist(struct token_s *tok)
{
    if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
        return NULL;
    struct node_s *wordlist = new_node(NODE_WORDLIST);
    if(!wordlist) return NULL;
    wordlist->lineno = tok->lineno;
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_WORD)
    {
        struct node_s *word = new_node(NODE_VAR);
        if(!word)
        {
            free_node_tree(wordlist);
            return NULL;
        }
        /* copy the name to the new node */
        set_node_val_str(word, tok->text);
        word->lineno = tok->lineno;
        add_child_node(wordlist, word);
        tok = tokenize(tok->src);
    }
    return wordlist;
}

struct node_s *parse_do_group(struct token_s *tok)
{
    if(tok->type != TOKEN_KEYWORD_DO)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_DO);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->src->wstart = tok->src->curpos;
    tok = tokenize(tok->src);
    struct node_s *_do = parse_compound_list(tok, TOKEN_KEYWORD_DONE);
    tok = get_current_token();
    if(tok->type != TOKEN_KEYWORD_DONE)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_DONE);
        free_node_tree(_do);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok = tokenize(tok->src);
    return _do;
}

struct node_s *parse_for_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'for' */
    tok = tokenize(tok->src);
    if(!is_name(tok->text))
    {
        /* 
         * second form of 'for' loops:
         *    for((expr1; expr2; expr3)); do commands; done
         * this is a non-POSIX extension used by all major shells.
         */
        if(tok->text && tok->text[0] == '(' && tok->text[1] == '(' && !option_set('P'))
        //if(tok->type == TOKEN_OPENBRACE)
            return parse_for_clause2(tok);
        //fprintf(stderr, "shell: expected name after 'for'\n");
        PARSER_RAISE_ERROR_DESC(MISSING_FOR_NAME, tok, NULL);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->type = TOKEN_NAME;
    struct node_s *_for = new_node(NODE_FOR);
    if(!_for) return NULL;
    _for->lineno = lineno;
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        free_node_tree(_for);
        return NULL;
    }
    /* copy the name to the new node */
    set_node_val_str(name, tok->text);
    name->lineno = tok->lineno;
    add_child_node(_for, name);
    tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    /* check for 'in' */
    if(tok->type == TOKEN_KEYWORD_IN)
    {
        tok = tokenize(tok->src);
        struct node_s *wordlist = get_wordlist(tok);
        if(wordlist) add_child_node(_for, wordlist);
        if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
            tok = tokenize(tok->src);
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            tok = tokenize(tok->src);
    }
    struct node_s *parse_group = parse_do_group(tok);
    if(parse_group) add_child_node(_for, parse_group);
    return _for;
}

/* 
 * parse the second form of 'for' loops:
 * 
 *    for((expr1; expr2; expr3)); do commands; done
 * 
 * this is a non-POSIX extension used by all major shells.
 */
struct node_s *parse_for_clause2(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past the first '(' */
    //tok = tokenize(tok->src);
    if(!tok->text || tok->text[0] != '(' || tok->text[1] != '(')
    //if(tok->type != TOKEN_OPENBRACE)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_OPENBRACE);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    //int i = src->curpos+1;
    //char *p = src->buffer+i;
    //char *pend = src->buffer+src->bufsize;
    int i = 2;
    char *p = tok->text+i;
    char *pend = tok->text+tok->text_len;
    char *expr[3] = { NULL, NULL, NULL };
    expr[0] = p;
    /* get expression #1 */
    while(*p && *p != ';') p++;
    if(p >= pend) goto eof;
    *p ='\0';
    expr[0] = get_malloced_str(expr[0]);
    *p++ = ';';
    /* get expression #2 */
    expr[1] = p;
    while(*p && *p != ';') p++;
    if(p >= pend) { free_malloced_str(expr[0]); goto eof; }
    *p ='\0';
    expr[1] = get_malloced_str(expr[1]);
    *p++ = ';';
    /* get expression #3 */
    expr[2] = p;
    while(*p && *p != ')') p++;
    if(p >= pend || p[1] != ')') { free_malloced_str(expr[0]); free_malloced_str(expr[1]); goto eof; }
    *p ='\0';
    expr[2] = get_malloced_str(expr[2]);
    *p++ = ')';
    //src->curpos = p-src->buffer;
    
    /* create the node tree */
    struct node_s *_for = new_node(NODE_FOR);
    if(!_for)
    {
        for(i = 0; i < 3; i++) free_malloced_str(expr[i]);
        return NULL;
    }
    _for->lineno = lineno;
    for(i = 0; i < 3; i++)
    {
        struct node_s *node = new_node(NODE_ARITHMETIC_EXPR);
        if(!node)
        {
            free_node_tree(_for);
            for(i = 0; i < 3; i++) if(expr[i]) free_malloced_str(expr[i]);
            return NULL;
        }
        node->val_type = VAL_STR;
        node->val.str  = expr[i];
        expr[i] = NULL;
        add_child_node(_for, node);
    }
    /* now get the loop body */
    tok = tokenize(tok->src);
    if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
        tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    struct node_s *parse_group = parse_do_group(tok);
    if(parse_group) add_child_node(_for, parse_group);
    return _for;
    
eof:
    PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
    EXIT_IF_NONINTERACTIVE();
    return NULL;
}

struct node_s *parse_select_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'for' */
    tok = tokenize(tok->src);
    if(!is_name(tok->text))
    {
        //fprintf(stderr, "shell: expected name after 'for'\n");
        PARSER_RAISE_ERROR_DESC(MISSING_SELECT_NAME, tok, NULL);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->type = TOKEN_NAME;
    struct node_s *select = new_node(NODE_SELECT);
    if(!select) return NULL;
    select->lineno = lineno;
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        free_node_tree(select);
        return NULL;
    }
    /* copy the name to the new node */
    set_node_val_str(name, tok->text);
    name->lineno = tok->lineno;
    add_child_node(select, name);
    tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    /* check for 'in' */
    if(tok->type == TOKEN_KEYWORD_IN)
    {
        tok = tokenize(tok->src);
        struct node_s *wordlist = get_wordlist(tok);
        if(wordlist) add_child_node(select, wordlist);
        if(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
            tok = tokenize(tok->src);
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
            tok = tokenize(tok->src);
    }
    struct node_s *parse_group = parse_do_group(tok);
    if(parse_group) add_child_node(select, parse_group);
    return select;
}

struct node_s *parse_case_item(struct token_s *tok)
{
    int lineno = tok->lineno;
    if(tok->type == TOKEN_OPENBRACE)
        tok = tokenize(tok->src);
    struct node_s *item = new_node(NODE_CASE_ITEM);
    if(!item) return NULL;
    item->lineno = lineno;
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_CLOSEBRACE)
    {
        struct node_s *word = new_node(NODE_VAR);
        if(!word)
        {
            free_node_tree(item);
            return NULL;
        }
        /* copy the pattern to the new node */
        set_node_val_str(word, tok->text);
        word->lineno = tok->lineno;
        add_child_node(item, word);
        tok = tokenize(tok->src);
        while(tok->type != TOKEN_EOF && tok->type == TOKEN_PIPE)
            tok = tokenize(tok->src);
    }
    tok->src->wstart = tok->src->curpos;
    tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    if(option_set('P'))
    {
        if(!is_token_of_type(tok, TOKEN_DSEMI_ESAC))
        {
            struct node_s *compound = parse_compound_list(tok, TOKEN_DSEMI_ESAC);
            if(compound) add_child_node(item, compound);
        }
    }
    else
    {
        if(!is_token_of_type(tok, TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR))
        {
            struct node_s *compound = parse_compound_list(tok, TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR);
            if(compound) add_child_node(item, compound);
        }
    }
    tok = get_current_token();

    if(tok->type == TOKEN_SEMI_AND) set_node_val_chr(item, '&');
    else if(tok->type == TOKEN_SEMI_OR || tok->type == TOKEN_SEMI_SEMI_AND) set_node_val_chr(item, ';');

    while(tok->type == TOKEN_DSEMI || tok->type == TOKEN_SEMI_AND)
        tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    return item;
}

struct node_s *parse_case_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'case' */
    tok = tokenize(tok->src);
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    struct node_s *_case = new_node(NODE_CASE);
    if(!_case) return NULL;
    _case->lineno = lineno;
    struct node_s *word  = new_node(NODE_VAR );
    if(!word)
    {
        free_node_tree(_case);
        return NULL;
    }
    /* copy the name to the new node */
    set_node_val_str(word, tok->text);
    word->lineno = tok->lineno;
    add_child_node(_case, word);
    tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    /* check for 'in' */
    if(tok->type != TOKEN_KEYWORD_IN)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_IN);
        free_node_tree(_case);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR && tok->type != TOKEN_KEYWORD_ESAC)
    {
        struct node_s *item = parse_case_item(tok);
        if(item) add_child_node(_case, item);
        tok = get_current_token();
    }
    if(tok->type != TOKEN_KEYWORD_ESAC)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_ESAC);
        free_node_tree(_case);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok = tokenize(tok->src);
    return _case;
}

struct node_s *parse_if_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    /* go past 'if' */
    tok = tokenize(tok->src);
    struct node_s *_if      = new_node(NODE_IF);
    if(!_if) return NULL;
    _if->lineno = lineno;
    struct node_s *compound = parse_compound_list(tok, TOKEN_KEYWORD_THEN);
    if(compound) add_child_node(_if, compound);
    tok = get_current_token();
    if(tok->type != TOKEN_KEYWORD_THEN)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_KEYWORD_THEN);
        free_node_tree(_if);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok->src->wstart = tok->src->curpos+1;
    tok = tokenize(tok->src);
    /*
     * TODO: we need to figure out a way to tell parse_compound_list()
     *       to stop at EITHER elif OR else keywords.
     */
    compound = parse_compound_list(tok, TOKEN_KEYWORDS_ELIF_ELSE_FI);
    if(compound && compound->children)
        add_child_node(_if, compound);
    else
    {
        PARSER_RAISE_ERROR_DESC(EXPECTED_TOKEN, tok, "expression");
        free_node_tree(compound);
        free_node_tree(_if);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    tok = get_current_token();
    tok->src->wstart = tok->src->curpos+1;
    if(tok->type == TOKEN_KEYWORD_ELIF)
    {
        compound = parse_if_clause(tok);
        if(compound) add_child_node(_if, compound);
    }
    else if(tok->type == TOKEN_KEYWORD_ELSE)
    {
        tok = tokenize(tok->src);
        compound = parse_compound_list(tok, TOKEN_KEYWORD_FI);
        if(compound) add_child_node(_if, compound);
    }
    tok = get_current_token();
    if(tok->type == TOKEN_KEYWORD_FI)
    {
        tok = tokenize(tok->src);
        return _if;
    }
    /* if we had an 'elif' clause, token 'fi' was consumed
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
    free_node_tree(_if);
    EXIT_IF_NONINTERACTIVE();
    return NULL;
}

struct node_s *parse_while_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    tok->src->wstart = tok->src->curpos;
    /* go past 'while' */
    tok = tokenize(tok->src);
    struct node_s *_while   = new_node(NODE_WHILE);
    if(!_while) return NULL;
    _while->lineno = lineno;
    struct node_s *compound = parse_compound_list(tok, TOKEN_KEYWORD_DO);
    if(compound)
    {
        add_child_node(_while, compound);
        struct node_s *parse_group = parse_do_group(tok);
        if(parse_group) add_child_node(_while, parse_group);
        return _while;
    }
    else
    {
        free_node_tree(_while);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
}

struct node_s *parse_until_clause(struct token_s *tok)
{
    int lineno = tok->lineno;
    tok->src->wstart = tok->src->curpos;
    /* go past 'until' */
    tok = tokenize(tok->src);
    struct node_s *_until   = new_node(NODE_UNTIL);
    if(!_until) return NULL;
    _until->lineno = lineno;
    struct node_s *compound = parse_compound_list(tok, TOKEN_KEYWORD_DO);
    if(compound)
    {
        add_child_node(_until, compound);
        struct node_s *parse_group = parse_do_group(tok);
        if(parse_group) add_child_node(_until, parse_group);
        return _until;
    }
    else
    {
        free_node_tree(_until);
        return NULL;
    }
}

struct node_s *parse_brace_group(struct token_s *tok)
{
    /* go past '{' */
    tok = tokenize(tok->src);
    struct node_s *node = parse_compound_list(tok, TOKEN_KEYWORD_RBRACE);
    struct token_s *tok2;
    tok = get_current_token();
    /*
     * if we have a nested function that ends right before the current one, something like:
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
    if(tok->type == TOKEN_KEYWORD_RBRACE) tok = tokenize(tok->src);
    return node;
}

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


struct node_s *parse_io_file(struct token_s *tok)
{
    struct node_s *file = new_node(NODE_IO_FILE);
    if(!file) return NULL;
    file->lineno = tok->lineno;
    if(tok->text[0] == '<')
    {
        switch(tok->text[1])
        {
            case '\0': set_node_val_chr(file, IO_FILE_LESS     ); break;
            case  '&': set_node_val_chr(file, IO_FILE_LESSAND  ); break;
            case  '>': set_node_val_chr(file, IO_FILE_LESSGREAT); break;
        }
    }
    else if(tok->text[0] == '>')
    {
        switch(tok->text[1])
        {
            case  '!':      /* zsh extension, equivalent to >| */
            case  '|': set_node_val_chr(file, IO_FILE_CLOBBER  ); break;
            case '\0': set_node_val_chr(file, IO_FILE_GREAT    ); break;
            case  '&': set_node_val_chr(file, IO_FILE_GREATAND ); break;
            case  '>': set_node_val_chr(file, IO_FILE_DGREAT   ); break;
        }
    }
    else if(tok->text[0] == '&')
    {
        if(strcmp(tok->text, "&>>") == 0)                   /* append stdout/stderr   */
            set_node_val_chr(file, IO_FILE_AND_GREAT_GREAT);
        else if(strcmp(tok->text, "&>") == 0)               /* redirect stdout/stderr */
            set_node_val_chr(file, IO_FILE_GREATAND);       /* treat '&>' as '>&'     */
        else if(strcmp(tok->text, "&<") == 0)
            set_node_val_chr(file, IO_FILE_LESSAND );       /* treat '&<' as '<&'     */
    }
    tok = tokenize(tok->src);
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        free_node_tree(file);
        return NULL;
    }
    set_node_val_str(name, tok->text);
    name->lineno = tok->lineno;
    add_child_node(file, name);
    tok = tokenize(tok->src);
    /*
     * zsh says r-shell can't redirect output to files. if the token that comes after
     * the output redirection operator is not a number, we treat it as a filename and 
     * raise an error (if the shell is restricted, of course).
     */
    if(startup_finished && option_set('r') && name->val.str &&
       file->val.chr >= IO_FILE_LESSGREAT && file->val.chr <= IO_FILE_DGREAT)
    {
        char *strend;
        strtol(name->val.str , &strend, 10);
        if(strend == name->val.str)     /* error parsing number means we've got a file name */
        {
            fprintf(stderr, "%s: cannot redirect output to file `%s`: restricted shell\r\n", SHELL_NAME, name->val.str);
            free_node_tree(file);
            return NULL;
        }
    }
    return file;
}

struct node_s *parse_io_here(struct token_s *tok)
{
    int strip = 0;
    struct node_s *file = new_node(NODE_IO_HERE);
    if(!file) return NULL;
    file->lineno = tok->lineno;
    /* is it a here-string? */
    if(tok->text[2] == '<')
    {
        tok = tokenize(src);
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            PARSER_RAISE_ERROR(UNEXPECTED_TOKEN, get_previous_token(), TOKEN_EOF);
            return NULL;
        }
        set_node_val_chr(file, IO_HERE_EXPAND  );
        struct node_s *here = new_node(NODE_VAR);
        if(!here)
        {
            free_node_tree(file);
            return NULL;
        }
        here->val.str = get_malloced_str(tok->text);
        add_child_node(file, here);
        return file;
    }

    /* is it a stripped here-doc? */
    if(tok->text[2] == '-')
    {
        strip = 1;
    }
    /*****************************************/
    /* NOTE: brute-force extract the heredoc */
    /* TODO: maybe find a better workaround? */
    /*****************************************/
    struct source_s  *src     = tok->src;
    struct cmd_token *heredoc = get_heredoc(src->buffer+src->curpos+1, strip);
    if(!heredoc)
    {
        free_node_tree(file);
        return NULL;
    }
    if(heredoc->token_type == HEREDOC_TOKEN_EXP )
         set_node_val_chr(file, IO_HERE_EXPAND  );
    else set_node_val_chr(file, IO_HERE_NOEXPAND);
    struct node_s *here = new_node(NODE_VAR);
    if(!here)
    {
        free_node_tree(file);
        free_all_tokens(heredoc);
        return NULL;
    }
    here->val.str = heredoc->data;
    add_child_node(file, here);
    free(heredoc);
    src->bufsize = strlen(src->buffer);
    return file;
}

struct node_s *parse_io_redirect(struct token_s *tok)
{
    struct node_s *io = new_node(NODE_IO_REDIRECT);
    if(!io) return NULL;
    io->lineno = tok->lineno;
    if(tok->type == TOKEN_IO_NUMBER)
    {
        set_node_val_sint(io, atoi(tok->text));
        tok = tokenize(tok->src);
    }
    else
    {
        if(tok->text[0] == '<') set_node_val_sint(io, 0);
        else                    set_node_val_sint(io, 1);
    }
    struct node_s *file = ((tok->text[0] == '<') && (tok->text[1] == '<')) ?
                            parse_io_here(tok) : parse_io_file(tok);
    if(file) add_child_node(io, file);
    return io;
}

static inline int is_redirect_op(char *str)
{
    if(!str) return 0;
    if(*str == '>' || *str == '<') return 1;
    if(*str == '&' && str[1] == '>') return 1;
    return 0;
}

struct node_s *parse_redirect_list(struct token_s *tok)
{
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) return NULL;
    if(!is_redirect_op(tok->text) && tok->type != TOKEN_IO_NUMBER)
        return NULL;
    struct node_s *io = new_node(NODE_IO_REDIRECT_LIST);
    if(!io) return NULL;
    io->lineno = tok->lineno;
    while(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
    {
        struct node_s *item = parse_io_redirect(tok);
        if(item) add_child_node(io, item);
        tok = get_current_token();
    }
    return io;
}


static inline int is_compound_keyword(struct token_s *tok)
{
    if((tok->type == TOKEN_KEYWORD_LBRACE) ||
       (tok->type == TOKEN_OPENBRACE     ) ||
       (tok->type == TOKEN_KEYWORD_FOR   ) ||
       (tok->type == TOKEN_KEYWORD_CASE  ) ||
       (tok->type == TOKEN_KEYWORD_IF    ) ||
       (tok->type == TOKEN_KEYWORD_WHILE ) ||
       (tok->type == TOKEN_KEYWORD_UNTIL ))
        return 1;
     
    if((tok->type == TOKEN_KEYWORD_SELECT) && !option_set('P'))
        return 1;
     
    return 0;
}

struct node_s *parse_function_body(struct token_s *tok)
{
    struct node_s *compound = parse_compound_command(tok);
    if(!compound) return NULL;
    tok = get_current_token();
    struct node_s *redirect = parse_redirect_list(tok);
    if(redirect) add_child_node(compound, redirect);
    return compound;
}

/*
 * the 'using_keyword' flag tells us if the 'function' keyword was used in 
 * defining this function, such as:
 *       funcion name { ... }
 * 
 * instead of:
 *       name() { ... }
 * 
 * in the former case, the braces are optional, while they are mandatory
 * in the latter case. the former is an extension, while the latter is POSIX.
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
    struct symtab_entry_s *func = add_func(tok->text);
    if(!func) return NULL;
    tok = get_current_token();
    //int wstart = tok->linestart+tok->charno;
    int wstart = tok->src->curpos;
    if(tok->type == TOKEN_OPENBRACE)
    {
        //wstart--;
        tok = tokenize(tok->src);
        if(tok->type != TOKEN_CLOSEBRACE)
        {
            err_tok = TOKEN_CLOSEBRACE;
            goto err;
        }
        tok = tokenize(tok->src);
    }
    else if(!using_keyword)
    {
        err_tok = TOKEN_OPENBRACE;
        goto err;
    }
    while(tok->type != TOKEN_EOF && tok->type == TOKEN_NEWLINE)
        tok = tokenize(tok->src);
    struct node_s *body = parse_function_body(tok);
    if(body) func->func_body = body;
    func->val_type = SYM_FUNC;
    tok = get_current_token();
    /*
     * our offset is relative to the start of the function, not the whole script. this will not
     * help us in getting the command string of this function (everything between the opening and
     * closing brace. we need to manipulate the offsets in order to get the correct offset.
     */
    int l = tok->linestart, c = tok->charno;
    tok->linestart = tok->src->curpos; tok->charno = 0;
    char *cmdline = get_cmdwords(tok, wstart);
    tok->linestart = l; tok->charno = c;
    symtab_entry_setval(func, cmdline);
    free_malloced_str(cmdline);
    set_exit_status(0, 0);
    /* we are not going to execute the function body right now */
    return node_func_def;
    
err:
    PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, err_tok);
    set_exit_status(1, 0);
    rem_from_symtab(func);
    EXIT_IF_NONINTERACTIVE();
    return NULL;
}


char *__parse_alias(char *cmd)
{
    if(!is_name(cmd)) return cmd;
    int i;
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(__aliases[i].name)
        {
            if(strcmp(__aliases[i].name, cmd) == 0)
            {
                /* now get the alias value */
                if(__aliases[i].val) return __aliases[i].val;
                return null_alias;
                
            }
        }
    }
    return cmd;
}

struct node_s *parse_simple_command(struct token_s *tok)
{
    struct node_s *cmd = new_node(NODE_COMMAND);
    if(!cmd) return NULL;
    cmd->lineno = tok->lineno;
    /* prefix */
    int has_prefix = 0;
    if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
    {
        struct node_s *redirect = parse_io_redirect(tok);
        if(redirect) add_child_node(cmd, redirect);
    }
    else if(tok->type == TOKEN_ASSIGNMENT_WORD)
    {
        /* TODO: add the assignment word to command */
        struct node_s *assign = new_node(NODE_ASSIGNMENT);
        if(!assign)
        {
            free_node_tree(cmd);
            return NULL;
        }
        set_node_val_str(assign, tok->text);
        assign->lineno = tok->lineno;
        add_child_node(cmd, assign);
    }
    else goto cmd_word;
    has_prefix = 1;
    tok = get_current_token();
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
    {
        if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
        {
            struct node_s *redirect = parse_io_redirect(tok);
            if(redirect) add_child_node(cmd, redirect);
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
        else break;
        tok = tokenize(tok->src);
    }
    
cmd_word:
    if(tok->type != TOKEN_WORD)
    {
        if(!cmd->children)
        {
            free(cmd);
            cmd = NULL;
        }
        else
        {
            char *cmdline = get_cmdwords(tok, tok->src->wstart);
            set_node_val_str(cmd, cmdline);   /* get the command line */
            free_malloced_str(cmdline);
        }
        return cmd;
    }
    struct node_s *word = new_node(NODE_VAR);
    if(!word)
    {
        free_node_tree(cmd);
        return NULL;
    }
    set_node_val_str(word, tok->text);
    word->lineno = tok->lineno;
    /* is this a test or '[[' command? */
    add_child_node(cmd, word);
    int istest = (strcmp(tok->text, "[[") == 0) ? 2 :
                 (strcmp(tok->text, "[" ) == 0) ? 1 : 0;
    if(has_prefix) tok = tokenize(tok->src);
    else           tok = get_current_token();
    if(tok->type == TOKEN_EOF) return cmd;
    if(tok->type == TOKEN_ERROR)
    {
        free_node_tree(cmd);
        return NULL;
    }
    /* suffix */
    struct node_s *last = last_child(cmd);
    do
    {
        if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
        {
            struct node_s *redirect = parse_io_redirect(tok);
            if(redirect)
            {
                /*
                 * check for the non-POSIX bash redirection extensions of {var}<&N
                 * and {var}>&N. the {var} part would have been added as the previous
                 * node.
                 */
                if(last && last->type == NODE_VAR)
                {
                    char *s = last->val.str;
                    if(s[0] == '{' && s[strlen(s)-1] == '}')
                    {
                        set_node_val_str(redirect, s);
                        redirect->next_sibling = last->next_sibling;
                        redirect->prev_sibling = last->prev_sibling;
                        if(cmd->first_child == last) cmd->first_child = redirect;
                        else redirect->prev_sibling->next_sibling = redirect;
                        free_node_tree(last);
                    }
                    else add_child_node(cmd, redirect);
                }
                else add_child_node(cmd, redirect);
                last = redirect;
            }
            tok = get_current_token();
            continue;
        }
        /*
        else if(tok->type == TOKEN_AND     || tok->type == TOKEN_SEMI  ||
                tok->type == TOKEN_NEWLINE || tok->type == TOKEN_DSEMI ||
                tok->type == TOKEN_AND_IF  || tok->type == TOKEN_OR_IF ||
                tok->type == TOKEN_PIPE    || tok->type == TOKEN_CLOSEBRACE || 
                tok->type == TOKEN_KEYWORD_RBRACE)
        {
            break;
        }
        */
        else if(is_separator_tok(tok->type))
        {
            if(!istest) break;
            /* the test command accepts !, &&, ||, (, ) */
            if(tok->type != TOKEN_KEYWORD_BANG && tok->type != TOKEN_AND_IF && 
               tok->type != TOKEN_OR_IF && tok->type != TOKEN_CLOSEBRACE && tok->type != TOKEN_OPENBRACE)
                break;
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
        struct node_s *word = new_node(NODE_VAR);
        if(!word)
        {
            free_node_tree(cmd);
            return NULL;
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
        if(istest == 2 && strcmp(word->val.str, "]]") == 0) break;
        else if(istest == 1 && strcmp(word->val.str, "]") == 0) break;
    } while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR);

    if(cmd)
    {
        char *cmdline = get_cmdwords(tok, tok->src->wstart);
        set_node_val_str(cmd, cmdline);   /* get the command line */
        free_malloced_str(cmdline);
    }
    return cmd;
}

struct node_s *parse_command(struct token_s *tok)
{
    while(tok->type == TOKEN_NEWLINE || tok->type == TOKEN_SEMI)
    {
        /* save the start of this line */
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(src);
    }
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) return NULL;
    if(is_compound_keyword(tok))
    {
        struct node_s *compound = parse_compound_command(tok);
        if(!compound) return NULL;
        tok = get_current_token();
        struct node_s *redirect = parse_redirect_list(tok);
        if(redirect) add_child_node(compound, redirect);
        return compound;
    }
    /* 'time' special keyword */
    else if(tok->type == TOKEN_KEYWORD_TIME)
    {
        struct node_s *t = new_node(NODE_TIME);
        if(!t) return NULL;
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
        {
            return t;
        }
        struct node_s *cmd = parse_command(tok);
        if(cmd)
        {
            add_child_node(t, cmd);
        }
        return t;
    }
    struct token_s *tok2 = dup_token(tok);
    if(!tok2) return NULL;
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
        if(!tok2) return NULL;
        int l = tok->src->curline, c = tok->src->curchar;
        tok2->src->curline = 1;
        tok2->src->curchar = 1;
        tok = tokenize(tok->src);
        struct node_s *func = parse_function_definition(tok2, 1);
        tok->src->curline += l;
        tok->src->curchar += c;
        free_token(tok2);
        return func;
    }
    /* POSIX way of defining functions */
    else if(is_name(tok2->text) && !is_special_builtin(tok2->text) &&
       tok->type != TOKEN_EOF && tok->type == TOKEN_OPENBRACE /* tok->text[0] == '(' */)
    {
        int l = tok->src->curline, c = tok->src->curchar;
        tok->src->curline = 1;
        tok->src->curchar = 1;
        struct node_s *func = parse_function_definition(tok2, 0);
        tok->src->curline += l;
        tok->src->curchar += c;
        free_token(tok2);
        return func;
    }
    struct node_s *cmd = parse_simple_command(tok2);
    free_token(tok2);
    return cmd;
}


struct node_s *parse_translation_unit()
{
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
        if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
        {
            i = src->curpos;
            /* save the start of this line */
            src->wstart = src->curpos;
            tok = tokenize(tok->src);
        }
        else break;
    }
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) return NULL;
    /* first time ever? */
    if(i < 0) i = 0;
    struct node_s *root = new_node(NODE_PROGRAM);
    int save_hist = option_set('i') && src->filename == stdin_filename;
    parser_err = 0;
    while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
    {
        //lineno = src->curline;
        struct node_s *cmd = parse_complete_command(tok);
        tok = get_current_token();
        if(!cmd) break;
        if(parser_err)
        {
            free_node_tree(cmd);
            break;
        }
        if(!noexec)
        {
            if(!cmd->lineno) cmd->lineno = src->curline;
            add_child_node(root, cmd);

            switch(cmd->type)
            {
                case NODE_TIME:
                    if(cmd->first_child) cmd = cmd->first_child;
                    else
                    {
                        if(save_hist) save_to_history("time");
                        if(option_set('v')) fprintf(stderr, "time\r\n");
                        break;
                    }
                    
                case NODE_COMMAND:
                case NODE_LIST:
                    if(cmd->val.str)
                    {
                        if(save_hist) save_to_history(cmd->val.str);
                        if(option_set('v')) fprintf(stderr, "%s\r\n", cmd->val.str);
                        break;
                    }
                    
                default:
                    ;
                    int j = src->curpos-(tok->text_len);
                    while(src->buffer[j] == '\n') j--;
                    char buf[j-i+1];
                    int k = 0;
                    do
                    {
                        buf[k++] = src->buffer[i++];
                    } while(i < j);
                    buf[k] = '\0';
                    if(save_hist) save_to_history(buf);
                    if(option_set('v')) fprintf(stderr, "%s\n", buf);
                    /* if(cmd->type == NODE_COMMAND)
                    {
                        //set_node_val_str(cmd, get_last_cmd_history());
                        if(buf[k-1] == '\n') buf[k-1] = '\0';
                        set_node_val_str(cmd, get_malloced_str(buf));
                    }
                    else if(cmd->type == NODE_LIST)
                    {
                        if(cmd->first_child->type == NODE_COMMAND)
                            set_node_val_str(cmd->first_child, get_last_cmd_history());
                    } */
            }
        }
        
        while(tok->type != TOKEN_EOF && tok->type != TOKEN_ERROR)
        {
            if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
                tok = tokenize(tok->src);
            else break;
        }
        if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) break;
        i = src->curpos-(tok->text_len);
        while(src->buffer[i] == '\n') i++;
        /* save the start of this line */
        src->wstart = src->curpos-(tok->text_len);
    }
    /* nothing useful found */
    if(!root->children)
    {
        free_node_tree(root);
        root = NULL;
    }
    return root;
}
