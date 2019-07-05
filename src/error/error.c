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

char err_format[] = "%s [%d, %d]: Error: %s\r\n%s\r\n";

char *get_line(struct source_s *src, long linestart, int *tabs)
{
    char *buffer = src->buffer;
    long i = linestart;
    *tabs = 0;
    do
    {
        if(buffer[i] == NL ) break;
        if(buffer[i] == TAB) (*tabs)++;
    } while(++i < src->bufsize);
    size_t sz = (size_t)(i-linestart);
    char *tmp = (char *)malloc(sz+1);
    if(!tmp)
    {
        return (char *)NULL;
    }
    strncpy(tmp, buffer+linestart, sz);
    tmp[sz] = '\0';
    return tmp;
}

void print_err(struct error_s *err, char *errstr)
{
    long spaces = err->charno-1;
    int tabs = 0;
    char *line = get_line(err->src, err->linestart, &tabs);
    if(!line) return;
    fprintf(stderr, err_format, SHELL_NAME, err->lineno, err->charno, errstr, line);
    spaces -= tabs;
    //spaces -= err->linestart;
    while(tabs--) fprintf(stderr, "\t");
    if(spaces) fprintf(stderr, "%*s", (int)spaces, " ");
    fprintf(stderr, "^\r\n");
    free(line);
}

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
                        "%s: Error: Missing newline at beginning of heredoc\r\n",
                            SHELL_NAME);
            break;
            
        case HEREDOC_MISSING_DELIM:
            fprintf(stderr, "%s: Error: Missing heredoc delimiter '%s'\r\n", 
                        SHELL_NAME, err.desc);
            break;
            
        case HEREDOC_EXPECTED_DELIM:
            fprintf(stderr, "%s: Error: Expected heredoc delimiter\r\n", SHELL_NAME);
            break;
            
        case INVALID_FUNC_NAME:
            fprintf(stderr, "%s: Error: Invalid function name: %s\r\n", SHELL_NAME, err.desc);
            break;
                        
        /**************************************/
        /* Interpreter Errors                 */
        /**************************************/
        case BREAK_OUTSIDE_LOOP:
            fprintf(stderr,
                            "%s: Error: break clause outside a loop\r\n", SHELL_NAME);
            break;
            
        case CONTINUE_OUTSIDE_LOOP:
            fprintf(stderr,
                            "%s: Error: continue clause outside a loop\r\n", SHELL_NAME);
            break;
            
        case FAILED_TO_FORK:
            fprintf(stderr,
                            "%s: Error: failed to fork: %s\r\n", SHELL_NAME, err.desc);
            break;
            
        case FAILED_TO_ADD_JOB:
            fprintf(stderr, "%s: Error: failed to add job\r\n", SHELL_NAME);
            break;
            
        case FAILED_TO_OPEN_FILE:
            fprintf(stderr,
                            "%s: Error: failed to open %s: %s\r\n",
                            SHELL_NAME, err.desc, err.extra);
            break;
            
        case FAILED_TO_OPEN_PIPE:
            fprintf(stderr, "%s: Error: failed to open pipe: %s\r\n", SHELL_NAME, err.desc);
            break;
            
        case FAILED_TO_EXEC:
            fprintf(stderr,
                            "%s: Error: failed to exec %s: %s\r\n",
                            SHELL_NAME, err.desc, err.extra);
            break;
            
        case FAILED_REDIRECT:
            if(err.desc && err.extra)
            {
                fprintf(stderr, "%s: Error: %s: %s\r\n", SHELL_NAME, err.desc, err.extra);
            }
            else
            {
                fprintf(stderr, "%s: Error: failed redirection: "
                                "incorrect file permissions\r\n", SHELL_NAME);
            }
            break;
            
        case EMPTY_CASE_WORD:
            fprintf(stderr, "%s: Error: empty case word\r\n", SHELL_NAME);
            break;
            
        case INVALID_REDIRECT_FILENO:
            fprintf(stderr,
                            "%s: Error: invalid redirection file number: %s\r\n",
                            SHELL_NAME, err.desc);
            break;
            
        case INSUFFICIENT_MEMORY:
            if(err.desc)
                fprintf(stderr,
                            "%s: Error: insufficient memory for %s\r\n",
                            SHELL_NAME, err.desc);
            else
                fprintf(stderr,
                            "%s: Error: insufficient memory\r\n", SHELL_NAME);
            break;
            
        case INVALID_ARITHMETIC:
            fprintf(stderr, "%s: Error: invalid arithmetic substitution at: '%s'\r\n",
                            SHELL_NAME, err.desc);
            break;
            
        case INVALID_SUBSTITUTION:
            fprintf(stderr, "%s: Error: invalid substitution at: '%s'\r\n",
                            SHELL_NAME, err.desc);
            break;
            
        case UNSET_VARIABLE:
            fprintf(stderr, "%s: %s: %s\r\n", SHELL_NAME, err.desc, err.extra);
            break;
            
        case INVALID_ASSIGNMENT:
            fprintf(stderr, "%s: Error: invalid variable assignment: %s\r\n",
                            SHELL_NAME, err.desc);
            break;
            
        case EXPANSION_ERROR:
            fprintf(stderr, "%s: Expansion error at: '%s'\r\n", SHELL_NAME, err.desc);
            break;
            
        case ASSIGNMENT_TO_READONLY:
            fprintf(stderr, "%s: Error: assignment to readonly variable: %s\r\n",
                            SHELL_NAME, err.desc);
            break;
            
        default:
            break;
            
    }
    fflush(stderr);
}
