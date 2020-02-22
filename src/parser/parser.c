/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "../builtins/builtins.h"
#include "../builtins/setx.h"
#include "node.h"
#include "parser.h"

/*********************************************/
/* top-down, recursive descent syntax parser */
/*********************************************/

/* flag to indicate a parsing error */
int parser_err = 0;


#if 0
/*
 * convert a list of words to a list of AST nodes.
 */
struct node_s *wordlist_to_nodetree(struct word_s *first_word)
{
    struct node_s *head_node = NULL, *tail_node = NULL, *node;
    while(first_word)
    {
        node = new_node(NODE_VAR);
        if(!node)
        {
            break;
        }
        set_node_val_str(node, first_word->data);
        if(!head_node)
        {
            head_node = node;
            tail_node = node;
        }
        else
        {
            tail_node->next_sibling = node;
            tail_node = node;
        }
        first_word = first_word->next;
    }
    return head_node;
}
#endif


/*
 * get the next word in a simple command's command line.. the word's beginning
 * and ending are stored in 'start' and 'end'.. if the word is found, 1 is
 * returned, otherwise 0 is returned if the command line finished (no more words).
 * if do_braces is non-zero, the function skips over brace loops ( ), adding
 * everything between the braces to the word, including whitespace chars.
 */
int next_cmd_word(char **start, char **end, int do_braces)
{
    char *p = *end;
    size_t i;
    
    /* skip spaces before the next word */
    while(isspace(*p))
    {
        p++;
    }
    
    /* end of string */
    if(!*p)
    {
        return 0;
    }
    
    /* we got the word's beginning, now find the end */
    (*start) = p;

    while(*p && !isspace(*p))
    {
        switch(*p)
        {
            case  '"':
            case '\'':
            case  '`':
                i = find_closing_quote(p, (*p == '"') ? 1 : 0, 0);
                p += i;
                break;
                
            case '\\':
                p++;
                break;
                
            case '(':
                if(!do_braces)
                {
                    break;
                }
                __attribute__((fallthrough));
                
            case '[':
                /* add the opening brace and everything up to the closing brace to the buffer */
                i = find_closing_brace(p, 0);
                p += i;
                break;
            
            case '$':
                switch(p[1])
                {
                    /* we have a '${', '$(' or '$[' sequence */
                    case '{':
                    case '(':
                    case '[':
                        /* add the opening brace and everything up to the closing brace to the buffer */
                        i = find_closing_brace(++p, 0);
                        p += i;
                        break;
                        
                    /* we have $#, $$, or $< (csh/tcsh) */
                    case '#':
                    case '$':
                    case '<':
                        p++;
                        break;
                }
                break;
        }
        
        p++;
    }
    
    (*end) = p;
    return 1;
}


/*
 * check if 'orig_name' is an alias name.. if so, check the first word in the 
 * alias value recuresively for aliases (unless it's the same as 'orig_name').
 * returns the malloc'd aliased value, or NULL in case of error.
 */
char *get_alias_recursive(char *orig_name)
{
    char *p, *p2, *val = NULL;
    size_t start, end;
    char *name = get_malloced_str(orig_name);
    
    while(1)
    {
        /* get the aliased value (if none defined, returns the same word) */
        p = get_alias_val(name);

        /* undefined or null alias */
        if(p == name || p == NULL)
        {
            free_malloced_str(name);
            break;
        }
        
        if(val)
        {
            p2 = substitute_str(val, p, start, end-1);
            free(val);

            if(!p2)
            {
                PRINT_ERROR("%s: insufficient memory for alias substitution\n", SHELL_NAME);
                free_malloced_str(name);
                return NULL;
            }

            val = p2;
        }
        else
        {
            val = __get_malloced_str(p);
        }

        /* skip any leading spaces */
        p2 = p;
        while(isspace(*p2))
        {
            p2++;
        }
        start = p2-p;
        
        /* get the first word in the alias value */
        while(*p2 && !isspace(*p2))
        {
            p2++;
        }
        end = p2-p;
        
        free_malloced_str(name);
        name = get_malloced_strl(p, start, end-start);
        if(!name)
        {
            PRINT_ERROR("%s: insufficient memory for alias substitution\n", SHELL_NAME);
            free(val);
            return NULL;
        }

        /* prevent infinite looping */
        if(strcmp(name, orig_name) == 0)
        {
            free_malloced_str(name);
            break;
        }
    }
    
    return val;
}


/*
 * substitute an aliased word with it's value.. we recursively check the alias 
 * value until we hit an unaliased word, or a word that's identical to the 
 * aliased word we're substituting, so that we won't enter an infinite loop 
 * where we keep substituting an alias by itself.. for example, the alias:
 * ls='ls --color=auto', when processed, results in the same alias word 'ls'..
 * if we tried to substitute it again, we'll loop infinitely..
 *
 * returns the malloc'd alias value, or 'orig_cmd' if the word isn't an alias.
 */
char *do_alias_substitution(char *orig_cmd)
{
    /* find the first command word */
    char *p, *p2, *cmd = orig_cmd;
    char *start, *end = cmd;
    char *word, *alias_val;
    char *errmsg = NULL;
    int skip_next = 0, endme = 0;
    
    while(next_cmd_word(&start, &end, 0))
    {
        /* get a copy of the next word */
        size_t len = end-start;
        word = get_malloced_strl(start, 0, len);
        if(!word)
        {
            PRINT_ERROR("%s: insufficient memory for alias substitution\n", SHELL_NAME);
            return NULL;
        }
        
        /*
         * in case of a redirection operator, we skip the word following the
         * operator, which typically includes the pathname of a redirected
         * file, or a file descriptor number.
         */
        if(skip_next)
        {
            skip_next = 0;
            free_malloced_str(word);
            continue;
        }
        
        /*
         * check for redirection operators and variable assignments which can precede 
         * the command name (also known as the 'command prefix').
         */
        p = word;
        switch(*p)
        {
            case '&':
                p++;
                if(*p == '<')       /* &< */
                {
                    p++;
                }
                else if(*p == '>')  /* &> */
                {
                    p++;
                    if(*p == '>')   /* &>> */
                    {
                        p++;
                    }
                }
                skip_next = !(*p);
                break;
                
            case '>':
                p++;
                if(*p == '>' || *p == '|' || *p == '!' || *p == '&')
                {
                    p++;
                }
                skip_next = !(*p);
                break;
                
            case '<':
                p++;
                if(*p == '>' || *p == '&')
                {
                    p++;
                    skip_next = !(*p);
                }
                else if(*p == '<')
                {
                    p++;
                    if(*p == '<')
                    {
                        /* here-string '<<<' */
                        p2 = herestr_end(++p);
                        if(!p2)
                        {
                            errmsg = "empty here-string";
                            goto err;
                        }
                        end = p2;
                        break;
                    }
                    else if(*p == '-')
                    {
                        /* here-document '<<-' */
                        p++;
                    }

                    char *delim, *nl;
                    int expand;

                    p2 = heredoc_end(p, &expand, &delim, &nl, 0);
                    if(!p2)
                    {
                        errmsg = NULL;
                        goto err;
                    }

                    end = p2+strlen(delim);
                }
                else
                {
                    skip_next = !(*p);
                }
                break;
            
            default:
                /*
                 * if the word contains '=', and if the string before the '=' 
                 * is a valid var name, we have a variable assignment, which we
                 * skip.
                 */
                p = strchr(word, '=');
                if(p)
                {
                    p2 = get_malloced_strl(word, 0, p-word);
                    if(!p2)
                    {
                        errmsg = "insufficient memory for alias substitution";
                        goto err;
                    }
                    
                    int n = is_name(p2);
                    free_malloced_str(p2);
                    
                    if(n)
                    {
                        skip_next = 0;
                        break;
                    }
                }
                
                /* normal command word. check for aliases */
                alias_val = get_alias_recursive(word);
                if(!alias_val)
                {
                    endme = 1;
                    break;
                }
                
                /* substitute the aliased value */
                size_t len = strlen(alias_val);
                size_t i = start-cmd+len;
                
                p = substitute_str(cmd, alias_val, start-cmd, end-cmd-1);
                if(!p)
                {
                    errmsg = "insufficient memory for alias substitution";
                    goto err;
                }
                
                if(cmd != orig_cmd)
                {
                    free(cmd);
                }
                
                cmd = p;
                end = p+i;
                
                /* check next word? */
                if(!isspace(alias_val[len-1]))
                {
                    endme = 1;
                }
                break;
        }
        
        free_malloced_str(word);
        
        if(endme)
        {
            break;
        }
    }
    
    return cmd;

err:
    if(errmsg)
    {
        PRINT_ERROR("%s: %s\n", SHELL_NAME, errmsg);
    }
    free_malloced_str(word);
    return NULL;
}


#if 0
/*
 * substitute a token whose text we identified as an alias word
 * with the value of that alias.. the 'aliased_word' argument tells us the
 * word we're substituting, so that we won't enter an infinite loop where we 
 * keep substituting an alias by itself.. for example, the alias:
 * ls='ls --color=auto', when processed, results in the same alias word 'ls'..
 * if we tried to substitute it again, we'll loop infinitely..
 * the 'first_aliased_word' argument contains the first word for which we'll
 * recursively substitute aliases, so that we can break the alias substitution
 * loop any time the original word occurs.
 *
 * returns 1 if the alias was successfully substituted, 0 otherwise.
 */
struct node_s *substitute_alias(char *first_aliased_word, char *aliased_word,
                                int *__check_next)
{
    /* sanity check */
    if(!aliased_word || !*aliased_word)
    {
        return NULL;
    }

    /* get the aliased value (if none defined, returns the same word) */
    char *alias_val = get_alias_val(aliased_word);

    /* undefined or null alias */
    if(alias_val == aliased_word || alias_val == NULL)
    {
        return NULL;
    }

    /* split the aliased value into words (or fields) */
    struct word_s *wordlist = field_split(alias_val);
    size_t val_len = strlen(alias_val);
    struct node_s *nodes = wordlist_to_nodetree(wordlist);
    free_all_words(wordlist);

    /* no field splitting done. use the original value */
    if(!nodes)
    {
        nodes = new_node(NODE_VAR);
        if(nodes)
        {
            char *val2 = __get_malloced_str(alias_val);
            /* remove trailing spaces */
            char *p1 = val2;
            if(p1)
            {
                char *p2 = p1+val_len-1;
                while(p2 > p1 && isspace(*p2))
                {
                    p2--;
                }
                if(isspace(p2[1]))
                {
                    p2[1] = '\0';
                }
            }
            set_node_val_str(nodes, val2);
            free(val2);
        }
        else
        {
            return NULL;
        }
        //(*__check_next) = nodes && isspace(alias_val[val_len-1]) ? 1 : 0;
        /*
        if(!root_word)
        {
            return nodes;
        }
        */
    }

    /*
     * to avoid infinite looping, we check if the first word of the aliased
     * value is the same as the word we're checking for alias substitution.
     * if that's the case, we return immediately, otherwise we continue to
     * recursively check the aliased value for more aliases.
     */
    if(strcmp(nodes->val.str, aliased_word) == 0 ||
       strcmp(nodes->val.str, first_aliased_word) == 0)
    {
        char *val = nodes->val.str;
        (*__check_next) = val && isspace(val[strlen(val)-1]) ? 1 : 0;
        return nodes;
    }

    /* perform the alias substitution */
    int check_next = 1, endme = 0;
    /*
     * we perform alias substitution on the original word, then we check the
     * substitution for possible aliases.. we repeat the process until we get
     * a substitution with no further aliases.. we'll only consider the original
     * aliased values when we want to determine whether we should check the
     * next word for aliases or not.
     */
    while(check_next)
    {
        struct node_s *node = nodes, *pnode = NULL;
        while(node)
        {
            struct node_s *subwords = substitute_alias(first_aliased_word, 
                                                       node->val.str, &check_next);
            /*
             * no aliases found, or the alias contains the same word we're checking
             * for alias substitution.
             */
            if(!subwords)
            {
                endme = 1;
                break;
            }
            
            if(strcmp(subwords->val.str, aliased_word) == 0 ||
               strcmp(subwords->val.str, first_aliased_word) == 0)
            {
                endme = 1;
                check_next = 0;
            }

            /* find the last word */
            struct node_s *node2 = subwords;
            while(node2->next_sibling)
            {
                node2 = node2->next_sibling;
            }

            /*
             * remove the word we just substituted from the wordlist, and put the
             * expanded alias in its place, then adjust our pointers.
             */
            if(pnode)
            {
                if(pnode->next_sibling == node)
                {
                    node2->next_sibling = NULL;
                }
                else
                {
                    node2->next_sibling = pnode->next_sibling;
                }
                pnode->next_sibling = subwords;
            }
            else
            {
                node2->next_sibling = node->next_sibling;
            }

            pnode = node2;
            node->next_sibling = NULL;

            /* free temp memory */
            free_node_tree(node);

            /* if this word is the first word in the alias substitution, adjust our pointer */
            if(node == nodes)
            {
                nodes = subwords;
            }
            node = node2->next_sibling;
            if(!check_next)
            {
                break;
            }
        }
        
        /* force breaking out from the loop */
        if(endme)
        {
            break;
        }

        /* repeat checking from the start */
        check_next = 1;
    }
    /* determine whether to check the next word for aliases or not */
    (*__check_next) = isspace(alias_val[val_len-1]) ? 1 : 0;
    /* return the expanded alias words */
    return nodes;
}
#endif


/*
 * return the command string of the command being parsed.
 */
char *get_cmdwords(struct token_s *tok, int wstart)
{
    if(!tok || !tok->src)
    {
        return NULL;
    }

    /* find the end of the command string */
    struct source_s *src = tok->src;
    int wend = tok->linestart + tok->charno - tok->text_len;

    /* at the very first word we would have a -ve position */
    if(wstart < 0)
    {
        wstart = 0;
    }
    
    /* make sure we get the beginning of the word */
    if(src->buffer[wstart] != '(')
    {
        while(wstart > 0 && !isspace(src->buffer[wstart]))
        {
            wstart--;
        }
    }
    
    /* skip spaces before the command string */
    while(isspace(src->buffer[wstart]))
    {
        wstart++;
    }

    /* ignore separator operators at the end of a command */
    char c = src->buffer[wend];
    if(c == ')' || c == '&' || c == ';')
    {
        wend--;
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
 * skip list terminator tokens, which include '&', ';' and '\n'.
 */
void skip_separator_operators(void)
{
    struct token_s *tok = get_current_token();

    /* skip & and ; operators */
    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
    {
        tok = tokenize(tok->src);
    }

    /* skip optional newlines */
    skip_newline_tokens();
}


/*
 * parse a command list that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_list(struct token_s *tok)
{
    /* a list consists of one or more AND-OR lists, separated by & or ; operators */
    struct node_s *node = parse_and_or(tok);
    if(!node)
    {
        return NULL;
    }
    tok = get_current_token();

    int type = tok->type;
    tok->src->wstart = tok->src->curpos;
    /* list not ending in & or ;. return the list */
    if(tok->type != TOKEN_AND && tok->type != TOKEN_SEMI)
    {
        skip_separator_operators();
        return node;
    }

    /* we have & or ; operator. skip and parse the next list */
    tok = tokenize(tok->src);
    tok->src->wstart++;

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
        skip_separator_operators();
        return list;
    }

    /* reached EOF or error getting the next token */
    if(tok->type == TOKEN_EOF)
    {
        return list;
    }

    /* parse the second list */
    node = parse_list(tok);
    if(node)
    {
        add_child_node(list, node);
    }

    /* save the command line */
    skip_separator_operators();
    
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
    enum token_type_e last_type = 0;

    while(node)
    {
        tok = get_current_token();
        int type = tok->type;
        if(tok->type == TOKEN_AND_IF || tok->type == TOKEN_OR_IF)
        {
            /* we have && or ||. skip and continue parsing the next pipeline */
            tok->src->wstart = tok->src->curpos+1;
            tok = tokenize(tok->src);
            /* skip optional newlines */
            skip_newline_tokens();
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
                    and_or = NULL;
                    break;
                }
                add_child_node(child , node );
                add_child_node(and_or, child);
                char *cmdline = get_cmdwords(tok, wstart);
                set_node_val_str(and_or, cmdline);
                free_malloced_str(cmdline);
            }
            else
            {
                and_or = node;
            }
            break;
        }
    
        /* reached EOF or error getting the next token */
        if(tok->type == TOKEN_EOF)
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
            and_or = NULL;
            break;
        }
    
        /* first child ever */
        if(!and_or)
        {
            /* create a new node for the AND-OR list */
            and_or = new_node(NODE_ANDOR);
            if(!and_or)     /* insufficient memory */
            {
                free_node_tree(node);
                break;
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
                and_or = NULL;
                break;
            }
            /* add the pipeline to the AND-OR list */
            add_child_node(child , node );
            add_child_node(and_or, child);
            child->lineno = node->lineno;
        }
        last_type = type;
        /* parse the next pipeline */
        node = parse_pipeline(tok);
    }
    
    /* return the AND-OR list */
    return and_or;
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
        tok = get_current_token();
        tok->src->wstart = tok->src->curpos;

        /* we have a pipe operator ('|' or '|&') */
        if(tok->type == TOKEN_PIPE || tok->type == TOKEN_PIPE_AND)
        {
            enum token_type_e type = tok->type;
            tok = tokenize(tok->src);
            tok->src->wstart = tok->src->curpos;

            /* skip optional newlines */
            skip_newline_tokens2();

            /* add implicit redirection of '2>&1' if the '|&' pipe operator was used */
            if(type == TOKEN_PIPE_AND)
            {
                struct node_s *io = io_file_node(2, IO_FILE_GREATAND, "1", node->lineno);
                if(io)
                {
                    add_child_node(node, io);
                }
            }

            /* reached EOF or error parsing */
            if(tok->type == TOKEN_EOF)
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
            }
            else
            {
                pipe = node;
            }
            break;
        }
    }

    /* failed to parse pipe sequence */
    if(!pipe)
    {
        return NULL;
    }

    /* pipeline has a bang */
    if(has_bang)
    {
        struct node_s *bang = new_node(NODE_BANG);
        if(!bang)       /* insufficient memory */
        {
            free_node_tree(pipe);
            return NULL;
        }
        add_child_node(bang, pipe);
        bang->lineno = pipe->lineno;
        pipe = bang;
    }


    /* return the pipeline nodetree */
    return pipe;
}


/*
 * parse a term (list) that starts with the given token, and stop when we get
 * the stop_at token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.
 */
struct node_s *parse_term(struct token_s *tok, enum token_type_e stop_at)
{
    /* a term consists of one or more AND-OR lists. parse the first list */
    struct node_s *node = parse_and_or(tok);
    if(!node)
    {
        return NULL;
    }
    tok = get_current_token();
    enum token_type_e type = tok->type;

    if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI || tok->type == TOKEN_NEWLINE)
    {
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);

        /* skip optional newlines */
        skip_newline_tokens2();

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
struct node_s *parse_compound_list(struct token_s *tok, enum token_type_e stop_at)
{
    /* create a new node for the list */
    struct node_s *list = new_node(NODE_LIST);
    if(!list)
    {
        return NULL;
    }

    /* skip optional newlines */
    skip_newline_tokens2();

    /* reached EOF or error parsing */
    if(tok->type == TOKEN_EOF)
    {
        free_node_tree(list);
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

        /* skip list separators '&' and ';' */
        if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI)
        {
            tok->src->wstart = tok->src->curpos;
            tok = tokenize(tok->src);
        }

        /* skip optional newlines */
        skip_newline_tokens2();
        tok = get_current_token();
        
        /* make sure we haven't reached EOF or an error token */
        if(tok->type == TOKEN_EOF)
        {
            break;
        }

        tok->src->wstart = tok->src->curpos;
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
    tok->src->wstart = tok->src->curpos;

    /* create a new node for the subshell */
    struct node_s *shell = new_node(NODE_SUBSHELL);
    if(!shell)
    {
        return NULL;
    }

    /* parse the compound list that forms the subshell's body */
    struct node_s *node = parse_compound_list(tok, TOKEN_RIGHT_PAREN);
    tok = get_current_token();
    
    /* current token should be a closing brace ')' */
    if(tok->type != TOKEN_RIGHT_PAREN)
    {
        /* missing ')' */
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_RIGHT_PAREN);
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
    tok->src->wstart = tok->src->curpos;

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
        case TOKEN_KEYWORD_LBRACE: return parse_brace_group(tok);
        case TOKEN_LEFT_PAREN    : return parse_subshell   (tok);
        case TOKEN_KEYWORD_FOR   : return parse_for_loop   (tok);
        case TOKEN_KEYWORD_SELECT: return parse_select_loop(tok);
        case TOKEN_KEYWORD_CASE  : return parse_case_clause(tok);
        case TOKEN_KEYWORD_IF    : return parse_if_clause  (tok);
        case TOKEN_KEYWORD_WHILE : return parse_while_loop (tok);
        case TOKEN_KEYWORD_UNTIL : return parse_until_loop (tok);
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
       (tok->type == TOKEN_LEFT_PAREN    ) ||       /* '(' */
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
 * same as is_compound_keyword(), except it works with a string, not a 
 * struct token_s (we need this to parse coproc commands).
 */
static inline int is_compound_keyword_str(char *str)
{
    if(!str || !*str)
    {
        return 0;
    }
    
    /* POSIX-defined compound words */
    if(strcmp(str, "for") == 0 || strcmp(str, "case") == 0 ||
       strcmp(str, "if") == 0 || strcmp(str, "while") == 0 ||
       strcmp(str, "until") == 0 || str[0] == '{' || str[0] == '(')
    {
        return 1;
    }

    /* non-POSIX compound word (only identified if POSIX mode is off) */
    if(strcmp(str, "select") == 0 && !option_set('P'))
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
    /* parse function body (which is just a compound command) */
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
 * the returned AST has the following structure:
 * 
 * NODE_FUNCTION (root node, val field contains function name as string)
 * |--> NODE_LIST (function body as AST)
 * +--> NODE_VAR (function body as string)
 * 
 */
struct node_s *parse_function_definition(struct token_s *tok, int using_keyword)
{
    /* special builtin utility names cannot be used as function names */
    if(is_special_builtin(tok->text))
    {
        PRINT_ERROR("%s: invalid function name: %s\n", SHELL_NAME, tok->text);
        set_internal_exit_status(1);
        EXIT_IF_NONINTERACTIVE();
        return NULL;
    }
    
    /* create the function's root node */
    struct node_s *func = new_node(NODE_FUNCTION);
    if(!func)
    {
        return NULL;
    }
    
    /* set the function name */
    set_node_val_str(func, tok->text);
    func->lineno = tok->src->curline;

    tok = get_current_token();
    int wstart = tok->src->curpos;
    
    /* POSIX-style of function definitions */
    if(tok->type == TOKEN_LEFT_PAREN)
    {
        /* skip the '(' */
#if 0
        char *p = tok->src->buffer+wstart;
        while(*p && *p != '(')
        {
            p++;
        }
        wstart = p-tok->src->buffer;
#endif
        tok = tokenize(tok->src);
    
        /* '(' should be followed by ')' */
        if(tok->type != TOKEN_RIGHT_PAREN)
        {
            /* missing ')' */
            PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_RIGHT_PAREN);
            EXIT_IF_NONINTERACTIVE();
            free_node_tree(func);
            return NULL;
        }
        
        /* skip the ')' */
        tok = tokenize(tok->src);
    }
    /* if not using the function keyword, a missing '(' is an error */
    else if(!using_keyword)
    {
        PARSER_RAISE_ERROR(EXPECTED_TOKEN, tok, TOKEN_LEFT_PAREN);
        EXIT_IF_NONINTERACTIVE();
        free_node_tree(func);
        return NULL;
    }
    
    /* skip optional newlines */
    skip_newline_tokens();
    
    /* parse the function body */
    struct node_s *body = parse_function_body(tok);
    if(body)
    {
        add_child_node(func, body);
        body->lineno = func->lineno;
    }
    else
    {
        PRINT_ERROR("%s: failed to parse function definition\n", SHELL_NAME);
        EXIT_IF_NONINTERACTIVE();
        free_node_tree(func);
        return NULL;
    }
    tok = get_current_token();
    
    /*
     * our offset is relative to the start of the function, not the whole script. this will not
     * help us in getting the command string of this function (everything between the opening and
     * closing brace. we need to manipulate the offsets in order to get the correct offset for
     * each command parsed as part of the function definition.
     */

    char *cmdline = get_cmdwords(tok, wstart);

    struct node_s *func_str = new_node(NODE_VAR);
    if(func_str)
    {
        set_node_val_str(func_str, cmdline);
        add_child_node(func, func_str);
    }
    free_malloced_str(cmdline);
    
    /* return the function's AST */
    return func;
}


/*
 * return the full command line of the next simple command.. the command line
 * is stored in 'cmd'.
 * 
 * returns 1 if the command is stored in 'cmd', 0 if we reached EOF.
 */
int read_complete_line(struct source_s *src, char **cmd)
{
    char *p1 = src->buffer + src->curpos;
    char *p2 = p1;
    size_t i, len;
    int endme = 0, has_heredoc = 0;
    
    while(*p2)
    {
        switch(*p2)
        {
            case '\n':
                endme = 1;
                break;
                
            case '(':
                /* if the brace is not the first char in the command line, stop reading here */
                if(p2 > p1 && isspace(p2[-1]))
                {
                    endme = 1;
                    break;
                }
                /* 
                 * otherwise, add the opening brace and everything up to the 
                 * closing brace to the buffer.
                 */
                i = find_closing_brace(p2, 0);
                p2 += i;
                break;
                
            case ';':
            case ')':
                endme = 1;
                break;
                
            case '|':
            case '&':
                /* recognize redirection operators, such as >& and <& */
                if(p2 > p1 && (p2[-1] == '>' || p2[-1] == '<'))
                {
                    break;
                }

                /*
                 * TODO: check if we're parsing the test command.
                 */
                endme = 1;
                break;
                
            case  '"':
            case '\'':
            case  '`':
                /*
                 * for quote chars, add the quote, as well as everything between this
                 * quote and the matching closing quote, to the command line.
                 */
                i = find_closing_quote(p2, (*p2 == '"') ? 1 : 0, 0);
                p2 += i;
                break;
                
            case '\\':
                p2++;
                break;
                
            case '[':
                /* add the opening brace and everything up to the closing brace to the buffer */
                i = find_closing_brace(p2, 0);
                p2 += i;
                break;
                        
            case '$':
                switch(p2[1])
                {
                    /* we have a '${', '$(' or '$[' sequence */
                    case '{':
                    case '(':
                    case '[':
                        /* add the opening brace and everything up to the closing brace to the buffer */
                        i = find_closing_brace(++p2, 0);
                        p2 += i;
                        break;
                        
                    /* we have $#, $$, or $< (csh/tcsh) */
                    case '#':
                    case '$':
                    case '<':
                        p2++;
                        break;
                }
                break;

            case '<':
                if(p2[1] == '<')
                {
                    if(p2[2] == '<')
                    {
                        /* here-string '<<<' */
                        p2 = herestr_end(p2+3);
                        if(!p2)
                        {
                            PRINT_ERROR("%s: empty here-string\n", SHELL_NAME);
                            len = 0;
                            goto check;
                        }
                        p2--;   /* will be incremented in the loop below */
                        break;
                    }
                    else if(p2[2] == '-')
                    {
                        /* here-document '<<-' */
                        p2 += 3;
                    }
                    else
                    {
                        /* here-document '<<' */
                        p2 += 2;
                    }

                    char *delim, *nl;
                    int expand;

                    p2 = heredoc_end(p2, &expand, &delim, &nl, 0);
                    if(!p2)
                    {
                        len = 0;
                        goto check;
                    }

                    p2 += strlen(delim);
                    has_heredoc = 1;
                    endme = 1;

                }
                break;
        }
        
        if(endme)
        {
            break;
        }
        
        p2++;
    }
    
    len = p2-p1;

check:
    if(len == 0)
    {
        (*cmd) = NULL;
        return 0;
    }
    
    (*cmd) = get_malloced_strl(p1, 0, len);
    /*
     * subtract 1, so when we call tokenize() in parse_simple_command() we will
     * end up having the correct offset.
     */
    src->curpos_old = src->curpos;
    src->curpos += len-1;

    if(has_heredoc)
    {
        /* make sure our cur_token is synced to the new src position */
        src->curpos--;  /* save the \n after the terminating delimiter */
        tokenize(src);
    }
    
    return len;
}


/* 
 * parse the simple command that starts with the given token.
 * 
 * returns the parsed nodetree, NULL on parsing errors.. the tree
 * of a parsed simple command looks like:
 *
 *      NODE_COMMAND
 *        |---> NODE_VAR
 *        |---> NODE_VAR
 *        +---> ...
 */
struct node_s *parse_simple_command(struct token_s *tok)
{
    int istest = 0;
    int wstart = 0;
    struct node_s *word, *last, *first = NULL;
    
    
    
    struct node_s *list = NULL, *root = NULL;
    
    /* save the current and previous token pointers */
    struct token_s *old_current_token = dup_token(get_current_token());
    struct token_s *old_previous_token = dup_token(get_previous_token());
    
    int do_aliases = interactive_shell || optionx_set(OPTION_EXPAND_ALIASES);
    int free_cmdline = 0;
    char *cmd_line, *p;
    
    struct source_s *src = tok->src;
    src->curpos = (src->curpos_old < 0) ? 0 : src->curpos_old;
    src->curchar = tok->charno;
    src->curlinestart = tok->linestart;
    while(isspace(src->buffer[src->curpos]))
    {
        src->curpos++;
    }

    /* this will update the source pointers */
    size_t len = read_complete_line(src, &cmd_line);
    if(len == 0)
    {
        return NULL;
    }
    
    if(do_aliases)
    {
        p = do_alias_substitution(cmd_line);
        
        if(p != cmd_line)
        {
            free_malloced_str(cmd_line);
            free_cmdline = 1;
        }
        
        if(!p)
        {
            return NULL;
        }

        cmd_line = p;
        len = strlen(cmd_line);
    }
    
    /* declare a dummy source so we can call tokenize() */
    struct source_s src2;
    src2.buffer = cmd_line;
    src2.bufsize = len;
    src2.srctype = SOURCE_EXTERNAL_FILE;
    src2.curpos = INIT_SRC_POS;

    tok = tokenize(&src2);
    
    /* create a new node for the command */
    struct node_s *cmd = new_node(NODE_COMMAND);
    if(!cmd)
    {
        return NULL;
    }
    cmd->lineno = tok->lineno;
    
    
loop:
    /* parse the command */
    last = last_child(cmd);
    do
    {
        /*
         * ignore redirection operators in the test command (anything that
         * falls between [[ and ]], or [ and ], as they can indicate
         * arithmetic and comparison operators.
         */
        if(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
        {
            if(!istest)
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
                        if(root)
                        {
                            free_node_tree(root);
                        }
                        root = NULL;
                        goto fin;
                    }
                }

                tok = get_current_token();
                continue;
            }
        }
        else if(is_separator_tok(tok->type))
        {
            /* finish parsing the command, unless its a 'test' command */
            if(!istest)
            {
                break;
            }

            /* the test command accepts !, &&, ||, (, ) */
            if(tok->type != TOKEN_KEYWORD_BANG && tok->type != TOKEN_AND_IF && 
               tok->type != TOKEN_OR_IF && tok->type != TOKEN_RIGHT_PAREN &&
               tok->type != TOKEN_LEFT_PAREN)
            {
                break;
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
                last = assign;
            }
            
            tok = tokenize(&src2);
            continue;
        }
        
        word = new_node(NODE_VAR);
        if(!word)
        {
            /* free the partially parsed nodetree */
            free_node_tree(cmd);
            return NULL;
        }
        set_node_val_str(word, tok->text);
        word->lineno = tok->lineno;
        add_child_node(cmd, word);
        
        if(!first)
        {
            first = word;
            /* check if this is a test or '[[' command */
            istest = (strcmp(word->val.str, "[[") == 0) ? 2 :
                     (strcmp(word->val.str, "[" ) == 0) ? 1 : 0;
#if 0
            /*
             * check if this is a test or '[[' command, and if so, add 
             * everything up to the closing ']]' or ']' without much fuss.
             */
            p = word->val.str;
            if(*p == '[' && (p[1] == '\0' || (p[1] == '[' && p[2] == '\0')))
            {
                char *p2 = (p[1]) ? "]]" : "]";
                tok = tokenize(&src2);

                char *start, *end = src2.buffer + src2.curpos;
                p = end;

                while(next_cmd_word(&start, &end))
                {
                    /* add the word to the command's AST */
                    word = new_node(NODE_VAR);
                    if(!word)
                    {
                        free_node_tree(cmd);
                        return NULL;
                    }
                    word->val_type = VAL_STR;
                    word->val.str = get_malloced_strl(start, 0, end-start);
                    word->lineno = tok->lineno;
                    add_child_node(cmd, word);
                    
                    if(strcmp(word->val.str, p2) == 0)
                    {
                        break;
                    }
                }
                
                src2.curpos += end-p-1;
                debug ("src2 = '%s'\n", src2.buffer + src2.curpos);
#endif
        }
        else if(istest == 2)
        {
            /* 
             * the string comparison operators, when used with the '[[' test
             * command, expect pattern strings.. we manually extract the
             * string, as tokenize() might mess it up.
             */
            p = tok->text;
            if(strcmp(p, "==") == 0 || strcmp(p, "!=") == 0 ||
               strcmp(p, "=~") == 0 || strcmp(p, "=" ) == 0)
            {
                //tok = tokenize(&src2);

                char *start, *end = src2.buffer + src2.curpos + 1;
                p = end;

                if(next_cmd_word(&start, &end, 1))
                {
                    /* add the word to the command's AST */
                    word = new_node(NODE_VAR);
                    if(!word)
                    {
                        free_node_tree(cmd);
                        return NULL;
                    }
                    word->val_type = VAL_STR;
                    word->val.str = get_malloced_strl(start, 0, end-start);
                    word->lineno = tok->lineno;
                    add_child_node(cmd, word);
                
                    src2.curpos += end-p;
                }
            }
        }

        last = word;
        tok = tokenize(&src2);
        
        if(istest && word->val.str[0] == ']')
        {
            istest = 0;
        }
        
#if 0
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
#endif

    } while(tok->type != TOKEN_EOF);

    /* check if we have successfully parsed the command */
    if(cmd)
    {
        set_node_val_str(cmd, get_malloced_strl(cmd_line, wstart, src2.curpos-wstart));
        
        /*
         * because we performed alias substitution, the command line might contain
         * a sequential or asynchronous list(s) of commands, which bypassed the
         * parsing step at parse_list().. we mitigate this by parsing them here.
         * 
         * NOTE: this code only works for simple commands.. it should be updated to
         *       handle compound commands resulting from alias substitution.
         */
        if(tok->type == TOKEN_AND || tok->type == TOKEN_SEMI ||
           tok->type == TOKEN_NEWLINE || list)
        {
            /* create a new node for the list */
            struct node_s *list2 = new_node(NODE_LIST);
            if(!list2)
            {
                PRINT_ERROR("%s: insufficient memory\n", SHELL_NAME);
                if(root)
                {
                    free_node_tree(root);
                    root = NULL;
                }
                goto fin;
            }
            
            if(!root)
            {
                root = list2;
            }
            else
            {
                add_child_node(list, list2);
            }
            list = list2;
            
            /* remember whether its a sequential (ending in ;) or asynchronous (ending in &) list */
            set_node_val_chr(list2, tok->type == TOKEN_AND ? '&' : ';');
            add_child_node(list2, cmd);
            list2->lineno = cmd->lineno;
            
            /* check the next command in this list */
            wstart = src2.curpos+1;
            tok = tokenize(&src2);
            if(tok->type != TOKEN_EOF)
            {
                cmd = new_node(NODE_COMMAND);
                if(!cmd)
                {
                    PRINT_ERROR("%s: insufficient memory\n", SHELL_NAME);
                    free_node_tree(root);
                    root = NULL;
                    goto fin;
                }
                goto loop;
            }
        }
        else if(!root)
        {
            root = cmd;
        }
        else
        {
            add_child_node(list, cmd);
        }
    }
    
fin:
    if(free_cmdline)
    {
        free(cmd_line);
    }
    else
    {
        free_malloced_str(cmd_line);
    }
    
    /* don't leave any hanging token structs */
    free_token(get_current_token());
    free_token(get_previous_token());

    /* restore token pointers */
    set_current_token(old_current_token);
    set_previous_token(old_previous_token);

    /* make sure our cur_token is synced to the new src position */
    tok = tokenize(src);
    
    /* return the parsed nodetree */
    return root;
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
        tok = tokenize(tok->src);
    }

    /* reached EOF or error getting the next token */
    if(tok->type == TOKEN_EOF)
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
        
        /* skip the 'time' keyword */
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);
        
        /*
         * if nothing comes after the 'time' keyword, return it as the backend
         * will still execute the time command.
         */
        if(tok->type == TOKEN_EOF)
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
    else if(tok->type == TOKEN_KEYWORD_COPROC)
    {
        /*
         * if we have a compound command, or a simple command with more than
         * one word, add the command to our AST and return.. otherwise, check
         * to see if the command (which must now be a simple command with one
         * word) is followed by another command on the same line as the proc
         * keyword (this means the command is actually the NAME we should give
         * to the coprocess, and thus we should read and parse another command).
         */
        char *start, *end = tok->src->buffer + tok->src->curpos + 1;
        char *p, *p2, *name_str = NULL;
        struct node_s *name_node = NULL;

        if(next_cmd_word(&start, &end, 1))
        {
            name_str = get_malloced_strl(start, 0, end-start);
        }
        
        if(!is_compound_keyword_str(name_str))
        {
            p2 = end;
            while(*p2)
            {
                if(*p2 == '\n' || !isspace(*p2))
                {
                    break;
                }
                p2++;
            }
            
            /* 
             * if there is another word, check it's type: if it's a compound 
             * command word, the first word is the coprocess name, and 
             * what follows is the compound command to be executed.
             * otherwise, the first word is the command name of a simple
             * command, and what follows are the command arguments.
             */
            if(*p2 && *p2 != '\n' && next_cmd_word(&start, &end, 1))
            {
                p = get_malloced_strl(start, 0, end-start);
                if(is_compound_keyword_str(p))
                {
                    /* add the name word to the command's AST */
                    name_node = new_node(NODE_VAR);
                    if(!name_node)
                    {
                        free_malloced_str(name_str);
                        return NULL;
                    }
                    name_node->val_type = VAL_STR;
                    name_node->val.str = name_str;
                    name_node->lineno = tok->lineno;
                    tok = tokenize(tok->src);
                }
                free_malloced_str(p);
            }
            else
            {
                free_malloced_str(name_str);
            }
        }
        else
        {
            free_malloced_str(name_str);
        }
        
        /* skip the next word */
        tok->src->wstart = tok->src->curpos;
        tok = tokenize(tok->src);

        /* parse the coproc command */
        struct node_s *cmd = parse_command(tok);
        if(!cmd)
        {
            PRINT_ERROR("%s: unexpected token: end-of-file\n", SHELL_NAME);
            if(name_node)
            {
                free_node_tree(name_node);
            }
            return NULL;
        }

        /* create a special node for the coproc'd command */
        struct node_s *t = new_node(NODE_COPROC);
        if(!t)
        {
            free_node_tree(cmd);
            if(name_node)
            {
                free_node_tree(name_node);
            }
            return NULL;
        }

        add_child_node(t, cmd);
        if(name_node)
        {
            add_child_node(t, name_node);
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

    /* save the current source pointers, in case we needed to rewind */
    struct source_s src2;
    memcpy(&src2, tok2->src, sizeof(struct source_s));
    
    /* skip the current token so we can check the token after it */
    tok = tokenize(tok->src);
    
    /* 
     * alternative, non-POSIX function definition using the
     * 'function' keyword.
     */
    if(tok2->type == TOKEN_KEYWORD_FUNCTION && !option_set('P'))
    {
        /* get rid of the (optional) opening brace */
        free(tok2);
        tok2 = dup_token(tok);
        if(!tok2)
        {
            return NULL;
        }
        tok = tokenize(tok->src);
        /* parse the function definition */
        struct node_s *func = parse_function_definition(tok2, 1);
        free_token(tok2);
        /* return the parsed nodetree */
        return func;
    }
    /* POSIX way of defining functions */
    else if(is_name(tok2->text) && !is_special_builtin(tok2->text) &&
       tok->type != TOKEN_EOF && tok->type == TOKEN_LEFT_PAREN)
    {
        /* parse the function definition */
        struct node_s *func = parse_function_definition(tok2, 0);
        free_token(tok2);
        /* return the parsed nodetree */
        return func;
    }
    
    /* restore the source pointers */
    free_token(tok);
    set_current_token(get_previous_token());
    set_previous_token(NULL);
    memcpy(tok2->src, &src2, sizeof(struct source_s));
    
    /* we have a simple command */
    struct node_s *cmd = parse_simple_command(tok2);
    
    /* return the parsed nodetree */
    return cmd;
}
