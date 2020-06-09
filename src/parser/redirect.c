/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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


/**********************************************
 * Functions used by the front-end parser to
 * collect I/O redirections from a command line.
 **********************************************/

/*
 * Return a ready-made I/O redirection node. useful when parsing non-POSIX
 * operators, such as '|&', which equates to '2>&1 |'. In this case, the pipe
 * is handled normally, but the implicit redirection needs an additional node,
 * which this function provides.
 * 
 * Arguments:
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
 * Parse an I/O file redirection. The passed token contains the redirection
 * operator, which determines if the redirection is going to be an input, output
 * or append operation.
 * 
 * Returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_file_redirect(struct token_s *tok)
{
    /* create a new node for the redirection */
    struct node_s *file = new_node(NODE_IO_FILE);
    if(!file)
    {
        return NULL;
    }
    file->lineno = tok->lineno;

    switch(tok->text[0])
    {
        /* redirection operator starts with '<' */
        case '<':
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
            break;

        /* redirection operator starts with '>' */
        case '>':
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
            break;

        /* redirection operator starts with '&' */
        case '&':
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
            break;
    }

    /* skip the redirection operator token */
    tok = tokenize(tok->src);

    /* reached EOF or error getting next token */
    if(tok->type == TOKEN_EOF || tok->text[0] == '\n')
    {
        /* free the partially parsed nodetree */
        PRINT_ERROR("%s: missing or invalid redirected filename\n", SOURCE_NAME);
        free_node_tree(file);
        return NULL;
    }

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
     * zsh says r-shell can't redirect output to files. If the token that comes after
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
            PRINT_ERROR("%s: cannot redirect output to file `%s`: restricted shell\n", 
                        SOURCE_NAME, name->val.str);
            free_node_tree(file);
            return NULL;
        }
    }

    /* return the redirection nodetree */
    return file;
}


/*
 * Parse a here-string redirection. The passed token contains the redirection
 * operator, which should be '<<<', non-POSIX extension).
 * 
 * Returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_herestr(struct token_s *tok)
{
    struct source_s *src = tok->src;

    /* skip any optional spaces before the here-string */
    char *cmd = src->buffer + src->curpos+1;
    
    char *start, *end = cmd;
    if(!next_cmd_word(&start, &end, 1))
    {
        parser_err = 1;
        return NULL;
    }

#if 0
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
#endif

    /* get the end of the here-string */
    size_t all_len = end - cmd;
    size_t word_len = end - start;
    
    /* create a new node for the redirection */
    struct node_s *file = new_node(NODE_IO_HERE);
    if(!file)       /* insufficient memory */
    {
        parser_err = 1;
        return NULL;
    }
    file->lineno = tok->lineno;

    /* we word-expand here-strings by default */
    set_node_val_chr(file, IO_HERE_STR);

    /* update the src struct pointer to point past the here-string */
    src->curpos += all_len;
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
    here->val_type = VAL_STR;
    here->val.str = get_malloced_strl(start, 0, word_len);
    add_child_node(file, here);

    /* return the here-string nodetree */
    return file;
}


/*
 * Parse a here-document redirection. The passed token contains the redirection
 * operator, which determines if the redirection is from a here-document (operator
 * is '<<' or '<<-', both POSIX-defined) or from a here-string (operator is '<<<',
 * non-POSIX extension).
 * 
 * Returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_heredoc_redirect(struct token_s *tok)
{
    /* is it a here-string redirection? */
    if(tok->text[2] == '<')     /* operator is '<<<' */
    {
        return parse_herestr(tok);
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
    struct source_s  *src  = tok->src;
    int expand = 0;

    int len = 0;
    char *delim, *delim_end;
    char *orig_cmd = src->buffer+src->curpos+1;
    if(!*orig_cmd)
    {
        parser_err = 1;
        free_node_tree(file);
        return NULL;
    }
    
    if(!heredoc_delim(orig_cmd, &expand, &delim, &delim_end))
    {
        parser_err = 1;
        free_node_tree(file);
        return NULL;
    }
    
    len = delim_end - orig_cmd;

    /*
     * check if the here-doc should be word-expanded or not (depends on whether the
     * here-doc word was quoted or not), and set the redirection node flags accordingly.
     */
    if(expand)
    {
        set_node_val_chr(file, strip ? IO_HERE_STRIP_EXPAND : IO_HERE_EXPAND);
    }
    else
    {
        set_node_val_chr(file, strip ? IO_HERE_STRIP_NOEXPAND : IO_HERE_NOEXPAND);
    }

    /* create a new node for the here-doc body */
    struct node_s *heredoc_node = new_node(NODE_VAR);
    if(!heredoc_node)
    {
        parser_err = 1;
        free_node_tree(file);
        return NULL;
    }

    /* store the here-doc body */
    add_child_node(file, heredoc_node);

    struct node_s *delim_node = new_node(NODE_VAR);
    if(!delim_node)
    {
        parser_err = 1;
        free_node_tree(file);
        return NULL;
    }

    /* store the here-doc body */
    add_child_node(file, delim_node);
    delim_node->val.str = delim;
    delim_node->val_type = VAL_STR;

    src->curchar += len;
    src->curpos  += len;

    /* make sure our cur_token is synced to the new src position */
    tokenize(src);
    
    /* return the here-doc nodetree */
    return file;
}


/*
 * Parse a file or here-document redirection. The passed token contains either the
 * redirected file descriptor, or the redirection operator, which determines if the
 * redirection is from a here-document, a here-string, or a regular file.
 * 
 * Returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_io_redirect(struct token_s *tok)
{
    /* create a new node for the redirection */
    struct node_s *io = new_node(NODE_IO_REDIRECT);
    if(!io)
    {
        parser_err = 1;
        return NULL;
    }
    io->lineno = tok->lineno;
    /* 
     * If the next token is a number, it is the redirected file descriptor.
     * Save it in the node and skip over it to get the redirection operator.
     */
    if(tok->type == TOKEN_IO_NUMBER)
    {
        set_node_val_sint(io, atoi(tok->text));
        tok = tokenize(tok->src);
    }
    else
    {
        /*
         * Without a given file descriptor, decide the file descriptor or the
         * redirected stream by checking the type of operator at hand. An operator
         * Starting with '<' is an input redirection, so we use file descriptor 0.
         * Otherwise, use file descriptor 1 for output redirection.
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
                            parse_heredoc_redirect(tok) :    /* '<<', '<<-', '<<<' operators */
                            parse_file_redirect(tok);     /* all other operators */
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
 * Check if the given str is a redirection operator.
 * 
 * Returns 1 if str is a redirection operator, 0 otherwise.
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
 * Parse a redirection list, which consists of one or more file or here-document
 * redirections. The passed token contains either the redirected file descriptor,
 * or the redirection operator, which determines if the redirection is from a
 * here-document, a here-string, or a regular file.
 * 
 * Returns a nodetree representing the I/O operation, NULL in case of error.
 */
struct node_s *parse_redirect_list(struct token_s *tok)
{
    /* reached EOF or error */
    if(tok->type == TOKEN_EOF)
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
    int heredoc_count = 0;
    while(is_redirect_op(tok->text) || tok->type == TOKEN_IO_NUMBER)
    {
        struct node_s *item = parse_io_redirect(tok);
        if(item)
        {
            add_child_node(io, item);
            if(item->first_child->type == NODE_IO_HERE)
            {
                heredoc_count++;
            }
        }
        tok = get_current_token();
    }

    if(heredoc_count)
    {
        if(!extract_heredocs(tok->src, io, heredoc_count))
        {
            free_node_tree(io);
            parser_err = 1;
            return NULL;
        }
    }

    /* make sure our cur_token is synced to the new src position */
    tok = get_current_token();

    /* return the redirection list nodetree */
    return io;
}
