/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: strbuf.c
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
#include <stddef.h>
#include "cmd.h"
#include "symtab/string_hash.h"
#include "debug.h"

/*
 * the string buffer aims to create a pool of the frequently used strings, instead
 * of wasting malloc calls which will inevitably result in heap memory fragmentation
 * and exhaution. candidates for string buffering are short-length, frquently accessed
 * strings, such as shell variable names. a heredoc, for example, is not a good candidate,
 * as it is either infrequently accessed, or is long. it is up to the caller to decide if
 * the string is a good candidate. if it is, obtain a buffered string by calling get_malloced_str()
 * or get_malloced_strl(). otherwise, call __get_malloced_str() directly. also, if you know you
 * are going to make changes to the string, then call __get_malloced_str() and work on your
 * own copy, as strings returned by get_malloced_str() are shared by others (this is the whole
 * point of it).
 */

/* the strings buffer (hash table) */
struct hashtab_s *str_hashes = NULL;

/* dummy value for an empty string */
char *empty_str = "";


/*
 * initialize the strings buffer.
 */
void init_str_hashtable(void)
{
    str_hashes = new_hashtable();
}


/*
 * return an malloc'd copy of the given str.. call it directly if you want a private
 * copy of str, one that you can modify at will.. also called by get_malloced_str() below.
 * 
 * returns the malloc'd str, or NULL if failed to alloc memory.
 */
char *__get_malloced_str(char *str)
{
    char *str2 = malloc(strlen(str)+1);
    if(!str2)
    {
        return NULL;
    }
    strcpy(str2, str);
    return str2;
}


/*
 * search for the given str in the string buffer.. if not found, __get_malloced_str()
 * is called to alloc a new string, which added to the buffer and returned.
 * 
 * returns the strings buffer entry of str, or NULL if failed to alloc memory.
 */
char *get_malloced_str(char *str)
{
    /* null pointer passed. return error */
    if(!str)
    {
        return NULL;
    }
    /* empty string passed. return the dummy empty string entry */
    if(!*str)
    {
        return empty_str;
    }
    /* search the strings buffer for str */
    if(str_hashes)
    {
        struct hashitem_s *entry = get_hash_item(str_hashes, str);
        if(entry)   /* entry found */
        {
            /* increment the count of references to str */
            entry->refs++;
            return entry->name;
        }
        else        /* entry not found. add a new entry */
        {
            entry = add_hash_itemb(str_hashes, str, 1);
            if(entry)
            {
                return entry->name;
            }
        }
    }
    /* strings buffer is not operational. return an malloc'd string */
    return __get_malloced_str(str);
}


/*
 * this function is similar to get_malloced_str(), except that it doesn't search
 * the buffer for the whole str, it searches for the substring starting at the start
 * index with the given length.
 * 
 * returns the strings buffer entry of str, or NULL if failed to alloc memory.
 */
char *get_malloced_strl(char *str, int start, int length)
{
    if(!str)
    {
        return NULL;
    }
    char *s1 = str+start;
    char tmp[length+1];
    strncpy(tmp, s1, length);
    tmp[length]= '\0';
    return get_malloced_str(tmp);
}


/*
 * decrement the count of str references in the strings buffer.. if the count
 * reaches zero, the string is freed.
 */
void free_malloced_str(char *str)
{
    if(!str || str == empty_str)
    {
        return;
    }
    /* search the strings buffer for str */
    if(str_hashes)
    {
        struct hashitem_s *entry = get_hash_item(str_hashes, str);
        if(entry)   /* entry found */
        {
            /* this string is hashed. process it */
            entry->refs--;
            if(entry->refs <= 0)
            {
                rem_hash_item(str_hashes, str);
            }
        }
        return;
    }
    /* strings buffer is not operational. free the malloc'd string */
    free(str);
}
