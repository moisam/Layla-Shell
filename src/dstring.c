/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2020 (c)
 *
 *    file: dstring.c
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

#include <stdlib.h>
#include <unistd.h>
#include "include/cmd.h"
#include "include/dstring.h"
#include "include/debug.h"

/*************************************************************************
 *
 * Functions to work with dynamic strings (string that grow automatically,
 * as needed).
 *
 *************************************************************************/

#define INIT_BUF_SIZE   1024


/*
 * Initialize dynamic string by allocating a buffer of size init_buf_size.
 * 
 * Returns 1 on success, 0 on error.
 */
int init_str(struct dstring_s *string, size_t init_buf_size)
{
    if(!string)
    {
        return 0;
    }
    
    string->buf_base = malloc(init_buf_size);

    if(!(string->buf_base))
    {
        return 0;
    }
    
    string->buf_base[0] = '\0';
    string->buf_ptr = string->buf_base;
    string->buf_size = init_buf_size;
    string->buf_len = 0;
    
    return 1;
}


/*
 * Append string str, or length str_len, to the given dynamic string, 
 * initializing the dynamic string if not already done. If the buffer
 * is full, the function extends the buffer by doubling it's size.
 * 
 * Returns 1 on success, 0 on error.
 */
int str_append(struct dstring_s *string, char *str, size_t str_len)
{
    if(string->buf_base == NULL)
    {
        if(!init_str(string, (str_len > INIT_BUF_SIZE) ? str_len : INIT_BUF_SIZE))
        {
            return 0;
        }
    }
    else if((string->buf_len + str_len) >= string->buf_size)
    {
        size_t newsz = string->buf_size * 2;
        char *newbuf = realloc(string->buf_base, newsz);
        
        if(!newbuf)
        {
            return 0;
        }
        
        string->buf_base = newbuf;
        string->buf_size *= 2;
        string->buf_ptr = string->buf_base + string->buf_len;
    }
    
    strncpy(string->buf_ptr, str, str_len);
    
    string->buf_ptr += str_len;
    string->buf_len += str_len;
    string->buf_ptr[0] = '\0';
    
    return 1;
}


/*
 * Free the memory used by a dynamic string and resets the dynamic string's
 * pointers to 0.
 * 
 * Returns nothing.
 */
void free_str(struct dstring_s *string)
{
    if(!string)
    {
        return;
    }
    
    if(string->buf_base)
    {
        free(string->buf_base);
    }
    
    string->buf_len = 0;
    string->buf_size = 0;
    string->buf_ptr = NULL;
    string->buf_base = NULL;
}
