/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2020 (c)
 *
 *    file: dstring.h
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

#ifndef DSTRING_H
#define DSTRING_H


struct dstring_s
{
    char  *buf_base;    /* ptr to buffer base */
    char  *buf_ptr;     /* ptr to cur pos in buffer */
    size_t buf_size;    /* total allocated buffer size */
    size_t buf_len;     /* length of string in buffer */
};


int  init_str(struct dstring_s *string, size_t init_buf_size);
int  str_append(struct dstring_s *string, char *str, size_t str_len);
void free_str(struct dstring_s *string);


#endif
