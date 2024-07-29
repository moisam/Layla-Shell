/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: lexical.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "../include/cmd.h"
#include "keywords.h"
#include "scanner.h"
#include "../builtins/setx.h"
#include "../include/debug.h"

char *tok_buf = NULL;             /* the buffer we'll use while parsing a token */
int   tok_bufsize = 0;            /* the size of memory alloc'd for the buffer */
int   tok_bufindex = -1;          /* our current position in the buffer */

/* a pointer to the current token struct */
static struct token_s *cur_tok  = NULL;

/* a pointer to the previous token struct */
static struct token_s *prev_tok = NULL;

/* special token to indicate end of input */
struct token_s eof_token = 
{
    .type     = TOKEN_EOF,
    .lineno   = 0,
    .charno   = 0,
    .text_len = 0,
};

/*
 * Return the token type that describes one of the shell's keywords.
 * The keywords are stored in an array (defined in keywords.h) and the
 * index field gives the index of an item in the array. The value we
 * return is the token type describing the indexed keyword. If the
 * index is out of the keywords array bounds, we return TOKEN_KEYWORD_NA.
 */
enum token_type_e get_keyword_toktype(int index)
{
    switch(index)
    {
        case 0 : return TOKEN_KEYWORD_IF      ; break;
        case 1 : return TOKEN_KEYWORD_THEN    ; break;
        case 2 : return TOKEN_KEYWORD_ELSE    ; break;
        case 3 : return TOKEN_KEYWORD_ELIF    ; break;
        case 4 : return TOKEN_KEYWORD_FI      ; break;
        case 5 : return TOKEN_KEYWORD_DO      ; break;
        case 6 : return TOKEN_KEYWORD_DONE    ; break;
        case 7 : return TOKEN_KEYWORD_CASE    ; break;
        case 8 : return TOKEN_KEYWORD_ESAC    ; break;
        case 9 : return TOKEN_KEYWORD_WHILE   ; break;
        case 10: return TOKEN_KEYWORD_UNTIL   ; break;
        case 11: return TOKEN_KEYWORD_FOR     ; break;
        case 12: return TOKEN_KEYWORD_LBRACE  ; break;
        case 13: return TOKEN_KEYWORD_RBRACE  ; break;
        case 14: return TOKEN_KEYWORD_BANG    ; break;
        case 15: return TOKEN_KEYWORD_IN      ; break;
        case 16: return TOKEN_KEYWORD_SELECT  ; break;
        case 17: return TOKEN_KEYWORD_FUNCTION; break;
        case 18: return TOKEN_KEYWORD_TIME    ; break;
        case 19: return TOKEN_KEYWORD_COPROC  ; break;
    }
    return TOKEN_KEYWORD_NA;
}


/*
 * Return a string describing the given type of token.
 * Used in printing error and debugging messages.
 */
char *get_token_description(enum token_type_e type)
{
    switch(type)
    {
        /* general type tokes */
        case TOKEN_EMPTY           : return "empty"          ;
        case TOKEN_UNKNOWN         : return "unknown"        ;
        case TOKEN_COMMENT         : return "comment"        ;
        case TOKEN_EOF             : return "end-of-file"    ;
        case TOKEN_WORD            : return "word"           ;
        case TOKEN_ASSIGNMENT_WORD : return "assignment word";
        case TOKEN_NAME            : return "name"           ;
        case TOKEN_NEWLINE         : return "newline"        ;
        case TOKEN_IO_NUMBER       : return "IO file number" ;
        /* operator tokes */
        case TOKEN_AND_IF          : return "'&&'"           ;
        case TOKEN_OR_IF           : return "'||'"           ;
        case TOKEN_DSEMI           : return "';;'"           ;
        case TOKEN_DLESS           : return "'<<'"           ;
        case TOKEN_DGREAT          : return "'>>'"           ;
        case TOKEN_LESSAND         : return "'<&'"           ;
        case TOKEN_GREATAND        : return "'>&'"           ;
        case TOKEN_LESSGREAT       : return "'<>'"           ;
        case TOKEN_DLESSDASH       : return "'<<-'"          ;
        case TOKEN_CLOBBER         : return "'>|'"           ;
        /* POSIX Shell Keywords */
        case TOKEN_KEYWORD_IF      : return "'if'"           ;
        case TOKEN_KEYWORD_THEN    : return "'then'"         ;
        case TOKEN_KEYWORD_ELSE    : return "'else'"         ;
        case TOKEN_KEYWORD_ELIF    : return "'elif'"         ;
        case TOKEN_KEYWORD_FI      : return "'fi'"           ;
        case TOKEN_KEYWORD_DO      : return "'do'"           ;
        case TOKEN_KEYWORD_DONE    : return "'done'"         ;
        case TOKEN_KEYWORD_CASE    : return "'case'"         ;
        case TOKEN_KEYWORD_ESAC    : return "'esac'"         ;
        case TOKEN_KEYWORD_WHILE   : return "'while'"        ;
        case TOKEN_KEYWORD_UNTIL   : return "'until'"        ;
        case TOKEN_KEYWORD_FOR     : return "'for'"          ;
        case TOKEN_KEYWORD_LBRACE  : return "'{'"            ;
        case TOKEN_KEYWORD_RBRACE  : return "'}'"            ;
        case TOKEN_KEYWORD_BANG    : return "'!'"            ;
        case TOKEN_KEYWORD_IN      : return "'in'"           ;
        /* non-POSIX Shell Keywords and Operators */
        case TOKEN_KEYWORD_SELECT  : return "'select'"       ;
        case TOKEN_KEYWORD_FUNCTION: return "'function'"     ;
        case TOKEN_KEYWORD_TIME    : return "'time'"         ;
        case TOKEN_KEYWORD_COPROC  : return "'coproc'"       ;
        case TOKEN_SEMI_AND        : return "';&'"           ;
        case TOKEN_SEMI_SEMI_AND   : return "';;&'"          ;
        case TOKEN_SEMI_OR         : return "';|'"           ;
        case TOKEN_PIPE_AND        : return "'|&'"           ;
        case TOKEN_TRIPLELESS      : return "'<<<'"          ;
        case TOKEN_ANDGREAT        : return "'&>'"           ;
        case TOKEN_AND_GREAT_GREAT : return "'&>>'"          ;
        /* unknown keyword */
        case TOKEN_KEYWORD_NA      : return "unknown keyword";
        /* others */
        case TOKEN_LEFT_PAREN      : return "'('"            ;
        case TOKEN_RIGHT_PAREN     : return "')'"            ;
        case TOKEN_PIPE            : return "'|'"            ;
        case TOKEN_LESS            : return "'<'"            ;
        case TOKEN_GREAT           : return "'>'"            ;
        case TOKEN_SEMI            : return "';'"            ;
        case TOKEN_AND             : return "'&'"            ;
        case TOKEN_INTEGER         : return "integer number" ;
        case TOKEN_DSEMI_ESAC      : return "'esac' or ';;'" ;
        case TOKEN_KEYWORDS_ELIF_ELSE_FI:
                                     return "'elif', 'else' or 'fi'";
        case TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR:
                                     return "'esac', ';;', ';&', ';;&' or ';|'";
    }
    return "unknown token";
}


/*
 * Check if the given str is a shell keyword.
 * 
 * Returns the index of str in the keywords array, which is 0 to
 * one less than keyword_count, or -1 if the str is not a keyword.
 */
int is_keyword(char *str)
{
    /* sanity check */
    if(!str || !*str)
    {
        return -1;
    }
    int i;
    for(i = 0; i < keyword_count; i++)
    {
        /* check the string in the keywords array */
        if(strcmp(keywords[i], str) == 0)
        {
            return i;
        }
    }
    /* string is not a keyowrd */
    return -1;
}


/*
 * Check if the given token type represents a separator token, such as
 * the semicolon, &&, ||, braces, the pipe operator, and so on.
 * 
 * Returns 1 if the type represents a separator operator, 0 otherwise.
 */
int is_separator_tok(enum token_type_e type)
{
    switch(type)
    {
        case TOKEN_LEFT_PAREN     :
        case TOKEN_RIGHT_PAREN    :
        //case TOKEN_KEYWORD_LBRACE :
        //case TOKEN_KEYWORD_RBRACE :
        //case TOKEN_KEYWORD_BANG   :
        case TOKEN_PIPE           :
        case TOKEN_PIPE_AND       :
        case TOKEN_AND            :
        case TOKEN_NEWLINE        :
        case TOKEN_SEMI           :
        case TOKEN_SEMI_AND       :
        case TOKEN_SEMI_OR        :
        case TOKEN_SEMI_SEMI_AND  :
        case TOKEN_DSEMI          :
        case TOKEN_AND_IF         :
        case TOKEN_OR_IF          :
        case TOKEN_ANDGREAT       :
        case TOKEN_GREATAND       :
        case TOKEN_AND_GREAT_GREAT:
        case TOKEN_COMMENT        :
        case TOKEN_EOF            :
        case TOKEN_EMPTY          :
            return 1;
            
        default:
            return 0;
    }
}


/*
 * Set the type field of the given token, according to the value of
 * the token's text field.
 */
void set_token_type(struct token_s *tok)
{
    enum token_type_e t = TOKEN_EMPTY;
    /* one-char tokens */
    if(tok->text_len == 1)
    {
        switch(tok->text[0])
        {
            case '(': t = TOKEN_LEFT_PAREN    ; break;
            case ')': t = TOKEN_RIGHT_PAREN   ; break;
            case '{': t = TOKEN_KEYWORD_LBRACE; break;
            case '}': t = TOKEN_KEYWORD_RBRACE; break;
            case '!': t = TOKEN_KEYWORD_BANG  ; break;
            case '|': t = TOKEN_PIPE          ; break;
            case '<': t = TOKEN_LESS          ; break;
            case '>': t = TOKEN_GREAT         ; break;
            case '&': t = TOKEN_AND           ; break;
            case '\n':t = TOKEN_NEWLINE       ; break;
            case ';': t = TOKEN_SEMI          ; break;

            default:
                if(isdigit(tok->text[0]))
                {
                    t = TOKEN_INTEGER;
                }
                else
                {
                    t = TOKEN_WORD;
                }
                break;
        }
    }
    /* two-char tokens */
    else if(tok->text_len == 2)
    {
        switch(tok->text[0])
        {
            case '&':
                if(tok->text[1] == '>')     /* &> */
                {
                    t = TOKEN_ANDGREAT ;
                }
                else                        /* && */
                {
                    t = TOKEN_AND_IF   ;
                }
                break;

            case '|':
                if(tok->text[1] == '&')     /* |& */
                {
                    t = TOKEN_PIPE_AND ;
                }
                else                        /* || */
                {
                    t = TOKEN_OR_IF    ;
                }
                break;
                
            case ';':
                if(tok->text[1] == '&')     /* ;& */
                {
                    t = TOKEN_SEMI_AND ;
                }
                else if(tok->text[1] == '|') /* ;| */
                {
                    t = TOKEN_SEMI_OR  ;
                }
                else                        /* ;; */
                {
                    t = TOKEN_DSEMI    ;
                }
                break;
                
            case '>':
                if(tok->text[1] == '>')     /* >> */
                {
                    t = TOKEN_DGREAT   ;
                }
                else if(tok->text[1] == '&') /* >& */
                {
                    t = TOKEN_GREATAND ;
                }
                else                        /* >| */
                {
                    t = TOKEN_CLOBBER  ;
                }
                break;

            case '<':
                if(tok->text[1] == '<')     /* << */
                {
                    t = TOKEN_DLESS    ;
                }
                else if(tok->text[1] == '&') /* <& */
                {
                    t = TOKEN_LESSAND  ;
                }
                else                        /* <> */
                {
                    t = TOKEN_LESSGREAT;
                }
                break;

            default:
                /* two digit number */
                if(isdigit(tok->text[0]) && isdigit(tok->text[1]))
                {
                    t = TOKEN_INTEGER;
                    break;
                }

                /* check if its a two-letter keyword */
                int index = -1;
                if((index = is_keyword(tok->text)) >= 0)
                {
                     t = get_keyword_toktype(index);
                }
                else if(isalpha(tok->text[0]) && tok->text[1] == '=')
                {
                    /* one-letter variable and '=' */
                    t = TOKEN_ASSIGNMENT_WORD;
                }
                else
                {
                    /* just a normal word */
                    t = TOKEN_WORD;
                }
                break;
        }
    }
    /* multi-char tokens */
    else
    {
        if(tok->text[0] == '#')
        {
            /* comment token */
            t = TOKEN_COMMENT;
        }
        else if(isdigit(tok->text[0]))
        {
            /* number token */
            t = TOKEN_INTEGER;
            char *p = tok->text;
            do
            {
                /* if it contains any non-digit chars, its a word, not a number */
                if(*p < '0' || *p > '9')
                {
                    t = TOKEN_WORD;
                    break;
                }
            } while(*++p);
        }
        else if(isalpha(tok->text[0]))
        {
            /* check if its a keyword */
            int index = -1;
            if((index = is_keyword(tok->text)) >= 0)
            {
                 t = get_keyword_toktype(index);
            }
            else
            {
                /* if it contains =, check if its an assignment word */
                if(strchr(tok->text, '='))
                {
                    t = TOKEN_ASSIGNMENT_WORD;
                    char *p = tok->text;
                    /*
                     * if assignment word, chars before the '=' must be alphanumeric or '_', as this
                     * refers to the variable name to which we're assigning a value.
                     */
                    while(*++p != '=')
                    {
                        if(!isalnum(*p) && *p != '_')
                        {
                            /* this is included to support bash's extended assignment by using += */
                            if(*p == '+' && p[1] == '=')
                            {
                                continue;
                            }
                            /* non-alphanumeric or '_' chars before '=' means its not an assignment word */
                            t = TOKEN_WORD;
                            break;
                        }
                    }
                }
                else
                {
                    t = TOKEN_WORD;
                }
            }
        }
        else if(strcmp(tok->text, "<<<") == 0)
        {
            t = TOKEN_TRIPLELESS;
        }
        else if(strcmp(tok->text, ";;&") == 0)
        {
            t = TOKEN_SEMI_SEMI_AND;
        }
        else if(strcmp(tok->text, "&>>") == 0)
        {
            t = TOKEN_AND_GREAT_GREAT;
        }
        else
        {
            t = TOKEN_WORD;
        }
    }
    /* if we couldn't assign a type to this token, give it a TOKEN_UNKNOWN type */
    if(t == TOKEN_EMPTY)
    {
        t = TOKEN_UNKNOWN;
    }
    tok->type = t;
}


/*
 * Create a new token for the given str.
 * 
 * Returns a pointer to the struct token_s of the new token.
 */
struct token_s *create_token(char *str)
{
    struct token_s *tok = malloc(sizeof(struct token_s));
    if(!tok)
    {
        return NULL;
    }
    memset(tok, 0, sizeof(struct token_s));
    tok->text_len = strlen(str);
    tok->text = get_malloced_str(str);
    return tok;
}


/*
 * Add a character to the token buffer. When NULL-terminated later on,
 * the buffer will contain the text of the current token.
 */
void add_to_buf(char c)
{
    tok_buf[tok_bufindex++] = c;
    /* if the buffer is full, try to extend it */
    if(tok_bufindex >= tok_bufsize)
    {
        /* try to double the buffer's size */
        char *tmp = realloc(tok_buf, tok_bufsize*2);
        if(!tmp)
        {
            errno = ENOMEM;
            return;
        }
        /* save the new buffer pointer and the new size */
        tok_buf = tmp;
        tok_bufsize *= 2;
    }
}


/*
 * Return a pointer to the current token.
 */
struct token_s *get_current_token(void)
{
    return cur_tok ? : &eof_token;
}


/*
 * Return a pointer to the previous token.
 */
struct token_s *get_previous_token(void)
{
    return prev_tok;
}


/*
 * Return a pointer to the previous token.
 */
void set_current_token(struct token_s *tok)
{
    cur_tok = tok;
}


/*
 * Return a pointer to the previous token.
 */
void set_previous_token(struct token_s *tok)
{
    prev_tok = tok;
}


/* 
 * Free the current and previous tokens and set the pointers to those in the 
 * arguments.
 */
void restore_tokens(struct token_s *old_current_token,
                    struct token_s *old_previous_token)
{
    /* free current pointers */
    free_token(get_current_token());
    free_token(get_previous_token());
    
    /* restore token pointers */
    set_current_token(old_current_token);
    set_previous_token(old_previous_token);
}


/*
 * Duplicate a token struct.
 * 
 * Returns the newly alloc'd token struct, or NULL in case of error.
 */
struct token_s *dup_token(struct token_s *tok)
{
    if(!tok)
    {
        return NULL;
    }

    /* alloc memory for the token struct */
    struct token_s *tok2 = malloc(sizeof(struct token_s));
    if(!tok2)
    {
        return NULL;
    }

    /* copy the old token into the new one */
    memcpy(tok2, tok, sizeof(struct token_s));
    
    /* copy the text string */
    if(tok->text)
    {
        tok->text_len = strlen(tok->text);
        tok2->text = get_malloced_str(tok->text);
    }
    tok2->text_len = tok->text_len;
    
    /* return the new token */
    return tok2;
}


/*
 * Free the memory used by a token.
 */
void free_token(struct token_s *tok)
{
    /* don't attempt to free the EOF token */
    if(!tok || tok == &eof_token)
    {
        return;
    }

    /* free the token text */
    if(tok->text)
    {
        free_malloced_str(tok->text);
    }
    
    /* free the token struct */
    free(tok);

    /* update the current token struct pointer */
    if(cur_tok == tok)
    {
        cur_tok = NULL;
    }

    /* update the previous token struct pointer */
    if(prev_tok == tok)
    {
        prev_tok = NULL;
    }
}


/*
 * Sometimes we need to check tokens against a number of different types.
 * For example, an if clause can end in elif, else or fi. we need to check
 * all three when parsing if clauses, and so on for the other types of tokens.
 * 
 * Returns 1 if the token is of the given type, 0 otherwise.
 */
int is_token_of_type(struct token_s *tok, enum token_type_e type)
{
    /* simple match */
    if(tok->type == type)
    {
        return 1;
    }

    /* type can be any one of elif, else, or fi */
    if(type == TOKEN_KEYWORDS_ELIF_ELSE_FI)
    {
        if(tok->type == TOKEN_KEYWORD_ELIF ||
           tok->type == TOKEN_KEYWORD_ELSE ||
           tok->type == TOKEN_KEYWORD_FI)
        {
            return 1;
        }
    }
    
    /*
     * case items should end in ';;' but sometimes the last 
     * item might end in 'esac'. non-POSIX extensions include
     * ';&' and ';;&' which are used by bash, ksh, zsh et al.
     */
    if(type == TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR)
    {
        if(tok->type == TOKEN_KEYWORD_ESAC ||
           tok->type == TOKEN_DSEMI        ||
           tok->type == TOKEN_SEMI_AND     ||
           tok->type == TOKEN_SEMI_OR      ||
           tok->type == TOKEN_SEMI_SEMI_AND)
        {
            return 1;
        }
    }
    
    /* type can be any one of esac or ;; */
    if(type == TOKEN_DSEMI_ESAC)
    {
        if(tok->type == TOKEN_KEYWORD_ESAC || tok->type == TOKEN_DSEMI)
        {
            return 1;
        }
    }
    
    /* token doesn't match the given type */
    return 0;
}


/*
 * Scan the input source and get the next token.
 * 
 * Returns an malloc'd token_s struct containing the next token
 * if successfull, NULL on error.
 */
struct token_s *tokenize(struct source_s *src)
{
    if(!src || !src->buffer || !src->bufsize)
    {
        errno = ENODATA;
        return &eof_token;
    }

    /* first time ever? */
    if(!tok_buf)
    {
        tok_bufsize = 1024;
        tok_buf = malloc(tok_bufsize);
        if(!tok_buf)
        {
            errno = ENOMEM;
            eof_token.src = src;
            return &eof_token;
        }
    }
    
    /* empty the buffer */
    tok_bufindex = 0;
    tok_buf[0]   = '\0';
    
    /* save the current token in the prev token pointer */
    if(cur_tok)
    {
        if(prev_tok)
        {
            free_token(prev_tok);
        }
        prev_tok = cur_tok;
        cur_tok = NULL;
    }

    struct token_s *tok = NULL;
    long linest, line, chr;
    char nc2, pc;
    size_t i;
    /* flag to end the loop */
    int  endloop = 0;
    /* 
     * bash and zsh identify # comments in non-interactive shells, and in interactive
     * shells with the interactive_comments option.
     */
    int skip_hashes = (interactive_shell && src->srctype == SOURCE_STDIN &&
                       !optionx_set(OPTION_INTERACTIVE_COMMENTS));

    /* init position indexes */
    src->curpos_old = src->curpos+1;
    if(src->curpos < 0)
    {
        linest = 0;
        line = 1;
        chr = 1;
    }
    else
    {
        linest = src->curlinestart;
        line = src->curline;
        chr  = src->curchar;
    }

    /* get next char */
    char nc = next_char(src);
    if(nc == ERRCHAR || nc == EOF)
    {
        eof_token.lineno    = src->curline     ;
        eof_token.charno    = src->curchar     ;
        eof_token.linestart = src->curlinestart;
        eof_token.src       = src;
        cur_tok = &eof_token;
        return &eof_token;
    }
    
    /* loop to get the next token */
    do
    {
        switch(nc)
        {
            case  '"':
            case '\'':
            case  '`':
                /*
                 * for quote chars, add the quote, as well as everything between this
                 * quote and the matching closing quote, to the token buffer.
                 */
                add_to_buf(nc);
                i = find_closing_quote(src->buffer+src->curpos,
                                       (nc == '"') ? 1 : 0,
                                       (prev_char(src) == '$') ? 1 : 0);
                while(i--)
                {
                    /*
                     * remove backslash quotes and discard \\n inside double and
                     * back quotes.
                     */
                    pc = next_char(src);
                    if(pc == '\\' && nc != '\'')
                    {
                        char pc2 = peek_char(src);
                        if(pc2 == '\n')
                        {
                            i--;
                            next_char(src);
                            continue;
                        }
                    }
                    add_to_buf(pc);
                }
                break;

            case '\\':
                /* get the next char after the backslah */
                nc2 = next_char(src);

                /*
                 * Discard backslash+newline '\\n' combination. In an interactive shell, this
                 * case shouldn't happen as the read_cmd() function discards the '\\n' sequence
                 * automatically. However, if the input comes from a command string or script,
                 * we might encounter this sequence.
                 */
                if(nc2 == '\n')
                {
                    break;
                }

                /* add the backslah to the token buffer */
                add_to_buf(nc);
                
                /* add the escaped char to the token buffer */
                if(nc2 > 0)
                {
                    add_to_buf(nc2);
                }
                break;

            case '$':
                /* add the '$' to buffer and check the char after it */
                add_to_buf(nc);
                nc = peek_char(src);
                
                /* we have a '${', '$(' or '$[' sequence */
                if(nc == '{' || nc == '(' || nc == '[')
                {
                    /* add the opening brace and everything up to the closing brace to the buffer */
                    i = find_closing_brace(src->buffer+src->curpos+1, 0);
                    if(i == 0)
                    {
                        /* closing brace not found */
                        return &eof_token;
                    }
                    
                    while(i--)
                    {
                        add_to_buf(next_char(src));
                    }
                    add_to_buf(next_char(src));     /* add the closing brace */
                }
                /*
                 * we have a special parameter name, such as $0, $*, $@, $#,
                 * or a positional parameter name, such as $1, $2, ...
                 */
                else if(isalnum(nc) || nc == '*' || nc == '@' || nc == '#' ||
                                       nc == '!' || nc == '?' || nc == '$')
                {
                    add_to_buf(next_char(src));
                }
                /* we have the $< special var (csh/tcsh) */
                else if(nc == '<')
                {
                    add_to_buf(next_char(src));
                    endloop = 1;
                }
                break;

            case '>':
            case '<':
            case '|':
                /* if an '>', '<', or '|' operator delimits the current token, delimit it */
                if(tok_bufindex > 0)
                {
                    unget_char(src);
                    endloop = 1;
                    break;
                }
                
                /* add the operator to buffer and check the char after it */
                add_to_buf(nc);
                pc = peek_char(src);
                if(nc == pc ||                     /* <<, >> and || */
                  (nc == '<' && pc == '>') ||      /* <> */
                  (nc == '>' && pc == '|') ||      /* >| */
                  (nc == '>' && pc == '!') ||      /* >! */
                  (nc == '<' && pc == '&') ||      /* <& */
                  (nc == '>' && pc == '&') ||      /* >& */
                  (nc == '|' && pc == '&'))        /* |& */
                {
                    add_to_buf(next_char(src));
                    pc = peek_char(src);
                    
                    /* peek at the next char to identify three-char operator tokens */
                    if(nc == '<' && (pc == '-' || pc == '<'))       /* <<- and <<< */
                    {
                        add_to_buf(next_char(src));
                    }
                }
                endloop = 1;
                break;

            case '&':
            case ';':
                /* if an '&' or ';' operator delimits the current token, delimit it */
                if(tok_bufindex > 0)
                {
                    unget_char(src);
                    endloop = 1;
                    break;
                }
                
                /* add the operator to buffer and check the char after it */
                add_to_buf(nc);
                pc = peek_char(src);
                /* identify two-char tokens ;; and && */
                if(nc == pc)
                {
                    add_to_buf(next_char(src));
                    
                    /* identify ';;&' */
                    if(nc == ';')
                    {
                        pc = peek_char(src);
                        if(pc == '&')
                        {
                            add_to_buf(next_char(src));
                        }
                    }
                }
                /* identify ';&' and ';|' */
                else if(nc == ';' && (pc == '&' || pc == '|'))
                {
                    add_to_buf(next_char(src));
                }
                /* identify '&>' */
                else if(nc == '&' && pc == '>')
                {
                    add_to_buf(next_char(src));
                    
                    /* identify '&>>' */
                    pc = peek_char(src);
                    if(pc == '>')
                    {
                        add_to_buf(next_char(src));
                    }
                }
                endloop = 1;
                break;
            
            case '(':
            case ')':
                /* if the brace delimits the current token, delimit it */
                if(tok_bufindex > 0)
                {
                    unget_char(src);
                    endloop = 1;
                    break;
                }
                
                if(nc == '(')
                {
                    /*
                     * Recognize the ((expr)) structs (an old shorthand for arithmetic evaluation),
                     * and the >(cmd) and <(cmd) structs, which are used for process substitution.
                     * All of these are non-POSIX extensions.
                     */
                    pc = prev_char(src);
                    nc2 = peek_char(src);

                    /* check if we have '((', '<(' or '>(' */
                    if(nc2 == '(' || pc == '<' || pc == '>')
                    {
                        i = find_closing_brace(src->buffer+src->curpos, 0);
                        if(i == 0)
                        {
                            /* closing brace not found */
                            return &eof_token;
                        }
                        add_to_buf(nc);
                        
                        /*
                         * if '((' is not terminated by '))', we don't have an 
                         * arithmetic evaluation of the type (( )).
                         */
                        if(nc == nc2 && (src->buffer[src->curpos+i-1] != ')'))
                        {
                            endloop = 1;
                            break;
                        }

                        while(i--)
                        {
                            add_to_buf(next_char(src));
                        }
                        break;
                    }
                }
                
                /* single char delimiters */
                add_to_buf(nc);
                endloop = 1;
                break;

            case ' ':
            case '\t':
                /* if the whitespace delimits the current token, delimit it */
                if(tok_bufindex > 0)
                {
                    endloop = 1;
                    /* We return the whitespace char to input, because we need to
                     * check the current input char when we're parsing I/O redirections.
                     * This is important as it makes the difference between cmds like
                     * `echo 2>out` and `echo 2 >out`.
                     */
                    unget_char(src);
                }
                /* otherwise just skip it */
                else
                {
                    chr++;
                }
                break;

            case '\n':
                /* if the newline delimits the current token, delimit it */
                if(tok_bufindex > 0)
                {
                    unget_char(src);
                    endloop = 1;
                    break;
                }
                
                /* otherwise return it as a token by itself */
                add_to_buf(nc);
                endloop = 1;
                break;

            case '#':
                /* 
                 * if the hash is part of the current token, or if we're not
                 * recognizing comments, add the hash char to the buffer.
                 */
                if(tok_bufindex > 0 || skip_hashes)
                {
                    add_to_buf(nc);
                    break;
                }
                
                /* 
                 * otherwise discard the comment as per POSIX section 2.3, but return a newline
                 * token (the newline is technically part of the comment itself).
                 */
                while((nc = next_char(src)) > 0)
                {
                    if(nc == '\n')
                    {
                        add_to_buf(nc);
                        endloop = 1;
                        break;
                    }
                }
                break;

            default:
                /* for all other chars, just add to the buffer */
                add_to_buf(nc);
                break;
        }
        
        /* is the token delimited? */
        if(endloop)
        {
            break;
        }
    } while((nc = next_char(src)) != EOF);      /* loop until we hit EOF */
        
    eof_token.lineno    = src->curline;
    eof_token.charno    = src->curchar;
    eof_token.linestart = src->curlinestart;
    eof_token.src       = src;
    
    /* if we have no chars, we've reached EOF */
    if(tok_bufindex == 0)
    {
        cur_tok = &eof_token;
        return &eof_token;
    }
        
    /* null-terminate the token */
    tok_buf[tok_bufindex] = '\0';

    /* create the token */
    tok = create_token(tok_buf);
    if(!tok)
    {
        PRINT_ERROR(SHELL_NAME, "failed to alloc buffer: %s", strerror(errno));
        return &eof_token;
    }
    
    /* give the token a numeric type, according to its contents */
    set_token_type(tok);

    /*
     * If the token consists solely of a number, we need to check the next
     * character. If it's the beginning of a redirection operator ('>' or '<'),
     * we have an TOKEN_IO_NUMBER, which is your file descriptor in redirections
     * such as '2>&/dev/null' or '1<some_file'. Otherwise treat it as a word.
     */
    if(tok->type == TOKEN_INTEGER)
    {
        char pc = peek_char(src);
        if(pc == '<' || pc == '>')
        {
            tok->type = TOKEN_IO_NUMBER;
        }
        else
        {
            tok->type = TOKEN_WORD; 
        }
    }
    /* record where we encountered this token in the input */
    tok->lineno    = line;
    tok->charno    = chr;
    tok->src       = src;
    tok->linestart = linest;
    
    /* we do the -v option in the parse_translation_unit() function */
    //if(option_set('v')) fprintf(stderr, "%s", tok->text);

    /* set the current token pointer */
    cur_tok = tok;

    /* return the token */
    return tok;
}
