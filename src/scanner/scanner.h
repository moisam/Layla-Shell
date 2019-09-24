/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: scanner.h
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

#ifndef SCANNER_H
#define SCANNER_H

#include "source.h"

/* typedefs for the lexical scanner */

enum token_type
{
        /* general type tokes */
        TOKEN_EMPTY,
        TOKEN_UNKNOWN,
        TOKEN_COMMENT,
        TOKEN_EOF,
        TOKEN_ERROR,
        TOKEN_WORD,
        TOKEN_ASSIGNMENT_WORD,
        TOKEN_NAME,
        TOKEN_NEWLINE,
        TOKEN_IO_NUMBER,        /* number preceding an I/O operator */
        /* POSIX Operators */
        TOKEN_AND_IF,           /* '&&' */
        TOKEN_OR_IF,            /* '||' */
        TOKEN_DSEMI,            /* ';;' */
        TOKEN_DLESS,            /* '<<' */
        TOKEN_DGREAT,           /* '>>' */
        TOKEN_LESSAND,          /* '<&' */
        TOKEN_GREATAND,         /* '>&' */
        TOKEN_LESSGREAT,        /* '<>' */
        TOKEN_DLESSDASH,        /* '<<-' */
        TOKEN_CLOBBER,          /* '>|' */
        /* POSIX Shell Keywords */
        TOKEN_KEYWORD_IF,
        TOKEN_KEYWORD_THEN,
        TOKEN_KEYWORD_ELSE,
        TOKEN_KEYWORD_ELIF,
        TOKEN_KEYWORD_FI,
        TOKEN_KEYWORD_DO,
        TOKEN_KEYWORD_DONE,
        TOKEN_KEYWORD_CASE,
        TOKEN_KEYWORD_ESAC,
        TOKEN_KEYWORD_WHILE,
        TOKEN_KEYWORD_UNTIL,
        TOKEN_KEYWORD_FOR,
        TOKEN_KEYWORD_LBRACE,   /* '{' (yes, that's a POSIX keyword) */
        TOKEN_KEYWORD_RBRACE,   /* '}' (ditto) */
        TOKEN_KEYWORD_BANG,     /* '!' (ditto) */
        TOKEN_KEYWORD_IN,
        /* non-POSIX Shell Keywords and Operators */
        TOKEN_KEYWORD_SELECT,   /* the 'select' keyword */
        TOKEN_KEYWORD_FUNCTION, /* the 'function' keyword */
        TOKEN_KEYWORD_TIME,     /* the 'time' keyword */
        TOKEN_SEMI_AND,         /* ';&'  */
        TOKEN_SEMI_SEMI_AND,    /* ';;&' */
        TOKEN_SEMI_OR,          /* ';|', equivalent in function to ';;&'  */
        TOKEN_PIPE_AND,         /* '|&'  */
        TOKEN_TRIPLELESS,       /* '<<<' */
        TOKEN_ANDGREAT,         /* '&>'  */
        TOKEN_AND_GREAT_GREAT,  /* '&>>' */
        /* others */
        TOKEN_OPENBRACE,        /* '(' */
        TOKEN_CLOSEBRACE,       /* ')' */
        TOKEN_PIPE,             /* '|' */
        TOKEN_LESS,             /* '<' */
        TOKEN_GREAT,            /* '>' */
        TOKEN_SEMI,             /* ';' */
        TOKEN_AND,              /* '&' */
        TOKEN_INTEGER,
        /* special case for ElIf-Else-Fi keywords, used by the parser */
        TOKEN_KEYWORDS_ELIF_ELSE_FI,
        /* special case for Esac/;; keywords, used by the parser in POSIX mode */
        TOKEN_DSEMI_ESAC,
        /* special case for Esac/;;/;&/;| keywords, used by the parser in non-POSIX mode */
        TOKEN_DSEMI_ESAC_SEMIAND_SEMIOR,
        /* unknown keyword */
        TOKEN_KEYWORD_NA
};

/* the token struct that is returned by the lexical scanner */
struct token_s
{
        enum   token_type type;     /* type of token */
        long   lineno, charno;      /* line and char number where token is found */
        long   linestart;           /* start of line where token is found (for error msgs) */
        struct source_s *src;       /* source of input */
        int    text_len;            /* length of token text */
        char   *text;               /* token text */
};

/* the special EOF token, which indicates the end of input */
extern struct token_s eof_token;

/* functions to manipulate tokes */
char  *get_token_description(enum token_type type);
void   set_token_type(struct token_s *tok);
struct token_s *tokenize(struct source_s *src);
struct token_s *get_current_token();
struct token_s *get_previous_token();
struct token_s *dup_token(struct token_s *tok);
void   free_token(struct token_s *tok);
int    is_token_of_type(struct token_s *tok, enum token_type type);
int    is_keyword(char *str);
int    is_separator_tok(enum token_type type);

#endif
