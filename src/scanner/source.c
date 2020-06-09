/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: source.c
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

#include <errno.h>
#include "../cmd.h"
#include "source.h"

/*
 * Check if the given char is a space or tab char.
 */
static inline char is_space(char c)
{
    if(c == ' ' || /* c == '\n' || */ c == '\t')
    {
        return 1;
    }
    return 0;
}


/*
 * Return the last char read from input back to the input buffer.
 */
void unget_char(struct source_s *src)
{
    if(src->curpos < 0)     /* invalid pos pointer */
    {
        return;
    }
    src->curpos--;          /* go back one char */

    if(--src->curchar == 0) /* first char in line */
    {
        src->curline--;     /* go back one line */
        
        /* get length of prev line */
        char *p = src->buffer+src->curpos, *p2 = p;
        while(p > src->buffer && *p != '\n')
        {
            p--;
        }
        
        /* new cur char is last char in prev line */
        src->curchar = p2-p;
        
        /* store start pos of prev line */
        src->curlinestart = p-src->buffer;
    }
}


/*
 * Get the previous char we read from input.
 * 
 * Returns the previous character, or ERRCHAR (0) in case of error.
 */
char prev_char(struct source_s *src)
{
    /* sanity checks */
    if(!src || !src->buffer)
    {
        errno = ENODATA;
        return ERRCHAR;
    }

    /* never read from input before? so no previous char */
    if(src->curpos <= 0)
    {
        return ERRCHAR;
    }
    
    return src->buffer[src->curpos-1];
}


/*
 * Get the next char in the input source.
 * 
 * Returns the next character, ERRCHAR (0) in case of error, or EOF (-1)
 * if we reached the end of input.
 */
char next_char(struct source_s *src)
{
    /* sanity checks */
    if(!src || !src->buffer)
    {
        errno = ENODATA;
        return ERRCHAR;
    }
    
    /* first time? adjust source pointers */
    if(src->curpos == INIT_SRC_POS)
    {
        src->curline      =  1;
        src->curchar      =  1;
        src->curpos       = -1;
        src->curlinestart =  0;
    }

    /* did we reach EOF? */
    if(++src->curpos >= src->bufsize)
    {
        src->curpos = src->bufsize;
        return EOF;
    }
    
    /* get the next char */
    char c2 = src->buffer[src->curpos];
    /* if the current char is '\n', adjust line and char pointers */
    if(c2 == '\n')
    //if(c1 == '\n')
    {
        src->curline++;
        src->curchar = 1;
        src->curlinestart = src->curpos;
    }
    else
    {
        /* current char is not '\n', advance the char pointer */
        src->curchar++;
    }
    
    /* return the next char */
    return c2;
}


/*
 * Peek into the next char in the input source. Peeking is similar to getting
 * the next char, except it doesn't 'remove' the char from input, which means
 * it doesn't modify the current position, char and line pointers of src.
 * 
 * Returns the next character, ERRCHAR (0) in case of error, or EOF (-1)
 * if we reached the end of input.
 */
char peek_char(struct source_s *src)
{
    /* sanity checks */
    if(!src || !src->buffer)
    {
        errno = ENODATA;
        return ERRCHAR;
    }

    long pos = src->curpos;
    /* first time? */
    if(pos == INIT_SRC_POS)
    {
        pos++;
    }
    pos++;
    
    /* reached EOF? */
    if(pos >= src->bufsize)
    {
        return EOF;
    }
    
    /* return the next char without adjusting pointers */
    return src->buffer[pos];
}


/*
 * Skip over whitespace characters in the input source.
 */
void skip_white_spaces(struct source_s *src)
{
    char c;
    /* sanity checks */
    if(!src || !src->buffer)
    {
        return;
    }
    /* peek into next char and skip it if its a space/tab */
    while(((c = peek_char(src)) != EOF) && (is_space(c)))
    {
        next_char(src);
    }
}
