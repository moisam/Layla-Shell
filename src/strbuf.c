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

struct hashtab_s *str_hashes = NULL;
char *empty_str = "";
// unsigned char MAGIC[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00 /* 0xDEADBEEF */ };

/* defined in symtab/string_hash.c */
struct hashitem_s *add_hash_itemb(struct hashtab_s *table, char *key, unsigned int value);


void init_str_hashtable()
{
    str_hashes = new_hashtable();
}


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

char *__get_malloced_str(char *str)
{
    char *str2 = (char *)malloc(strlen(str)+1);
    if(!str2) return NULL;
    strcpy(str2, str);
    return str2;
}

char *get_malloced_str(char *str)
{
    if(!str) return NULL;
    if(!*str) return empty_str;

    if(str_hashes)
    {
        struct hashitem_s *entry = get_hash_item(str_hashes, str);
        if(entry)
        {
            entry->refs++;
            return entry->name;
        }
        else
        {
            entry = add_hash_itemb(str_hashes, str, 1);
            if(entry)
            {
                return entry->name;
            }
        }
    }
    return __get_malloced_str(str);
}

char *get_malloced_strl(char *str, int start, int length)
{
    if(!str) return NULL;
    char *s1 = str+start;
    char tmp[length+1];
    strncpy(tmp, s1, length);
    tmp[length]= '\0';
    return get_malloced_str(tmp);
}

void free_malloced_str(char *str)
{
    if(!str || str == empty_str) return;

    if(str_hashes)
    {
        struct hashitem_s *entry = get_hash_item(str_hashes, str);
        if(entry)
        {
#if 0
            /*
             * to avoid accidentally removing a hashed string when it was not actually
             * alloc'd through us, we check the magic number we put at the start of each
             * hashed string. this way, even if we were passed a string that wasn't hashed
             * here, it wouldn't affect us.
             */
            unsigned char *mag = ((unsigned char *)str)-offsetof(struct hashitem_s, name);
            /* this string is not ours. free and return */
            if(memcmp(mag, MAGIC, 4) != 0)
            {
                free(str);
                return;
            }
#endif
            /* this string is hashed. process it */
            entry->refs--;
            if(entry->refs == 0)
            {
                rem_hash_item(str_hashes, str);
            }
            return;
        }
    }
    free(str);
}
