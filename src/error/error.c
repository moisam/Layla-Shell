/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include <ctype.h>
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
 * Get the full text of the line where the error occurred, so that we can
 * print the whole line to show the user where the error occurred.
 * 
 * Returns the malloc'd string, or NULL if malloc failed.
 */
char *get_line(struct source_s *src, long linestart, int *tabs)
{
    /* start from the given position in the input string and find the next newline */
    char *buf = src->buffer+linestart;
    char *bufend = buf+src->bufsize;
    char *bufstart = buf;
    (*tabs) = 0;
    
    while(isspace(*buf))
    {
        buf++;
    }
    
    if(!*buf)
    {
        return NULL;
    }
    
    bufstart = buf;
    
    do
    {
        if(*buf == '\n')     /* break at the first newline */
        {
            break;
        }
        else if(*buf == '\t')   /* keep the count of tabs */
        {
            (*tabs)++;
        }
    } while(++buf < bufend);

    /* allocate memory for the string */
    size_t sz = (size_t)(buf-bufstart);
    char *tmp = malloc(sz+1);
    if(!tmp)
    {
        return NULL;
    }

    /* copy the line */
    strncpy(tmp, bufstart, sz);
    tmp[sz] = '\0';

    /* return it */
    return tmp;
}


/*
 * Print an error message given the error details in the error_s struct.
 * The errstr contains the body of the error message, which will be printed
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
    fprintf(stderr, err_format, SOURCE_NAME, err->lineno, err->charno, errstr, line);
    
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
 * Raise a parsing or execution error and print a well-formatted error message
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
            sprintf(err_str, "expected token: %s", err.desc);
            print_err(&err, err_str);
            break;
            
        case UNEXPECTED_TOKEN:
            sprintf(err_str, "unexpected token: %s", err.desc);
            print_err(&err, err_str);
            break;
            
        case MISSING_FOR_NAME:
            print_err(&err, "missing name after `for`");
            break;
            
        case MISSING_SELECT_NAME:
            print_err(&err, "missing name after `select`");
            break;
    }
    fflush(stderr);
}
