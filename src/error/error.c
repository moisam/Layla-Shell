/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: error.c
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
#include "../cmd.h"
#include "error.h"
#include "../debug.h"

/*
 * POSIX's Consequences of Shell Errors table:
 * ===========================   ================     ===============
 *     Error                     Special Built-In     Other Utilities
 * ===========================   ================     ===============
 * Shell language syntax error   Shall exit           Shall exit
 * Utility syntax error          Shall exit           Shall not exit
 *    (option or operand error)
 * Redirection error             Shall exit           Shall not exit
 * Variable assignment error     Shall exit           Shall not exit
 * Expansion error               Shall exit           Shall exit
 * Command not found             N/A                  May exit
 * Dot script not found          Shall exit           N/A
 * ===========================   ================     ===============
 */

/* general format of the shell's error messages */
char err_format[] = "%s [%d, %d]: error: %s\n%s\n";

/*
 * get the full text of the line where the error occurred, so that we can
 * print the whole line to show the user where the error occurred.
 * 
 * returns the malloc'd string, or NULL if malloc failed.
 */
char *get_line(struct source_s *src, long linestart, int *tabs)
{
    /* start from the given position in the input string and find the next newline */
    char *buffer = src->buffer;
    long i = linestart;
    *tabs = 0;
    do
    {
        if(buffer[i] == '\n')     /* break at the first newline */
        {
            break;
        }
        else if(buffer[i] == '\t')   /* keep the count of tabs */
        {
            (*tabs)++;
        }
    } while(++i < src->bufsize);
    /* allocate memory for the string */
    size_t sz = (size_t)(i-linestart);
    char *tmp = malloc(sz+1);
    if(!tmp)
    {
        return NULL;
    }
    /* copy the line */
    strncpy(tmp, buffer+linestart, sz);
    tmp[sz] = '\0';
    /* return it */
    return tmp;
}


/*
 * print an error message given the error details in the error_s struct.
 * the errstr contains the body of the error message, which will be printed
 * after the "error:" prompt (see the err_format field at the top of this file).
 */
void print_err(struct error_s *err, char *errstr)
{
    long spaces = err->charno-1;
    int  tabs = 0;
    /* get the line where the error occurred */
    char *line = get_line(err->src, err->linestart, &tabs);
    if(!line)
    {
        return;
    }
    /* print the error message */
    fprintf(stderr, err_format, SHELL_NAME, err->lineno, err->charno, errstr, line);
    /* print a caret pointer '^' that points at the error token */
    spaces -= tabs;
    while(tabs--)
    {
        fprintf(stderr, "\t");
    }
    if(spaces)
    {
        fprintf(stderr, "%*s", (int)spaces, " ");
    }
    fprintf(stderr, "^\n");
    free(line);
}


/*
 * raise a parsing or execution error and print a well-formatted error message
 * according to the error whose details are passed in the error_s struct.
 */
void raise_error(struct error_s err)
{
    char err_str[256];
    switch(err.errcode)
    {
        /**************************************/
        /* Parser Errors                      */
        /**************************************/
        case EXPECTED_TOKEN:
            sprintf(err_str, "Expected token: %s", err.desc);
            print_err(&err, err_str);
            break;
            
        case UNEXPECTED_TOKEN:
            sprintf(err_str, "Unexpected token: %s", err.desc);
            print_err(&err, err_str);
            break;
            
        case MISSING_TOKEN:
            sprintf(err_str, "Missing token: %s", err.desc);
            print_err(&err, err_str);
            break;
            
        case MISSING_FOR_NAME:
            print_err(&err, "Missing name after for");
            break;
            
        case MISSING_SELECT_NAME:
            print_err(&err, "Missing name after select");
            break;
                        
        case HEREDOC_MISSING_NEWLINE:
            fprintf(stderr,
                        "%s: error: Missing newline at beginning of heredoc\n",
                            SHELL_NAME);
            break;
            
        case HEREDOC_MISSING_DELIM:
            fprintf(stderr, "%s: error: Missing heredoc delimiter '%s'\n", 
                        SHELL_NAME, err.desc);
            break;
            
        case HEREDOC_EXPECTED_DELIM:
            fprintf(stderr, "%s: error: Expected heredoc delimiter\n", SHELL_NAME);
            break;
            
        case INVALID_FUNC_NAME:
            fprintf(stderr, "%s: error: Invalid function name: %s\n", SHELL_NAME, err.desc);
            break;
                        
        /**************************************/
        /* Interpreter Errors                 */
        /**************************************/
        case BREAK_OUTSIDE_LOOP:
            fprintf(stderr,
                            "%s: error: break clause outside a loop\n", SHELL_NAME);
            break;
            
        case CONTINUE_OUTSIDE_LOOP:
            fprintf(stderr,
                            "%s: error: continue clause outside a loop\n", SHELL_NAME);
            break;
            
        case FAILED_TO_FORK:
            fprintf(stderr,
                            "%s: error: failed to fork: %s\n", SHELL_NAME, err.desc);
            break;
            
        case FAILED_TO_ADD_JOB:
            fprintf(stderr, "%s: error: failed to add job\n", SHELL_NAME);
            break;
            
        case FAILED_TO_OPEN_FILE:
            fprintf(stderr, "%s: error: failed to open %s: %s\n",
                            SHELL_NAME, err.desc, err.extra);
            break;
            
        case FAILED_TO_OPEN_PIPE:
            fprintf(stderr, "%s: error: failed to open pipe: %s\n", SHELL_NAME, err.desc);
            break;
            
        case FAILED_TO_EXEC:
            fprintf(stderr, "%s: error: failed to exec %s: %s\n",
                            SHELL_NAME, err.desc, err.extra);
            break;
            
        case FAILED_REDIRECT:
            if(err.desc && err.extra)
            {
                fprintf(stderr, "%s: error: %s: %s\n", SHELL_NAME, err.desc, err.extra);
            }
            else
            {
                fprintf(stderr, "%s: error: failed redirection: "
                                "incorrect file permissions\n", SHELL_NAME);
            }
            break;
            
        case EMPTY_CASE_WORD:
            fprintf(stderr, "%s: error: empty case word\n", SHELL_NAME);
            break;
            
        case INVALID_REDIRECT_FILENO:
            fprintf(stderr,
                            "%s: error: invalid redirection file number: %s\n",
                            SHELL_NAME, err.desc);
            break;
            
        case INSUFFICIENT_MEMORY:
            if(err.desc)
                fprintf(stderr,
                            "%s: error: insufficient memory for %s\n",
                            SHELL_NAME, err.desc);
            else
                fprintf(stderr,
                            "%s: error: insufficient memory\n", SHELL_NAME);
            break;
            
        case INVALID_ARITHMETIC:
            fprintf(stderr, "%s: error: invalid arithmetic substitution at: '%s'\n",
                            SHELL_NAME, err.desc);
            break;
            
        case INVALID_SUBSTITUTION:
            fprintf(stderr, "%s: error: invalid substitution at: '%s'\n",
                            SHELL_NAME, err.desc);
            break;
            
        case UNSET_VARIABLE:
            fprintf(stderr, "%s: %s: %s\n", SHELL_NAME, err.desc, err.extra);
            break;
            
        case INVALID_ASSIGNMENT:
            fprintf(stderr, "%s: error: invalid variable assignment: %s\n",
                            SHELL_NAME, err.desc);
            break;
            
        case EXPANSION_ERROR:
            fprintf(stderr, "%s: Expansion error at: '%s'\n", SHELL_NAME, err.desc);
            break;
            
        case ASSIGNMENT_TO_READONLY:
            fprintf(stderr, "%s: error: assignment to readonly variable: %s\n",
                            SHELL_NAME, err.desc);
            break;
            
        default:
            break;
            
    }
    fflush(stderr);
}
