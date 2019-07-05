/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: buffer.c
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

#include <limits.h>
#include <stdlib.h>

long    bufbitmap = 0;
int     bufcount  = 0;
#define maxbufs   (sizeof(long)*CHAR_BIT)

struct buf_s
{
    char **ptr ;
    int    size;
} bufs[maxbufs];

struct buf_s *getbuf(size_t size)
{
    struct buf_s *buf = NULL;
    int i, j = 1;
    /* no buffers in the list, or all buffers are used */
    if(!bufcount)
    {
        char *p = malloc(size);
        if(!p) return NULL;
        for(i = 0; i < maxbufs; i++)
        {
            if(bufs[i].size == 0)
            {
                bufs[i].ptr  = p;
                bufs[i].size = size;
                return &bufs[i];
            }
        }
        return NULL;
    }
    /* find and reuse a suitable buffer */
    for(i = 0; i < maxbufs; i++, j <<= 1)
    {
        if(bufbitmap & j)
        {
            if(bufs[i].size >= size)
            {
                buf = &bufs[i];
                bufbitmap &= ~j;
                bufcount--;
                break;
            }
        }
    }
    return buf;
}

void freebuf(struct buf_s *buf)
{
    char *ptr = buf->ptr, *p2;
    size_t size = buf->size, s2;
    int i, j = 1;
    /*
     * find a suitable slot to insert the new buffer, while keeping
     * order of the buffer sizes from smaller to larger (to ease
     * later retrieval).
     */
    for(i = 0; i < maxbufs; i++, j <<= 1)
    {
        if(bufbitmap & j)
        {
            if(bufs[i].size >= size)
            {
                /* swap sizes */
                s2 = size;
                size = bufs[i].size;
                bufs[i].size = s2;
                /* swap pointers */
                p2 = ptr;
                ptr = bufs[i].ptr;
                bufs[i].ptr = p2;
            }
        }
        else
        {
            bufs[i].size = size;
            bufs[i].ptr  = ptr;
            bufbitmap |= j;
            bufcount++;
            size = 0;
            ptr  = NULL;
            break;
        }
    }
    /* we have a buffer with no place for it */
    if(ptr)
    {
        free(ptr);
    }
}
