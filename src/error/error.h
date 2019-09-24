/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: error.h
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

#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>
#include "../scanner/scanner.h"
#include "../scanner/source.h"

/* types of errors */
enum error_code
{
    /* Parser errors */
    EXPECTED_TOKEN,
    UNEXPECTED_TOKEN,
    MISSING_TOKEN,
    MISSING_FOR_NAME,
    MISSING_SELECT_NAME,
    HEREDOC_MISSING_NEWLINE,
    HEREDOC_MISSING_DELIM,
    HEREDOC_EXPECTED_DELIM,
    INVALID_FUNC_NAME,
        
    /* Backend (Interpreter) errors */
    BREAK_OUTSIDE_LOOP,
    CONTINUE_OUTSIDE_LOOP,
    FAILED_TO_FORK,
    FAILED_TO_ADD_JOB,
    FAILED_TO_OPEN_FILE,
    FAILED_TO_EXEC,
    FAILED_TO_OPEN_PIPE,
    FAILED_REDIRECT,
    EMPTY_CASE_WORD,
    INVALID_REDIRECT_FILENO,
    INVALID_ARITHMETIC,
    INVALID_SUBSTITUTION,
    INVALID_ASSIGNMENT,
    INSUFFICIENT_MEMORY,
    UNSET_VARIABLE,
    EXPANSION_ERROR,
    ASSIGNMENT_TO_READONLY,
};

/* structure for error reporting */
struct error_s
{
    enum error_code errcode;        /* type of error */
    long lineno, charno, linestart; /* where was the error token encountered */
    struct source_s *src;           /* source input where error token appeared */
    char *desc;                     /* description of the error */
    char *extra;                    /* extra description -- used by the backend */
};

/* functions to report errors */
void print_err(struct error_s *err, char *errstr);
void raise_error(struct error_s err);

/*
 * marco to raise parsing errors given the error code, the error token and
 * the string description of the error, e.g. if the error is an unexpected token,
 * the tdesc field contains the type of token that was expected, and so on.
 */
#define __PARSER_RAISE_ERROR(code, tok, tdesc)        \
do                                                    \
{                                                     \
    parser_err = 1;                                   \
    raise_error((struct error_s)                      \
    {                                                 \
        .errcode = (code),                            \
        .lineno = (tok->lineno),                      \
        .charno = (tok->charno),                      \
        .src = (tok->src),                            \
        .linestart = (tok->linestart),                \
        .desc = (tdesc),                              \
    });                                               \
} while(0)

/*
 * marco to raise parsing errors given the error code, the error token and
 * the type of token that was expected instead of the error token.
 */
#define PARSER_RAISE_ERROR(code, tok, type)           \
do                                                    \
{                                                     \
    char *tdesc = get_token_description(type);        \
    __PARSER_RAISE_ERROR((code), (tok), (tdesc));     \
} while(0)

/*
 * wrapper for the PARSER_RAISE_ERROR() macro above.
 */
#define PARSER_RAISE_ERROR_DESC(code, tok, tdesc)     \
do                                                    \
{                                                     \
    __PARSER_RAISE_ERROR((code), (tok), (tdesc));     \
} while(0)


/*
 * marco to raise execution errors given the error code, the string description
 * of the error, and an optional extra string description that depends on the
 * type of error we are reporting.
 */
#define BACKEND_RAISE_ERROR(code, edesc, xdesc)       \
do {                                                  \
    raise_error((struct error_s)                      \
    {                                                 \
        .errcode = (code),                            \
        .desc    = (edesc),                           \
        .extra   = (xdesc),                           \
    });                                               \
} while(0)

#endif
