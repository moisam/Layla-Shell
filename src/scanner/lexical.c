/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "keywords.h"
#include "scanner.h"
#include "../parser/parser.h"       /* __parse_alias() */
#include "../builtins/setx.h"
#include "../debug.h"

char *__buf = NULL;             /* the buffer we'll use while parsing a token */
int   __bufsize = 0;            /* the size of memory alloc'd for the buffer */
int   __bufindex = -1;          /* our current position in the buffer */
/* the current token */
static struct token_s   __cur_tok = { .type = TOKEN_EMPTY, };
/* the previous (last) token */
static struct token_s  __prev_tok = { .type = TOKEN_EMPTY, };
/* a pointer to the current token struct */
static struct token_s *cur_tok    = NULL;

/* special token to indicate end of input */
struct token_s eof_token = 
{
    .type     = TOKEN_EOF,
    .lineno   = 0,
    .charno   = 0,
    .text_len = 0,
};

/* special token to indicate error scanning input */
struct token_s err_token = 
{
    .type     = TOKEN_ERROR,
    .lineno   = 0,
    .charno   = 0,
    .text_len = 0,
};

/* type of the previous token */
enum token_type prev_type = TOKEN_EMPTY;

/*
 * return the token type that describes one of the shell's keywords.
 * the keywords are stored in an array (defined in keywords.h) and the
 * index field gives the index of an item in the array.. the value we
 * return is the token type describing the indexed keyword.. if the
 * index is out of the keywords array bounds, we return TOKEN_KEYWORD_NA.
 */
enum token_type get_keyword_toktype(int index)
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
    }
    return TOKEN_KEYWORD_NA;
}


/*
 * return a string describing the given type of token..
 * used in printing error and debugging messages.
 */
char *get_token_description(enum token_type type)
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
        case TOKEN_OPENBRACE       : return "'('"            ;
        case TOKEN_CLOSEBRACE      : return "')'"            ;
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
        case TOKEN_ERROR           : return "'error'"        ;
    }
    return "unknown token";
}


/*
 * check if the given str is a shell keyword.
 * 
 * returns the index of str in the keywords array, which is 0 to
 * one less than keyword_count, or -1 if the str is not a keyword.
 */
int is_keyword(char *str)
{
    /* sanity check */
    if(!str)
    {
        return 0;
    }
    size_t len = strlen(str);
    /* empty string */
    if(len == 0)
    {
        return 0;
    }
    int j;
    for(j = 0; j < keyword_count; j++)
    {
        if(keywords[j].len != len)
        {
            continue;
        }
        /* found the string in the array */
        if(strcmp(keywords[j].str, str) == 0)
        {
            return j;
        }
    }
    /* string is not a keyowrd */
    return -1;
}


/*
 * check if the given token type represents a separator token, such as
 * the semicolon, &&, ||, braces, the pipe operator, and so on.
 * 
 * returns 1 if the type represents a separator operator, 0 otherwise.
 */
int is_separator_tok(enum token_type type)
{
    switch(type)
    {
        case TOKEN_OPENBRACE      :
        case TOKEN_CLOSEBRACE     :
        case TOKEN_KEYWORD_LBRACE :
        case TOKEN_KEYWORD_RBRACE :
        case TOKEN_KEYWORD_BANG   :
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
 * set the type field of the given token, according to the value of
 * the token's text field.
 */
void set_token_type(struct token_s *tok)
{
    enum token_type t = TOKEN_EMPTY;
    /* one-char tokens */
    if(tok->text_len == 1)
    {
        switch(tok->text[0])
        {
            case '(': t = TOKEN_OPENBRACE     ; break;
            case ')': t = TOKEN_CLOSEBRACE    ; break;
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
 * create a new token for the given str.
 * 
 * returns a pointer to the struct token_s of the new token.
 */
struct token_s *create_token(char *str)
{
    /* save cur token as the prev token */
    memcpy(&__prev_tok, &__cur_tok, sizeof(struct token_s));
    /* save new token str */
    size_t len = strlen(str);
    __cur_tok.text_len = len;
    __cur_tok.text     = str;
    /* return pointer to cur token */
    return &__cur_tok;
}


/*
 * create a new token from the given char, as if we were passed
 * a one-char string.
 * 
 * returns a pointer to the struct token_s of the new token.
 */
struct token_s *create_singlechar_token(char c)
{
    __buf[0] = c;
    __buf[1] = '\0';
    return create_token(__buf);
}


/*
 * add a character to the token buffer.. when NULL-terminated later on,
 * the buffer will contain the text of the current token.
 */
void add_to_buf(char c)
{
    __buf[__bufindex++] = c;
    /* if the buffer is full, try to extend it */
    if(__bufindex >= __bufsize)
    {
        /* try to double the buffer's size */
        char *tmp = realloc(__buf, __bufsize*2);
        if(!tmp)
        {
            errno = ENOMEM;
            return;
        }
        /* save the new buffer pointer and the new size */
        __buf = tmp;
        __bufsize *= 2;
    }
}


/*
 * NULL-terminate the token buffer.
 */
static inline void null_terminate_buf()
{
    __buf[__bufindex] = '\0';
}


/*
 * return a pointer to the current token.
 */
struct token_s *get_current_token()
{
    return cur_tok;
}


/*
 * return a pointer to the previous token.
 */
struct token_s *get_previous_token()
{
    return &__prev_tok;
}


/*
 * for the opening brace nc, loop through the input source until we find
 * the matching closing brace.. this looping involves skipping quote chars
 * and quoted strings, as well as nested braces of all types.. all chars
 * from the opening to the closing braces, and anything in between, are
 * added to the token buffer as part of the current token.
 * 
 * returns 1 if the closing brace is found, 0 otherwise.
 */
int brace_loop(char nc, struct source_s *src)
{
    /* add opening brace to buffer */
    add_to_buf(next_char(src));
    /* determine which closing brace to search for */
    char opening_brace = nc, closing_brace;
    if(opening_brace == '{')
    {
        closing_brace = '}';
    }
    else if(opening_brace == '[')
    {
        closing_brace = ']';
    }
    else
    {
        closing_brace = ')';
    }
    /* count the number of opening and closing braces we encounter */
    int ob_count = 1, cb_count = 0;
    char pc = nc;
    /* get the next char */
    while((nc = next_char(src)) > 0)
    {
        /* add it to the buffer */
        add_to_buf(nc);
        /* we have a quote char */
        if((nc == '"') || (nc == '\'') || (nc == '`'))
        {
            /* if it's an escaped quote, pass */
            if(pc == '\\')
            {
                continue;
            }
            char quote = nc;
            char nc2 = 0;
            /* loop till we get EOF (-1) or ERRCHAR (0) */
            while((nc2 = next_char(src)) > 0)
            {
                /* add char to buffer */
                add_to_buf(nc2);
                /* we have a closing quote matching our opening quote */
                if(nc2 == quote)
                {
                    if(nc != '\\')
                    {
                        /* if it's not escaped, break the loop */
                        break;
                    }
                }
                /* save the current char, so we can check for escaped quotes */
                nc = nc2;
            }
            continue;
        }
        if((pc != '\\'))
        {
            /* we have an unquoted opening brace char */
            if(nc == opening_brace)
            {
                ob_count++;
            }
            /* we have an unquoted closing brace char */
            else if(nc == closing_brace)
            {
                cb_count++;
            }
        }
        /* if we have a matching number of opening and closing braces, break the loop */
        if(ob_count == cb_count)
        {
            break;
        }
        pc = nc;
    }
    /* unmatching opening and closing brace count */
    if(ob_count != cb_count)
    {
        src->curpos = src->bufsize;
        cur_tok = &eof_token;
        fprintf(stderr, "%s: missing closing brace '%c'\n",
                SHELL_NAME, closing_brace);
        /* return failure */
        return 0;
    }
    /* closing brace found. return success */
    return 1;
}


/*
 * scan the input source and get the next token.
 * 
 * returns an malloc'd token_s struct containing the next token
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
    if(!__buf)
    {
        __bufsize = 1024;
        __buf = malloc(__bufsize);
        if(!__buf)
        {
            errno = ENOMEM;
            return &eof_token;
        }
    }
    /* empty the buffer */
    __bufindex     = 0;
    __buf[0]       = '\0';

    struct token_s *tok = NULL;
    long line, chr;
    char nc2, pc;
    size_t i;

    /* init position indexes */
    if(src->curpos < 0)
    {
        line = 1;
        chr  = 1;
    }
    else
    {
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
                i = find_closing_quote(src->buffer+src->curpos, (prev_char(src) == '$') ? 1 : 0);
                while(i--)
                {
                    add_to_buf(next_char(src));
                }
#if 0
                quote = nc;
                nc2 = 0;
                /* add quote to buffer */
                add_to_buf(nc);
                /*
                 * ANSI-C strings look like $'string' (non-POSIX extension).
                 * they allow escaped single quotes, so we take note of that case.
                 */
                int ansic = (prev_char(src) == '$') ? 1 : 0;
                /* loop till we get EOF (-1) or ERRCHAR (0) */
                while((nc2 = next_char(src)) > 0)
                {
                    /* add char to buffer */
                    add_to_buf(nc2);
                    /* we have a quote char matching our opening quote */
                    if(nc2 == quote)
                    {
                        /*
                         * a matching single quote terminates the quoted string, even if
                         * the quote is escaped, except when we're parsing an ANSI-C string,
                         * which allows escaped single quotes within the string.
                         */
                        if(quote == '\'' && !ansic)
                        {
                            break;
                        }
                        /* unquoted single quote. stop parsing the quoted string */
                        if(nc != '\\')
                        {
                            break;
                        }
                    }
                    else if(nc2 == '$')
                    {
                        /* check next char after the '$' */
                        nc2 = peek_char(src);
                        /* we have a '${' or a '$(' sequence */
                        if(nc2 == '{' || nc2 == '(')
                        {
                            /* find the matching closing brace */
                            if(!brace_loop(nc2, src))
                            {
                                /* failed to find matching brace. return error token */
                                return &err_token;
                            }
                        }
                        /*
                         * we have a special parameter name, such as $0, $*, $@, $#,
                         * or a positional parameter name, such as $1, $2, ...
                         */
                        else if(isalnum(nc2) || nc2 == '*' || nc2 == '@' || nc2 == '#' ||
                                                nc2 == '!' || nc2 == '?' || nc2 == '$')
                        {
                            /* add next char to token buffer */
                            add_to_buf(next_char(src));
                        }
                    }
                    nc = nc2;
                }
#endif
                break;

            case '\\':
                /* get the next char after the backslah */
                nc2 = next_char(src);
                /*
                 * discard backslash+newline '\\n' combination.. in an interactive shell, this
                 * case shouldn't happen as the read_cmd() function discards the '\\n' sequence
                 * automatically.. however, if the input comes from a command string or script,
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
                    /* find the matching closing brace */
                    i = find_closing_brace(src->buffer+src->curpos);
                    if(!i)
                    {
                        /* failed to find matching brace. return error token */
                        src->curpos = src->bufsize;
                        cur_tok = &eof_token;
                        fprintf(stderr, "%s: missing closing brace '%c'\n", SHELL_NAME, nc);
                        return &err_token;
                    }
                    while(i--)
                    {
                        add_to_buf(next_char(src));
                    }
#if 0
                    if(!brace_loop(nc, src))
                    {
                        /* failed to find matching brace. return error token */
                        return &err_token;
                    }
#endif
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
                    goto prep_token;
                }
                break;

            case '>':
            case '<':
            case '|':
                /* if an '>', '<', or '|' operator delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                /* add the operator to buffer and check the char after it */
                add_to_buf(nc);
                pc = peek_char(src);
                /* identify two-char tokens <<, >> and || */
                if(nc == pc)
                {
                    add_to_buf(next_char(src));
                    pc = peek_char(src);
                    /* peek at the next char to identify three-char operator tokens */
                    if(nc == '<' && (pc == '-' || pc == '<'))       /* <<- and <<< */
                    {
                        add_to_buf(next_char(src));
                    }
                    goto prep_token;
                }
                if((nc == '<' && pc == '>') ||      /* <> */
                   (nc == '>' && pc == '|') ||      /* >| */
                   (nc == '>' && pc == '!') ||      /* >! */
                   (nc == '<' && pc == '&') ||      /* <& */
                   (nc == '>' && pc == '&') ||      /* >& */
                   (nc == '|' && pc == '&'))        /* |& */
                {
                    add_to_buf(next_char(src));
                    goto prep_token;
                }
                /* nothing of the above matched. we have a single-char token */
                tok = create_singlechar_token(nc);
                goto token_ready;

            case '&':
            case ';':
                /* if an '&' or ';' operator delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
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
                    goto prep_token;
                }
                /* identify ';&' and ';|' */
                if(nc == ';' && (pc == '&' || pc == '|'))
                {
                    add_to_buf(next_char(src));
                    goto prep_token;
                }
                /* identify '&>' */
                if(nc == '&' && pc == '>')
                {
                    add_to_buf(next_char(src));
                    /* identify '&>>' */
                    pc = peek_char(src);
                    if(pc == '>')
                    {
                        add_to_buf(next_char(src));
                    }
                    goto prep_token;
                }
                /* nothing of the above matched. we have a single-char token */
                tok = create_singlechar_token(nc);
                goto token_ready;

            case '{':
            case '}':
                /*
                 * if the '{' or '}' keyword delimits the current token, delimit it.
                 * braces are recognized as keywords only when they occur after a semi-colon
                 * or a newline character.
                 */
                pc = prev_char(src);
                if(__bufindex > 0 && (isspace(pc) || pc == ';'))
                {
                    unget_char(src);
                    goto prep_token;
                }
                /* check the char after the brace */
                pc = peek_char(src);
                if(nc == '}' || (!isalnum(pc) && pc != '_'))   /* not a {var} I/O redirection word */
                {
                    /* single char keywords '}' or '{' */
                    add_to_buf(nc);
                    tok = create_singlechar_token(nc);
                    goto token_ready;
                }
                /* add the opening brace and everything up to the closing brace to the buffer */
                unget_char(src);
                if(!brace_loop(nc, src))
                {
                    /* closing brace not found */
                    return &err_token;
                }
                break;
                
            case '!':
            case '(':
            case ')':
                /* if the '!' keyword or the brace delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                if(nc == '(')
                {
                    /*
                     * recognize the ((expr)) structs (an old shorthand for arithmetic evaluation),
                     * and the >(cmd) and <(cmd) structs, which are used for process substitution.
                     * all of these are non-POSIX extensions.
                     */
                    pc = prev_char(src);
                    /* check if we have '((', '<(' or '>(' */
                    if(peek_char(src) == '(' || pc == '<' || pc == '>')
                    {
                        unget_char(src);    /* the 1st '(' */
                        /* add the opening brace and everything up to the closing brace to the buffer */
                        if(!brace_loop(nc, src))
                        {
                            /* closing brace not found */
                            return &err_token;
                        }
                        break;
                    }
                }
                /* '!' must be a separate token if it's followed by a space char */
                else if(nc == '!')
                {
                    pc = peek_char(src);
                    if(pc && !isspace(pc))
                    {
                        add_to_buf(nc);
                        break;
                    }
                }
                /* single char delimiters */
                add_to_buf(nc);
                tok = create_singlechar_token(nc);
                goto token_ready;

            case ' ':
            case '\t':
                /* if the whitespace delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    goto prep_token;
                }
                /* otherwise just skip it */
                chr++;
                break;

            case '\n':
                /* if the newline delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                /* otherwise return it as a token by itself */
                add_to_buf(nc);
                tok = create_singlechar_token(nc);
                goto token_ready;

            case '#':
                /* if the comment delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                /*
                 * >#((expr)) and <#((expr)) are non-POSIX extensions to move
                 * I/O file pointers to the offset specified by expr.
                 */
                pc = prev_char(src);
                if(pc == '>' || pc == '<')      /* '#>' and '#<' */
                {
                    nc2 = peek_char(src);
                    if(nc2 == '(')              /* '#>(' and '#<(' */
                    {
                        nc2 = next_char(src);   /* skip the first '(' */
                        nc2 = peek_char(src);   /* check the second '(' */
                        if(nc2 == '(')
                        {
                            /* add both braces to buffer */
                            add_to_buf(nc);
                            add_to_buf(nc2);
                            /*
                             * loop to find the matching 2 braces, but no need to call the heavy guns,
                             * aka brace_loop().
                             */
                            int cb = 0;
                            while((nc = next_char(src)) != EOF)
                            {
                                add_to_buf(nc);
                                /* keep the count of closing braces */
                                if(nc == ')')
                                {
                                    cb++;
                                }
                                /* and break when we have two */
                                if(cb == 2)
                                {
                                    break;
                                }
                            }
                            goto prep_token;
                        }
                    }
                }
                /* 
                 * bash and zsh identify # comments in non-interactive shells, and in interactive
                 * shells with the interactive_comments option.
                 */
                if(option_set('i') && !optionx_set(OPTION_INTERACTIVE_COMMENTS))
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
                        tok = create_singlechar_token(nc);
                        goto token_ready;
                    }
                }
                break;

            default:
                /* for all other chars, just add to the buffer */
                add_to_buf(nc);
                break;
        }
    } while((nc = next_char(src)) != EOF);      /* loop until we hit EOF */
        
    /* if we have no chars, we've reached EOF */
    if(__bufindex == 0)
    {
        eof_token.lineno    = src->curline     ;
        eof_token.charno    = src->curchar     ;
        eof_token.linestart = src->curlinestart;
        cur_tok = &eof_token;
        return &eof_token;
    }
        
prep_token:
    /* null-terminate the token */
    null_terminate_buf();
    /* create the token */
    tok = create_token(__buf);
    
token_ready:
    /* give the token a numeric type, according to its contents */
    set_token_type(tok);
    /*
     * if the token consists solely of a number, we need to check the next
     * character.. if it's the beginning of a redirection operator ('>' or '<'),
     * we have an TOKEN_IO_NUMBER, which is your file descriptor in redirections
     * such as '2>&/dev/null' or '1<some_file'.. otherwise treat it as a word.
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
    tok->linestart = src->curlinestart;
    
    /* we do the -v option in the parse_translation_unit() function */
    //if(option_set('v')) fprintf(stderr, "%s", tok->text);

    /* set the current and previous token pointers */
    prev_type = tok->type;
    cur_tok = tok;
    /* return the token */
    return tok;
}


/*
 * duplicate a token struct.
 * 
 * returns the newly alloc'd token struct, or NULL in case of error.
 */
struct token_s *dup_token(struct token_s *tok)
{
    /* alloc memory for the token struct */
    struct token_s *tok2 = malloc(sizeof(struct token_s));
    if(!tok2)
    {
        return NULL;
    }
    /* copy the old token into the new one */
    memcpy(tok2, (void *)tok, sizeof(struct token_s));
    tok->text_len = strlen(tok->text);
    /* alloc memory for the token text */
    char *text = malloc(tok->text_len+1);
    if(!text)
    {
        free(tok2);
        return NULL;
    }
    /* copy the text string */
    strcpy(text, tok->text);
    tok2->text = text;
    tok2->text_len = tok->text_len;
    /* return the new token */
    return tok2;
}


/*
 * free the memory used by a token.
 */
void free_token(struct token_s *tok)
{
    /* free the token text */
    if(tok->text)
    {
        free(tok->text);
    }
    /* free the token struct */
    free(tok);
}

/*
 * sometimes we need to check tokens against a number of different types.
 * for example, an if clause can end in elif, else or fi. we need to check
 * all three when parsing if clauses, and so on for the other types of tokens.
 * 
 * returns 1 if the token is of the given type, 0 otherwise.
 */
int is_token_of_type(struct token_s *tok, enum token_type type)
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
