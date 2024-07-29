/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: heredoc.c
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
#include "include/cmd.h"
#include "scanner/scanner.h"
#include "parser/node.h"
#include "parser/parser.h"
#include "error/error.h"
#include "include/debug.h"

/* max allowed length for heredoc delimiter words */
#define MAX_DELIM_LEN       512


/**********************************************************************
 * Functions used by the front-end parser and the back-end executor to
 * parse and execute here-document and here-string I/O redirections.
 **********************************************************************/


/*
 * Extract a heredoc body text. The *orig_cmd parameter points to the start
 * of the heredoc body in the source buffer, while the strip parameter
 * tells us if we should strip leading tabs from heredoc lines or not.
 * 
 * Returns a node representing the parsed here-doc, or NULL on parsing errors.
 * The curpos field of src is updated to point past the heredoc body.
 */
char *get_heredoc(char *start, char *end, int strip)
{
    int heredoc_len = end - start;

    /* now get the heredoc proper */
    char *heredoc = malloc(heredoc_len+1);
    if(!heredoc)        /* insufficient memory */
    {
        INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "storing here-document");
        return NULL;
    }

    /* copy the heredoc body */
    char *p1 = start;
    char *p2 = heredoc;

    /* strip tabs at the beginning of the first line */
    if(strip)
    {
        while(*p1 == '\t')
        {
            p1++;
        }
    }

    /* now copy the heredoc body */
    while(p1 < end)
    {
        *p2++ = *p1++;
        if(p1[-1] == '\n')
        {
            /* strip tabs at the beginning of each subsequent line */
            if(strip)
            {
                while(*p1 == '\t')
                {
                    p1++;
                }
            }
        }
    }

    /* NULL-terminate the copied string */
    *p2 = '\0';
    
    /* return the here-document */
    return heredoc;
}


/*
 * Get the end of a here-string by skipping all characters upto the first 
 * newline character.
 * 
 * Returns a pointer to the first character after the herestring (ideally a 
 * newline or '\0'), or NULL in case of an empty here-string.
 */
char *herestr_end(char *cmd)
{
    char *start, *end = cmd;
    if(!next_cmd_word(&start, &end, 1))
    {
        return NULL;
    }
    
    return end;
    
#if 0
    /* skip any optional spaces before the here-string */
    while(isspace(*cmd) && *cmd != '\n')
    {
        cmd++;
    }

    /* empty here-string */
    if(!*cmd || *cmd == '\n')
    {
        return NULL;
    }
    
    /* get the end of the here-string */
    while(*cmd && *cmd != '\n')
    {
        /* here-string terminated by whitespace or # */
        if(isspace(*cmd) || *cmd == '#')
        {
            break;
        }
        
        cmd++;
    }
    
    return cmd;
#endif
}


/*
 * If we have nested here-documents, find the ending of each one of them and
 * return a pointer to the last char in the last heredoc. 'start' points to
 * the first char of the first heredoc, 'heredoc_count' is the documents' count,
 * and 'heredoc_delims' contains the delimiter words for each here-document.
 */
char *last_heredoc_end(char *start, int heredoc_count, char **heredoc_delims,
                       char last_char)
{
    int j, k = heredoc_count-1;
    for(j = 0; j < heredoc_count; j++)
    {
        char *delim = heredoc_delims[j];
        /* only the last heredoc might end in last_char */
        start = heredoc_end(start, delim, (j == k) ? last_char : 0);
        if(!start)
        {
            return NULL;
        }

        /* skip to the first newline (or delimiter) char after the heredoc body */
        while(*start && *start != '\n' && *start != last_char)
        {
            /* skip possible \\n in the heredoc delimiter */
            if(*start == '\\')
            {
                /* beware of a hanging slash */
                if(!*++start)
                {
                    break;
                }
            }
            
            start++;
        }
    }
    return start;
}


/*
 * Find the end of a here-document, given the beginning of the heredoc and the 
 * delimiter word. If last_char is non-zero, it contains the character that can occur
 * after the ending delimiter word (instead of \n or \0). This is useful when we
 * are parsing a here-document that's part of command substitution, for example.
 * 
 * Returns a pointer to the first char after the here-document's body (which is
 * the first char of the ending delimiter word), or NULL in case of error.
 */
char *heredoc_end(char *start, char *delim, char last_char)
{
    if(*start == '\n')
    {
        start++;
    }

    /* make a word and perform quote removal */
    struct word_s *word = make_word(delim);
    if(!word)
    {
        INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "heredoc parsing");
        return NULL;
    }
    remove_quotes(word);

    char *delim_start, *delim_end = NULL, *end = NULL;
    int delim_len, quoted;
    char *delim_word = word->data;

    /* empty heredoc delimiter word */
    if(!delim_word || *delim_word == '\0')
    {
        PRINT_ERROR(SHELL_NAME, "expected heredoc delimiter");
        free_all_words(word);
        return NULL;
    }

    delim_len = strlen(delim_word);
    quoted = (flag_set(word->flags, FLAG_WORD_HAD_QUOTES) ||
              flag_set(word->flags, FLAG_WORD_HAD_DOUBLE_QUOTES));

    while(*start)
    {
        /*
         * Get the heredoc length. The body ends with the first occurrence of
         * the delimiter word that's followed by a newline char. If the 
         * original delimiter word was quoted, we simply search for the ending
         * delimiter word, otherwise we find the ending delimiter by processing
         * the document, char-by-char, to account for escaped chars, such as \\n.
         */
        char *p = start, *p2;
        if(quoted)
        {
            end = strstr(start, delim_word);
            if(end)
            {
                delim_start = end;
                delim_end = end + delim_len;
            }
            else
            {
                /* find the end of the string */
                while(*p)
                {
                    p++;
                }
            }
        }
        else
        {
            while(*p)
            {
                p2 = delim_word;
                delim_start = p;

                while(*p2)
                {
                    if(*p == '\\')
                    {
                        p++;
                        /* skip \\n */
                        if(*p == '\n')
                        {
                            p++;
                            /* continue from the top, in case there is another \\n */
                            continue;
                        }
                    }
                    
                    if(*p != *p2)
                    {
                        p++;
                        break;
                    }
                        
                    p++;
                    p2++;
                }
                
                /* if we reached the end of the delimiter word, we have a match */
                if(!*p2)
                {
                    end = delim_start;
                    delim_end = p;
                    break;
                }
            }
        }

        if(!delim_end)
        {
            start = p;
            break;
        }
        
        char c = *delim_end;
        if(c == '\n' || c == '\0' || c == last_char)
        {
            /*
             * The delimiter is followed by newline, or is the last word in input,
             * or is followed by the given char. Now check that its preceded by
             * newline, or that it has whitespace (but nothing else) before it on
             * the line.
             */
            int bailout = 0;
            char *c1 = delim_start;
            while(*(--c1) != '\n')
            {
                if(!isspace(*c1))
                {
                    bailout = 1;
                    break;
                }
            }
            
            if(!bailout)
            {
                break;
            }
        }
        
        start = delim_end;
        delim_end = NULL;
        end = NULL;
    }

    if(!end)
    {
        /* have we reached input's end? */
        if(!*start || *start == last_char)
        {
            PRINT_ERROR(SHELL_NAME, "heredoc delimited by EOF");
            end = start;
        }
        else
        {
            PRINT_ERROR(SHELL_NAME, "expected heredoc delimiter: %s", delim);
        }
    }
    
    free_all_words(word);
    return end;
}


/*
 * Get a here-document delimiter word, which typically comes after the << or <<-
 * operator. 'orig_cmd' is the first char after the operator. If any part of
 * the delimiter word is quoted, the heredoc should not be expanded, and we store
 * 0 in 'expand', otherwise we store 1. After getting the delimiter word, we
 * store a pointer to the word (after removing any quoted chars) in '__delim'.
 * As the final delimiter word might differ from what we found in input (due to
 * quoted chars, \\n sequences, etc), we store a pointer to the last char in the
 * input's delimiter word in '__delim_end', so that our caller knows where to
 * continue processing input from.
 */
int heredoc_delim(char *orig_cmd, int *expand, char **__delim, char **__delim_end)
{
    /*
     * Skip any optional spaces before the heredoc delimiter word. While this behavior
     * is non-POSIX, it is quite common, and so we have to deal with it (in POSIX,
     * the heredoc delimiter word and the heredoc redirection operator (<< or <<-)
     * should have no spaces between them).
     */
    while(isspace(*orig_cmd) && *orig_cmd != '\n')
    {
        orig_cmd++;
    }

    char buf[MAX_DELIM_LEN];        /* heredoc delimiter word */
    char *delim;
    int chars = 0;
    //int delim_len = 0;
    char *c1, *c2;
    (*expand) = 1;          /* expand by default, unless delimiter is quoted */

    /* copy delimiter */
    c1 = orig_cmd;
    c2 = buf;
    while(*c1)
    {
        /* make sure we don't overflow our buffer */
        if(chars == MAX_DELIM_LEN)
        {
            PRINT_ERROR(SHELL_NAME, "heredoc delimiter too long (max length %d)",
                        MAX_DELIM_LEN);
            return 0;
        }
            
        /* 
         * Delimiter ends at the first whitespace char (POSIX).
         * As an extension, end the delimiter at the first control char,
         * i.e. ;|& (non-POSIX).
         */
        if(isspace(*c1) || *c1 == ';' || *c1 == '|' || *c1 == '&')
        {
            break;
        }
        else if(*c1 == '"' || *c1 == '\'')
        {
            /* add quoted substring as-is (we'll do quote removal later) */
            size_t i = find_closing_quote(c1, 0, 0);
            if(i)
            {
                /* we will not expand this heredoc */
                (*expand) = 0;
                char *c3 = orig_cmd+i;
                while(c1 <= c3)
                {
                    *c2++ = *c1++;
                    chars++;
                }
            }
            else
            {
                /* isolated quote char, add it and move on */
                *c2++ = *c1++;
                chars++;
            }
        }
        else
        {
            /* word has a backslash-quoted character. skip the backslash char */
            if(*c1 == '\\')
            {
                /* we will not expand this heredoc */
                (*expand) = 0;
                c1++;
                
                /* skip the \\n combination */
                if(*c1 == '\n')
                {
                    c1++;
                    continue;
                }
            }
            
            /* copy the next char */
            *c2++ = *c1++;
            chars++;
        }
    }

    *c2 = '\0';
    delim = buf;

    /* empty heredoc delimiter word */
    if(*delim == '\0')
    {
        PRINT_ERROR(SHELL_NAME, "expected heredoc delimiter");
        return 0;
    }

    (*__delim) = get_malloced_str(delim);
    (*__delim_end) = c1;
    
    return 1;
}


/*
 * Extract the text of 'heredoc_count' number of here-documents from the input
 * source pointed to by 'src'. For each extracted heredoc, we search the node
 * representing the heredoc in the command's parse tree, which is passed in 'cmd',
 * and we add the heredoc's body as the first child node of the node NODE_IO_HERE
 * of the here-document (the second node contains the heredoc's delimiter word).
 * 
 * Returns 1 if we got 'heredoc_count' of heredoc bodies, 0 in case of error.
 */
int extract_heredocs(struct source_s *src, struct node_s *cmd, int heredoc_count)
{
    int j;
    struct node_s *here = cmd->first_child, *child;
    char *p = src->buffer + src->curpos + 1;

    for(j = 0; j < heredoc_count; j++)
    {
        char *delim = NULL;
        child = NULL;
        
        if(*p == '\n')
        {
            p++;
        }
        
        while(here)
        {
            enum node_type_e type = here->type;
            child = here->first_child;
            here = here->next_sibling;
            if(type == NODE_IO_REDIRECT &&
               child && child->type == NODE_IO_HERE)
            {
                delim = last_child(child)->val.str;
                break;
            }
        }
        
        if(!delim || !child)
        {
            return 0;
        }

        char *p2 = heredoc_end(p, delim, 0);
        if(!p2)
        {
            return 0;
        }

        int strip = 0;
        if(child->val.chr == IO_HERE_STRIP_EXPAND ||
           child->val.chr == IO_HERE_STRIP_NOEXPAND)
        {
            strip = 1;
            child->val.chr = (child->val.chr == IO_HERE_STRIP_EXPAND) ?
                           IO_HERE_EXPAND : IO_HERE_NOEXPAND;
        }

        char *body = get_heredoc(p, p2, strip);
        if(!body)
        {
            return 0;
        }

        child->first_child->val.str = get_malloced_str(body);
        child->first_child->val_type = VAL_STR;
        free(body);
        
        /* skip to the first newline char after the heredoc body */
        p = p2;
        while(*p && *p != '\n')
        {
            /* skip possible \\n in the heredoc delimiter */
            if(*p == '\\')
            {
                /* beware of a hanging slash */
                if(!*++p)
                {
                    break;
                }
            }
            
            p++;
        }
    }
    
    src->curpos = (long)(p - src->buffer - 1);
    src->curlinestart = src->curpos;
    src->curline++;
    src->curchar = 1;

    /* make sure our cur_token is synced to the new src position */
    tokenize(src);
    return 1;
}
