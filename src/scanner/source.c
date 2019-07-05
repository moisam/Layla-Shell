/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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

void unget_char(struct source_s *src)
{
    if(src->curpos < 0) return;
    src->curpos--;
    if(--src->curchar == 0)
    {
        src->curline--;
        /* get length of prev line */
        char *p = src->buffer+src->curpos, *p2 = p;
        while(p > src->buffer && *p != '\n') p--;
        src->curchar = p2-p;
        /* store start pos of prev line */
        src->curlinestart = p-src->buffer;
    }
}

char prev_char(struct source_s *src)
{
    /* sanity checks */
    if(!src || !src->buffer)
    {
        errno = ENODATA;
        return ERRCHAR;
    }
    /* first time? */
    if(src->curpos <= 0) return ERRCHAR;
    return src->buffer[src->curpos-1];
}

char next_char(struct source_s *src)
{
    /* sanity checks */
    if(!src || !src->buffer)
    {
        errno = ENODATA;
        return ERRCHAR;
    }
    char c1 = 0;
    /* first time? */
    if(src->curpos == -2)
    {
        src->curline      =  1;
        src->curchar      =  1;
        src->curpos       = -1;
        src->curlinestart =  0;
    }
    else c1 = src->buffer[src->curpos];

    char c2 = src->buffer[++src->curpos];
    /* EOF? */
    if(src->curpos >= src->bufsize)
    {
        src->curpos = src->bufsize;
        return EOF;
    }
    if(c1 == NL)
    {
        src->curline++;
        src->curchar = 1;
        src->curlinestart = src->curpos;
    }
    else src->curchar++;
    return c2;
}

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
    if(pos == -2) pos++;
    pos++;
    /* EOF? */
    if(pos >= src->bufsize) return EOF;
    return src->buffer[pos];
}

void skip_white_spaces(struct source_s *src)
{
    char c;
    while(((c = peek_char(src)) != EOF) && (is_space(c)))
        next_char(src);
}
