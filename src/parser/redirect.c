/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: redirect.c
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
 * return a ready-made I/O redirection node. useful when parsing non-POSIX
 * operators, such as '|&', which equates to '2>&1 |'. in this case, the pipe
 * is handled normally, but the implicit redirection needs an additional node,
 * which this function provides.
 * 
 * arguments:
 *   fd      : file descriptor of redirected file (0, 1, 2, ...).
 *   type    : node type, such as IO_FILE_GREAT, IO_FILE_LESSGREAT, ...
 *   namestr : string containing the part following the redirection operator,
 *             i.e. file path or file descriptor.
 *   lineno  : the source file line number to assign to the new node.
 */
struct node_s *io_file_node(int fd, char type, char *namestr, int lineno)
{
    /* create a new redirection node */
    struct node_s *io = new_node(NODE_IO_REDIRECT);
    if(!io)
    {
        return NULL;
    }
    io->lineno = lineno;
    /* set the node's file descriptor number */
    set_node_val_sint(io, fd);
    /* create a new file node */
    struct node_s *file = new_node(NODE_IO_FILE);
    if(!file)
    {
        free_node_tree(io);
        return NULL;
    }
    file->lineno = lineno;
    /* set the node's redirection operator and add it to the parent node */
    set_node_val_chr(file, type);
    add_child_node(io, file);
    /* create a new node for the path */
    struct node_s *name = new_node(NODE_VAR);
    if(!name)
    {
        free_node_tree(io);
        return NULL;
    }
    /* save the path and add it to the file node */
    set_node_val_str(name, namestr);
    name->lineno = lineno;
    add_child_node(file, name);
    /* return the redirection node */
    return io;
}


/*
 * parse an I/O file redirection.. the passed token contains the redirection
 * operator, which determines if the redirection is going to be an input, output
 * or append operation.
 * 
 * returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_io_file(struct token_s *tok)
{
    /* create a new node for the redirection */
    struct node_s *file = new_node(NODE_IO_FILE);
    if(!file)
    {
        return NULL;
    }
    file->lineno = tok->lineno;
    /* redirection operator starts with '<' */
    if(tok->text[0] == '<')
    {
        switch(tok->text[1])
        {
            case '\0':      /* '<' */
                set_node_val_chr(file, IO_FILE_LESS     );
                break;
                
            case  '&':      /* '<&' */
                set_node_val_chr(file, IO_FILE_LESSAND  );
                break;
                
            case  '>':      /* '<>' */
                set_node_val_chr(file, IO_FILE_LESSGREAT);
                break;
        }
    }
    /* redirection operator starts with '>' */
    else if(tok->text[0] == '>')
    {
        switch(tok->text[1])
        {
            case  '!':      /* zsh extension '>!', equivalent to >| */
            case  '|':            /* '>|' */
                set_node_val_chr(file, IO_FILE_CLOBBER  );
                break;
                
            case '\0':            /* '>' */
                set_node_val_chr(file, IO_FILE_GREAT    );
                break;
                
            case  '&':            /* '>&' */
                set_node_val_chr(file, IO_FILE_GREATAND );
                break;
                
            case  '>':            /* '>>' */
                set_node_val_chr(file, IO_FILE_DGREAT   );
                break;
        }
    }
    /* redirection operator starts with '&' */
    else if(tok->text[0] == '&')
    {
        if(strcmp(tok->text, "&>>") == 0)                   /* append stdout/stderr   */
        {
            set_node_val_chr(file, IO_FILE_AND_GREAT_GREAT);
        }
        else if(strcmp(tok->text, "&>") == 0)               /* redirect stdout/stderr */
        {
            set_node_val_chr(file, IO_FILE_GREATAND);       /* treat '&>' as '>&'     */
        }
        else if(strcmp(tok->text, "&<") == 0)
        {
            set_node_val_chr(file, IO_FILE_LESSAND );       /* treat '&<' as '<&'     */
        }
    }
    /* skip the redirection operator token */
    tok = tokenize(tok->src);
    /* create a new node for the file descriptor/path that follows the operator */
    struct node_s *name = new_node(NODE_VAR);
    if(!name)       /* insufficient memory */
    {
        free_node_tree(file);
        return NULL;
    }
    set_node_val_str(name, tok->text);
    name->lineno = tok->lineno;
    add_child_node(file, name);
    /* skip the file descriptor/path token */
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
            fprintf(stderr, "%s: cannot redirect output to file `%s`: restricted shell\n", SHELL_NAME, name->val.str);
            free_node_tree(file);
            return NULL;
        }
    }
    /* return the redirection nodetree */
    return file;
}


/*
 * parse a here-string redirection.. the passed token contains the redirection
 * operator, which should be '<<<', non-POSIX extension).
 * 
 * returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_io_herestr(struct token_s *tok)
{
    /* skip any optional spaces before the here-string */
    char *cmd = tok->src->buffer+tok->src->curpos+1;
    while(*cmd && isspace(*cmd) && *cmd != '\n')
    {
        cmd++;
        src->curpos++;
    }
    /* empty here-string */
    if(!*cmd || *cmd == '\n')
    {
        parser_err = 1;
        return NULL;
    }
    /* get the end of the here-string */
    size_t len;
    char *end = cmd;
    while(*end && *end != '\n')
    {
        end++;
    }
    len = end - cmd;
    /* create a new node for the redirection */
    struct node_s *file = new_node(NODE_IO_HERE);
    if(!file)       /* insufficient memory */
    {
        parser_err = 1;
        return NULL;
    }
    file->lineno = tok->lineno;
    /* we word-expand here-strings by default */
    set_node_val_chr(file, IO_HERE_EXPAND);
    /* update the src struct pointer to point past the here-string */
    src->curpos += len;
    src->curlinestart = src->curpos;
    src->curchar = 1;
    src->curline++;
    /* make sure our cur_token is synced to the new src position */
    tokenize(tok->src);
    /* create a new node for the here-string body */
    struct node_s *here = new_node(NODE_VAR);
    if(!here)       /* insufficient memory */
    {
        parser_err = 1;
        free_node_tree(file);
        return NULL;
    }
    here->val.str = get_malloced_strl(cmd, 0, len+1);
    add_child_node(file, here);
    /* return the here-string nodetree */
    return file;
}


/*
 * parse a here-document redirection.. the passed token contains the redirection
 * operator, which determines if the redirection is from a here-document (operator
 * is '<<' or '<<-', both POSIX-defined) or from a here-string (operator is '<<<',
 * non-POSIX extension).
 * 
 * returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_io_here(struct token_s *tok)
{
    /* is it a here-string redirection? */
    if(tok->text[2] == '<')     /* operator is '<<<' */
    {
        return parse_io_herestr(tok);
    }
    /* is it a stripped here-doc redirection? */
    int strip = 0;
    if(tok->text[2] == '-')     /* operator is '<<-' */
    {
        strip = 1;
    }
    /* create a new node for the redirection */
    struct node_s *file = new_node(NODE_IO_HERE);
    if(!file)       /* insufficient memory */
    {
        parser_err = 1;
        return NULL;
    }
    file->lineno = tok->lineno;
    /*****************************************
     * NOTE: brute-force extract the heredoc
     * TODO: maybe find a better workaround?
     *****************************************/
    struct source_s  *src     = tok->src;
    struct word_s *heredoc = get_heredoc(src, src->buffer+src->curpos+1, strip);
    /* error getting the here-document body */
    if(!heredoc)
    {
        parser_err = 1;
        free_node_tree(file);
        return NULL;
    }
    /*
     * check if the here-doc should be word-expanded or not (depends on whether the
     * here-doc word was quoted or not), and set the redirection node flags accordingly.
     */
    if(flag_set(heredoc->flags, HEREDOC_TOKEN_EXP))
    {
        set_node_val_chr(file, IO_HERE_EXPAND);
    }
    else
    {
        set_node_val_chr(file, IO_HERE_NOEXPAND);
    }
    /* create a new node for the here-doc body */
    struct node_s *here = new_node(NODE_VAR);
    if(!here)
    {
        parser_err = 1;
        free_node_tree(file);
        free_all_words(heredoc);
        return NULL;
    }
    /* store the here-doc body */
    here->val.str = get_malloced_str(heredoc->data);
    add_child_node(file, here);
    free_all_words(heredoc);
    src->bufsize = strlen(src->buffer);
    /* return the here-doc nodetree */
    return file;
}


/*
 * parse a file or here-document redirection.. the passed token contains either the
 * redirected file descriptor, or the redirection operator, which determines if the
 * redirection is from a here-document, a here-string, or a regular file.
 * 
 * returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_io_redirect(struct token_s *tok)
{
    /* create a new node for the redirection */
    struct node_s *io = new_node(NODE_IO_REDIRECT);
    if(!io)
    {
        return NULL;
    }
    io->lineno = tok->lineno;
    /* 
     * if the next token is a number, it is the redirected file descriptor.
     * save it in the node and skip over it to get the redirection operator.
     */
    if(tok->type == TOKEN_IO_NUMBER)
    {
        set_node_val_sint(io, atoi(tok->text));
        tok = tokenize(tok->src);
    }
    else
    {
        /*
         * without a given file descriptor, decide the file descriptor or the
         * redirected stream by checking the type of operator at hand.. an operator
         * starting with '<' is an input redirection, so we use file descriptor 0..
         * otherwise, use file descriptor 1 for output redirection.
         */
        if(tok->text[0] == '<')
        {
            set_node_val_sint(io, 0);
        }
        else
        {
            set_node_val_sint(io, 1);
        }
    }
    /* parse as file or here-document redirection depending on the operator */
    struct node_s *file = ((tok->text[0] == '<') && (tok->text[1] == '<')) ?
                            parse_io_here(tok) :    /* '<<', '<<-', '<<<' operators */
                            parse_io_file(tok);     /* all other operators */
    if(file)
    {
        add_child_node(io, file);
        /* return the redirection nodetree */
        return io;
    }
    else
    {
        /* error parsing the redirection */
        free_node_tree(io);
        parser_err = 1;
        return NULL;
    }
}


/*
 * check if the given str is a redirection operator.
 * 
 * returns 1 if str is a redirection operator, 0 otherwise.
 */
int is_redirect_op(char *str)
{
    if(!str)
    {
        return 0;
    }
    if(*str == '>' || *str == '<')
    {
        return 1;
    }
    if(*str == '&' && str[1] == '>')
    {
        return 1;
    }
    return 0;
}


/*
 * parse a redirection list, which consists of one or more file or here-document
 * redirections.. the passed token contains either the redirected file descriptor,
 * or the redirection operator, which determines if the redirection is from a
 * here-document, a here-string, or a regular file.
 * 
 * returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_redirect_list(struct token_s *tok)
{
    /* reached EOF or error */
    if(tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR)
    {
        return NULL;
    }
    /* not a redirection token */
    if(!is_redirect_op(tok->text) && tok->type != TOKEN_IO_NUMBER)
    {
        return NULL;
    }
    /* create a new node for the redirection list */
    struct node_s *io = new_node(NODE_IO_REDIRECT_LIST);
    if(!io)
    {
        return NULL;
    }
    io->lineno = tok->lineno;
    /*
     * parse I/O redirections as long as we have a redirection operator or a numeric
     * token followed by a redirection operator.
     */
    while(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
    {
        struct node_s *item = parse_io_redirect(tok);
        if(item)
        {
            add_child_node(io, item);
        }
        tok = get_current_token();
    }
    /* return the redirection list nodetree */
    return io;
}


/*
 * extract a heredoc body text.. the *_cmd parameter points to the start
 * of the heredoc body in the source buffer, while the strip parameter
 * tells us if we should strip leading tabs from heredoc lines or not.
 * 
 * returns a node representing the parsed here-doc, or NULL on parsing errors.
 * the curpos field of src is updated to point past the heredoc body.
 */
struct word_s *get_heredoc(struct source_s *src, char *_cmd, int strip)
{
    if(!_cmd)
    {
        return NULL;
    }
    /*
     * skip any optional spaces before the heredoc delimiter word.. while this behavior
     * is non-POSIX, it is quite common, and so we have to deal with it (in POSIX,
     * the heredoc delimiter word and the heredoc redirection operator (<< or <<-)
     * should have no spaces between them).
     */
    while(*_cmd && isspace(*_cmd) && *_cmd != '\n')
    {
        _cmd++;
        src->curpos++;
    }
    char *cmd   = _cmd;
    char delim[256];        /* heredoc delimiter word */
    int  expand = 1;        /* expand by default, unless delimiter is quoted */
    int  heredoc_len = 0;   /* length of heredoc body text */
    /* 
     * find the end of the delimiter word by skipping all non-space chars, semi-colons
     * and ampersands.
     */
    while(*cmd && !isspace(*cmd) && *cmd != ';' && *cmd != '&')
    {
        cmd++;
    }
    /* empty delimiter word */
    if(cmd == _cmd)
    {
        delim[0] = '\0';
    }
    else
    {
        /* copy delimiter */
        char *c1 = _cmd;
        char *c2 = delim;
        while(c1 < cmd)
        {
            if(*c1 == '"' || *c1 == '\'')
            {
                /* word has single or double quotes. skip the quote char */
                c1++;
                /* we will not expand this heredoc */
                expand = 0;
            }
            else
            {
                /* word has a backslash-quoted character. skip the backslash char */
                if(*c1 == '\\')
                {
                    /* we will not expand this heredoc */
                    expand = 0;
                    c1++;
                }
                /* copy the next char */
                *c2++ = *c1++;
            }
        }
        /* terminate the copied word with newline and NULL */
        *c2++ = '\n';
        *c2   = '\0';
        /* now remove the word from the original command string */
        /*
        c2 = _cmd;
        c1 = cmd;
        while((*c2++ = *c1++))
        {
            ;
        }
        */
    }
    /* get the next newline char */
    char *nl = strchr(_cmd, '\n');
    char *end;
    /* missing newline char */
    if(!nl)
    {
        BACKEND_RAISE_ERROR(HEREDOC_MISSING_NEWLINE, NULL, NULL);
        return NULL;
    }
    /* empty heredoc delimiter word */
    if(delim[0] == '\0')
    {
        BACKEND_RAISE_ERROR(HEREDOC_EXPECTED_DELIM, NULL, NULL);
        return NULL;
    }
    else
    {
        /*
         * get the heredoc length.. the body ends with the first occurrence of the delimiter
         * word that's followed by a newline char.
         */
        end = strstr(nl, delim);
        if(!end)
        {
            /* remove the newline char to print delimiter nicely */
            delim[cmd - _cmd] = '\0';
            BACKEND_RAISE_ERROR(HEREDOC_EXPECTED_DELIM, delim, NULL);
            return NULL;
        }
        heredoc_len = end - nl;
    }
    /* now get the heredoc proper */
    char *heredoc = (char *)malloc(heredoc_len+1);
    if(!heredoc)        /* insufficient memory */
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "storing here-document", NULL);
        return NULL;
    }
    /* copy the heredoc body */
    char *p1 = nl+1;
    char *p2 = heredoc;
    do
    {
        if(*p1 == '\n')
        {
            /* strip tabs at the beginning of each line */
            if(strip)
            {
                while(*p1 == '\t')
                {
                    p1++;
                }
            }
        }
        *p2++ = *p1++;
    } while(p1 < end);
    /* NULL-terminate the copied string */
    *p2 = '\0';
    p1 = end;
    p1 += strlen(delim);
    /*
    p2 = nl+1;
    while((*p2++ = *p1++))
    {
        ;
    }
    */
    /* update the src struct pointer to point past the heredoc body */
    src->curpos += (p1 - _cmd);
    src->curlinestart = src->curpos;
    src->curchar = 1;
    src->curline++;
    /* make sure our cur_token is synced to the new src position */
    tokenize(src);
    /* save the heredoc body */
    struct word_s *t = (struct word_s *)malloc(sizeof(struct word_s));
    if(!t)      /* insufficient memory */
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "parsing heredoc", NULL);
        free(heredoc);
        return NULL;
    }
    t->data = heredoc;
    t->len  = strlen(heredoc);
    /* remember if we'll need to word-expand the heredoc body later on */
    if(expand)
    {
        t->flags |= HEREDOC_TOKEN_EXP;
    }
    else
    {
        t->flags |= HEREDOC_TOKEN_NOEXP;
    }
    /* return the heredocument */
    return t;
}
