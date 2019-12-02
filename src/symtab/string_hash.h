/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: string_hash.h
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

#ifndef STRING_HASH_H
#define STRING_HASH_H


#ifndef HASHTABLE_INIT_SIZE
#define HASHTABLE_INIT_SIZE     256     /* initial size of the string buffer */
#endif

/* the structure to hold a hashed string */
struct hashitem_s
{
    char   *name;               /* string key */
    union
    {
        char *val;              /* string value */
        long refs;              /*
                                 * number of references to this string.. used by
                                 * the string buffer in strbuf.c.
                                 */
    };
    struct  hashitem_s *next;   /* pointer to the next item */
};

/* string hash table structure */
struct hashtab_s
{
    int     size;               /* maximum number of buckets */
    int     used;               /* number of used buckets */
    struct  hashitem_s **items; /* the buckets array */
};

struct hashtab_s *new_hashtable(void);
struct hashtab_s *new_hashtable_sz(int size);
void   free_hashtable(struct hashtab_s *table);
void   rem_all_items (struct hashtab_s *table, int free_index);
void   rem_hash_item (struct hashtab_s *table, char *key);
struct hashitem_s *add_hash_item(struct hashtab_s *table, char *key, char *value);
struct hashitem_s *add_hash_itemb(struct hashtab_s *table, char *key, long value);
struct hashitem_s *get_hash_item(struct hashtab_s *table, char *key);
void   dump_hashtable(struct hashtab_s *table, char *format);

#endif
