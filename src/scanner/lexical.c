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


struct token_s *first_token;
struct token_s eof_token = 
{
    .type     = TOKEN_EOF,
    .lineno   = 0,
    .charno   = 0,
    .text_len = 0,
};

struct token_s err_token = 
{
    .type     = TOKEN_ERROR,
    .lineno   = 0,
    .charno   = 0,
    .text_len = 0,
};

enum token_type prev_type = TOKEN_EMPTY;

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
            case TOKEN_KEYWORDS_ELIF_ELSE_FI:
                                         return "'elif', 'else' or 'fi'";
            case TOKEN_DSEMI_ESAC_SEMIAND:
                                         return "'esac', ';;', ';&' or ';;&'";
            case TOKEN_ERROR           : return "'error'"        ;
    }
    return "unknown token";
}


int is_keyword(char *str)
{
    size_t len = strlen(str);
    if(len == 0) return 0;
    int j;
    for(j = 0; j < keyword_count; j++)
    {
        if(keywords[j].len != len) continue;
        if(strcmp(keywords[j].str, str) == 0) return j;
    }
    return -1;
}

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


void set_token_type(struct token_s *tok)
{
    enum token_type t = TOKEN_EMPTY;
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
                    t = TOKEN_INTEGER; break;
                }
                t = TOKEN_WORD; break;
        }
    }
    else if(tok->text_len == 2)
    {
        switch(tok->text[0])
        {
            case '&':
                if(tok->text[1] == '>')      t = TOKEN_ANDGREAT ;
                else                         t = TOKEN_AND_IF   ;
                break;

            case '|':
                if(tok->text[1] == '&')      t = TOKEN_PIPE_AND ;
                else                         t = TOKEN_OR_IF    ;
                break;
                
            case ';':
                if(tok->text[1] == '&')      t = TOKEN_SEMI_AND ;
                else                         t = TOKEN_DSEMI    ;
                break;
                
            case '>':
                if(tok->text[1] == '>')      t = TOKEN_DGREAT   ;
                else if(tok->text[1] == '&') t = TOKEN_GREATAND ;
                else                         t = TOKEN_CLOBBER  ;
                break;

            case '<':
                if(tok->text[1] == '<')      t = TOKEN_DLESS    ;
                else if(tok->text[1] == '&') t = TOKEN_LESSAND  ;
                else                         t = TOKEN_LESSGREAT;
                break;

            default:
                if(isdigit(tok->text[0]) && isdigit(tok->text[1]))
                {
                    t = TOKEN_INTEGER; break;
                }
                int index = -1;
                if((index = is_keyword(tok->text)) >= 0)
                     t = get_keyword_toktype(index);
                else t = TOKEN_WORD;
                break;
        }
    }
    else
    {
        if(tok->text[0] == '#') t = TOKEN_COMMENT;
        else if(isdigit(tok->text[0]))
        {
            t = TOKEN_INTEGER;
            char *p = tok->text;
            do
            {
                if(*p < '0' || *p > '9') { t = TOKEN_WORD; break; }
            } while(*++p);
        }
        else if(isalpha(tok->text[0]))
        {
            int index = -1;
            if((index = is_keyword(tok->text)) >= 0)
                 t = get_keyword_toktype(index);
            else
            {
                if(strchr(tok->text, '='))
                {
                    t = TOKEN_ASSIGNMENT_WORD;
                    char *p = tok->text;
                    while(*++p != '=')
                        if(!isalnum(*p) && *p != '_')
                        {
                            /* this is included to support bash's extended assignment by using += */
                            if(*p == '+' && p[1] == '=') continue;
                            t = TOKEN_WORD;
                            break;
                        }
                }
                else t = TOKEN_WORD;
            }
        }
        else if(strcmp(tok->text, "<<<") == 0) t = TOKEN_TRIPLELESS;
        else if(strcmp(tok->text, ";;&") == 0) t = TOKEN_SEMI_SEMI_AND;
        else if(strcmp(tok->text, "&>>") == 0) t = TOKEN_AND_GREAT_GREAT;
        else t = TOKEN_WORD;
    }
    if(t == TOKEN_EMPTY) t = TOKEN_UNKNOWN;
    tok->type = t;
}



char *__buf = (char *)NULL;
int   __bufsize = 0;
int   __bufindex = -1;
static struct token_s   __cur_tok = { .type = TOKEN_EMPTY, };
static struct token_s  __prev_tok = { .type = TOKEN_EMPTY, };
static struct token_s *cur_tok    = (struct token_s *)NULL;

struct token_s *create_token(char *str)
{
    memcpy(&__prev_tok, &__cur_tok, sizeof(struct token_s));
    size_t len = strlen(str);
    __cur_tok.text_len = len;
    __cur_tok.text     = str;
    return &__cur_tok;
}

struct token_s *create_singlechar_token(char c)
{
    __buf[0] = c; __buf[1] = '\0';
    return create_token(__buf);
}

struct token_s *create_digraph_token(int di_index)
{
    strcpy(__buf, operators[di_index]);
    return create_token(__buf);
}


int substitute_alias(char *buf, char *alias)
{
    char *alias_val;
    int   name_len ;
    int   val_len  ;
    int   res = 1;
    char *p = buf, *p2, *p3;
    size_t i;
    int   sep = 1;
    while(*p)
    {
        switch(*p)
        {
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
                /* skip escaped quotes */
                if(p != buf && p[-1] == '\\') p++;
                else
                {
                    i = find_closing_quote(p, 0);
                    if(i) p = p+i;
                    else  p++;
                }
                sep = 0;
                break;
                
            default:
                p2 = p;
                while(*p2)
                {
                    if(isalnum(*p2) || *p2 == '_' || *p2 == '!' || *p2 == '%' || *p2 == ',' || *p2 == '@') p2++;
                    else break;
                }
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
                name_len  = p2-p;
                char a[name_len+1];
                strncpy(a, p, name_len);
                a[name_len] = '\0';
                /* prevent alias recursion */
                if(alias && strcmp(a, alias) == 0)
                {
                    p = p2;
                    res = 0;
                    sep = 0;
                    break;
                }
                alias_val = __parse_alias(a);
                if(!alias_val || strcmp(a, alias_val) == 0)
                {
                    p = p2;
                    res = 0;
                    sep = 0;
                    break;
                }
                val_len = strlen(alias_val);
                /* give some room for the alias value */
                p2 = p+name_len;
                p3 = p+val_len ;
                /*
                if(p2 > p3)
                {
                    char *pp = p3;
                    p3 = p2;
                    p2 = pp;
                }
                */
                while((*p3++ = *p2++))
                    ;
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


void add_to_buf(char c)
{
    __buf[__bufindex++] = c;
    if(__bufindex >= __bufsize)
    {
        char *tmp = (char *)realloc(__buf, __bufsize*2);
        if(!tmp)
        {
            errno = ENOMEM;
            return;
        }
        if(tmp != __buf) __buf = tmp;
        __bufsize *= 2;
    }
}

static inline void null_terminate_buf()
{
    __buf[__bufindex] = '\0';
}

struct token_s *get_current_token()
{
    return cur_tok;
}

struct token_s *get_previous_token()
{
    return &__prev_tok;
}


int brace_loop(char nc, struct source_s *src)
{
    add_to_buf(next_char(src));
    char opening_brace = nc, closing_brace;
    if(opening_brace == '{') closing_brace = '}';
    else if(opening_brace == '[') closing_brace = ']';
    else closing_brace = ')';
    int ob_count = 1, cb_count = 0;
    char pc = nc;
    while((nc = next_char(src)) > 0)
    {
        add_to_buf(nc);
        if((nc == '"') || (nc == '\'') || (nc == '`'))
        {
            if(pc == '\\') continue;
            char quote = nc;
            char nc2 = 0;
            /* loop till we get EOF (-1) or ERRCHAR (0) */
            while((nc2 = next_char(src)) > 0)
            {
                add_to_buf(nc2);
                if(nc2 == quote)
                    if(nc != '\\') break;
                nc = nc2;
            }
            continue;
        }
        if((nc == opening_brace) && (pc != '\\')) ob_count++;
        if((nc == closing_brace) && (pc != '\\')) cb_count++;
        if(ob_count == cb_count) break;
        pc = nc;
    }
    if(ob_count != cb_count)
    {
        src->curpos = src->bufsize;
        cur_tok = &eof_token;
        fprintf(stderr, 
                "%s: missing closing brace '%c'\r\n",
                SHELL_NAME, closing_brace);
        return 0;
    }
    return 1;
}

struct token_s *tokenize(struct source_s *src)
{
    if(!src || !src->buffer || !src->bufsize)
    {
        errno = ENODATA;
        //return 0;
        return &eof_token;
    }
    /* first time ever? */
    if(!__buf)
    {
        __bufsize = 1024;
        __buf = (char *)malloc(__bufsize);
        if(!__buf)
        {
            errno = ENOMEM;
            //return 0;
            return &eof_token;
        }
    }
    __bufindex     = 0;
    __buf[0]       = '\0';
    struct token_s *tok = (struct token_s *)NULL;
    long line, chr;
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
    char nc = next_char(src);
    if(nc == ERRCHAR || nc == EOF)
    {
        eof_token.lineno    = src->curline     ;
        eof_token.charno    = src->curchar     ;
        eof_token.linestart = src->curlinestart;
        cur_tok = &eof_token;
        return &eof_token;
    }
    //long line = 0, chr = 0;
    char quote, nc2, pc;

    do
    {
        /*
        if(line == 0)
        {
            line = src->curline;
            chr  = src->curchar;
        }
        */

        switch(nc)
        {
            case  '"':
            case '\'':
            case  '`':
                quote = nc;
                nc2 = 0;
                add_to_buf(nc);
                /*
                 * ANSI-C strings look like $'string' (non-POSIX extension).
                 * they allow escaped single quotes, so we take note of that case.
                 */
                int ansic = (prev_char(src) == '$') ? 1 : 0;
                /* loop till we get EOF (-1) or ERRCHAR (0) */
                while((nc2 = next_char(src)) > 0)
                {
                    add_to_buf(nc2);
                    if(nc2 == quote)
                    {
                        if(quote == '\'' && !ansic) break;
                        if(nc    != '\\') break;
                    }
                    else if(nc2 == '$')
                    {
                        nc2 = peek_char(src);
                        if(nc2 == '{' || nc2 == '(')
                        {
                            if(!brace_loop(nc2, src)) return &err_token;
                        }
                        else if(isalnum(nc2) || nc2 == '*' || nc2 == '@' || nc2 == '#')
                            add_to_buf(next_char(src));
                    }
                    nc = nc2;
                }
                continue;

            case '\\':
                nc2 = next_char(src);
                /* discard backslash+newline combination */
                if(nc2 == NL) continue;
                add_to_buf(nc);
                if(nc2 > 0) add_to_buf(nc2);
                continue;

            case '$':
                add_to_buf(nc);
                nc = peek_char(src);
                if(nc == '{' || nc == '(' || nc == '[')
                {
                    if(!brace_loop(nc, src)) return &err_token;
                }
                else if(isalnum(nc) || nc == '*' || nc == '@' || nc == '#' ||
                                       nc == '!' || nc == '?' || nc == '$')
                    add_to_buf(next_char(src));
                else if(nc == '<')          /* the $< special var of csh/tcsh */
                {
                    add_to_buf(next_char(src));
                    goto prep_token;
                }
                continue;

            case '>':
            case '<':
            case '|':
                /* if an operator delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                add_to_buf(nc);
                pc = peek_char(src);
                /* identify operator tokens */
                if(nc == pc)
                {
                    add_to_buf(next_char(src));
                    pc = peek_char(src);
                    if(nc == '<' && (pc == '-' || pc == '<'))
                        add_to_buf(next_char(src));
                    goto prep_token;
                }
                if((nc == '<' && pc == '>') ||
                   (nc == '>' && pc == '|') ||
                   (nc == '<' && pc == '&') ||
                   (nc == '>' && pc == '&') ||
                   (nc == '|' && pc == '&'))
                {
                    add_to_buf(next_char(src));
                    goto prep_token;
                }
                tok = create_singlechar_token(nc);
                goto token_ready;

            case '&':
            case ';':
                /* if an operator delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                add_to_buf(nc);
                pc = peek_char(src);
                /* identify operator tokens */
                if(nc == pc)
                {
                    add_to_buf(next_char(src));
                    /* identify ';;&' */
                    if(nc == ';')
                    {
                        pc = peek_char(src);
                        if(pc == '&') add_to_buf(next_char(src));
                    }
                    goto prep_token;
                }
                /* identify ';&' */
                if(nc == ';' && pc == '&')
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
                    if(pc == '>') add_to_buf(next_char(src));
                    goto prep_token;
                }
                tok = create_singlechar_token(nc);
                goto token_ready;

            case '{':
            case '}':
                /* if the word delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                /*
                 * braces are recognized as keywords only when they occur after a semi-colon
                 * or a newline character.
                 */
                //if(prev_type == TOKEN_EMPTY || prev_type == TOKEN_SEMI || prev_type == TOKEN_NEWLINE)
                pc = peek_char(src);
                if(nc == '}' || (!isalpha(pc) && pc != '_'))   /* not a {var} I/O redirection word */
                {
                    /* single char delimiters */
                    add_to_buf(nc);
                    tok = create_singlechar_token(nc);
                    goto token_ready;
                }
                unget_char(src);
                if(!brace_loop(nc, src)) return &err_token;
                continue;
                
            case '!':
            case '(':
            case ')':
                /* if the word delimits the current token, delimit it */
                if(__bufindex > 0)
                {
                    unget_char(src);
                    goto prep_token;
                }
                /* recognize ((expr)) structs */
                if(nc == '(')
                {
                    if(peek_char(src) == '(')
                    {
#if 0
                        next_char(src);    /* the 2nd '(' */
                        pc = peek_char(src);
                        if(isspace(pc) || !pc)
                        {
                            /* not ((expr)) but a good ol' '(' */
                            unget_char(src);    /* the 2nd '(' */
                            add_to_buf(nc);
                            tok = create_singlechar_token(nc);
                            goto token_ready;
                        }
                        /* an ((expr)) struct */
                        unget_char(src);    /* the 2nd '(' */
#endif
                        unget_char(src);    /* the 1st '(' */
                        if(!brace_loop(nc, src)) return &err_token;
                        continue;
                    }
                }
                /* '!' must be a separate token */
                else if(nc == '!')
                {
                    pc = peek_char(src);
                    if(pc && !isspace(pc))
                    {
                        add_to_buf(nc);
                        continue;
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
                continue;

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
                if(pc == '>' || pc == '<')
                {
                    nc2 = peek_char(src);
                    if(nc2 == '(')
                    {
                        nc2 = next_char(src);
                        nc2 = peek_char(src);
                        if(nc2 == '(')
                        {
                            add_to_buf(nc);
                            add_to_buf(nc2);
                            int cb = 0;
                            while((nc = next_char(src)) != EOF)
                            {
                                add_to_buf(nc);
                                if(nc == ')') cb++;
                                if(cb == 2) break;
                            }
                            goto prep_token;
                        }
                    }
                }
                /* 
                 * bash identifies # comments in non-interactive shells and interactive
                 * shells with the interactive_comments option.
                 */
                if(option_set('i') && !optionx_set(OPTION_INTERACTIVE_COMMENTS))
                {
                    add_to_buf(nc);
                    continue;
                }
                /* otherwise make it a token */
                /* ...... actually, discard it as per POSIX section 2.3 */
                while((nc = next_char(src)) > 0)
                {
                    if(nc == '\n')
                    {
                        add_to_buf(nc);
                        tok = create_singlechar_token(nc);
                        goto token_ready;
                    }
                }
                continue;

            default:
                add_to_buf(nc);
                continue;
        }
    } while((nc = next_char(src)) != EOF);
        
    if(__bufindex == 0)
    {
        cur_tok = &eof_token;
        return &eof_token;
    }
        
prep_token:
    null_terminate_buf();
    tok = create_token(__buf);
    
token_ready:
    set_token_type(tok);
    if(tok->type == TOKEN_INTEGER)
    {
        char pc = peek_char(src);
        if(pc == '<' || pc == '>') tok->type = TOKEN_IO_NUMBER;
        else tok->type = TOKEN_WORD; 
    }
    /* check words for aliases */
    if(tok->type == TOKEN_WORD && optionx_set(OPTION_EXPAND_ALIASES))
    {
        /*
         * TODO: we should only check the first word of a command for
         *       possible alias substitution. also, we should check if the
         *       last char of an alias substitution is space, in which case
         *       we will need to check the next word for alias substitution.
         */
        struct token_s *prev = get_previous_token();
        if(!prev || is_separator_tok(prev->type))
        {
            if(valid_alias_name(tok->text))
            {
                char *a = get_malloced_str(tok->text);
                if(a)
                {
                    if(substitute_alias(tok->text, NULL))
                    {
                        while(substitute_alias(tok->text, a)) ;
                    }
                    free_malloced_str(a);
                    tok->text_len = strlen(tok->text);
                }
            }
        }
    }
    tok->lineno    = line;
    tok->charno    = chr;
    tok->src       = src;
    tok->linestart = src->curlinestart;

    if(option_set('v')) fprintf(stderr, "%s", tok->text);

    prev_type = tok->type;
    cur_tok = tok;
    return tok;
}

struct token_s *dup_token(struct token_s *tok)
{
    struct token_s *tok2 = (struct token_s *)malloc(sizeof(struct token_s));
    if(!tok2) return NULL;
    memcpy((void *)tok2, (void *)tok, sizeof(struct token_s));
    tok->text_len = strlen(tok->text);
    char *text = (char *)malloc(tok->text_len+1);
    if(!text)
    {
        free(tok2);
        return NULL;
    }
    strcpy(text, tok->text);
    tok2->text = text;
    tok2->text_len = tok->text_len;
    return tok2;
}

void free_token(struct token_s *tok)
{
    if(tok->text) free(tok->text);
    free(tok);
}

/*
 * sometimes we need to check tokens against a number
 * of different types. for example, an if clause can
 * end in elif, else or fi. we need to check all three
 * when parsing if clauses.
 */
int is_token_of_type(struct token_s *tok, int type)
{
    if(tok->type == type) return 1;
    if(type == TOKEN_KEYWORDS_ELIF_ELSE_FI)
    {
        if(tok->type == TOKEN_KEYWORD_ELIF ||
           tok->type == TOKEN_KEYWORD_ELSE ||
           tok->type == TOKEN_KEYWORD_FI)
            return 1;
    }
    /*
     * case items should end in ';;' but sometimes the last 
     * item might end in 'esac'. non-POSIX extensions include
     * ';&' and ';;&' which are used by bash, ksh, zsh et al.
     */
    if(type == TOKEN_DSEMI_ESAC_SEMIAND)
    {
        if(tok->type == TOKEN_KEYWORD_ESAC ||
           tok->type == TOKEN_DSEMI        ||
           tok->type == TOKEN_SEMI_AND     ||
           tok->type == TOKEN_SEMI_SEMI_AND)
            return 1;
    }
    return 0;
}
