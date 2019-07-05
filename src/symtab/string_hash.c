/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: string_hash.c
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
#include <stdio.h>
#include <string.h>
#include "../cmd.h"
#include "string_hash.h"
#include "../debug.h"

/* defined in ../strbuf.c */
char *__get_malloced_str(char *str);

/***********************************************
 * Functions for hashing a string to get its 
 * table index. You can replace this function 
 * if you want to use another hashing algorithm.
 ***********************************************/
 
/*
 * default values recommended by http://isthe.com/chongo/tech/comp/fnv/
 */
const uint32_t fnv1a_prime = 0x01000193; /* 16777619 */
const uint32_t fnv1a_seed  = 0x811C9DC5; /* 2166136261 */

inline uint32_t fnv1a_hash_byte(unsigned char b, uint32_t hash)
{
    return (b ^ hash) * fnv1a_prime;
} 
 
uint32_t fnv1a(char *text, uint32_t hash)
{
    if(!text) return 0;
    unsigned char *p = (unsigned char *)text;
    while(*p) hash = (*p++ ^ hash) * fnv1a_prime;        
    return hash;
}

/*
 * TODO: If you want to use another hashing algorithm,
 *       change the function call to fnv1a() to any other 
 *       function.
 */
uint32_t calc_hash(struct hashtab_s *table, char *text)
{
    if(!table || !text) return 0;
    return fnv1a(text, fnv1a_seed) % table->size;
}

/*****************************************
 * Functions for manipulating hash tables.
 *****************************************/

struct hashtab_s *new_hashtable_sz(int size)
{
    struct hashtab_s *table = malloc(sizeof(struct hashtab_s));
    if(!table)
    {
        fprintf(stderr, "%s: insufficient memory for saving hash table\n", SHELL_NAME);
        return NULL;
    }
    table->size   = size;
    table->used   = 0;
    size_t itemsz = size * sizeof(struct hashitem_s *);
    table->items  = malloc(itemsz);
    if(!table->items)
    {
        free(table);
        fprintf(stderr, "%s: insufficient memory for saving hash table\n", SHELL_NAME);
        return NULL;
    }
    memset(table->items, 0, itemsz);
    return table;
}

struct hashtab_s *new_hashtable()
{
    return new_hashtable_sz(HASHTABLE_INIT_SIZE);
}

void free_hashtable(struct hashtab_s *table)
{
    if(!table) return;
    rem_all_items(table, 1);
    free(table);
}

/*
 * free_index is a flag that indicates if we want to release
 * the memory allocated to the items structure in the table.
 */
void rem_all_items(struct hashtab_s *table, int free_index)
{
    if(!table || !table->items) return;
    if(table->used)
    {
        struct hashitem_s **h1 = table->items;
        struct hashitem_s **h2 = table->items + table->size;
        for( ; h1 < h2; h1++)
        {
            struct hashitem_s *entry = *h1;
            while(entry)
            {
                struct hashitem_s *next = entry->next;
                if(entry->name) free(entry->name);
                if(entry->val ) free(entry->val );
                free(entry);
                entry = next;
            }
        }
    }
    table->used = 0;
    if(free_index)
    {
        free(table->items);
        table->items = NULL;
    }
}

void rem_hash_item(struct hashtab_s *table, char *key)
{
    int index = calc_hash(table, key);
    struct hashitem_s *e = table->items[index];
    struct hashitem_s *p = NULL;
    while(e)
    {
        if(is_same_str(e->name, key))
        {
            /* if this is the first item in the list, adjust the table
             * pointer to point to the next item. Otherwise, ajust
             * the previous item's pointer to point to the next item.
             */
            if(!p) table->items[index] = e->next;
            else   p->next = e->next;
            /* free the memory used by this entry */
            if(e->val) free(e->val);
            free(e->name);
            free(e);
            table->used--;
            break;
        }
        p = e, e = e->next;
    }
}

struct hashitem_s *add_hash_item(struct hashtab_s *table, char *key, char *value)
{
    if(!table || !key || !value) return NULL;
    /* do not duplicate an existing entry */
    struct hashitem_s *entry = NULL;
    if((entry = get_hash_item(table, key)))
    {
        /* don't alloc new string if the new value is the same as 
         * the old value.
         */
        if(entry->val && value && is_same_str(entry->val, value)) return entry;
        if(entry->val) free(entry->val);
        entry->val = __get_malloced_str(value);
        return entry;
    }
    /* add a new entry */
    entry = (struct hashitem_s *)malloc(sizeof(struct hashitem_s));
    if(!entry)
    {
        fprintf(stderr, "%s: failed to malloc hashtable item\r\n", SHELL_NAME);
        return NULL;
    }
    memset((void *)entry, 0, sizeof(struct hashitem_s));
    entry->name = __get_malloced_str(key  );
    entry->val  = __get_malloced_str(value);
    /*
     * we are acting on the premise that a newly added variable
     * is bound to be accessed sooner than later, this is why we
     * add the new variable to the start of the linked list for 
     * that bucket. Think about it, when you declare a variable,
     * you are probably going to use it soon, right? Or maybe wrong,
     * but this is how we do it here :)
     */
    int index = calc_hash(table, key);
    entry->next = table->items[index];
    table->items[index] = entry;
    table->used++;
    return entry;
}

/*
 * this function is similar to add_hash_item(), except it treats value as a literal
 * value, an unsigned int that is destined to be accessed as the 'refs'
 * union member. this function should only be called by the string buffer.
 * DO NOT use this function at all, unless you are modifying the string buffer
 * in strbuf.c.
 */
struct hashitem_s *add_hash_itemb(struct hashtab_s *table, char *key, unsigned int value)
{
    if(!table || !key) return NULL;
    /* do not duplicate an existing entry */
    struct hashitem_s *entry = NULL;
    if((entry = get_hash_item(table, key)))
    {
        entry->refs = value;
        return entry;
    }
    /* add a new entry */
    entry = (struct hashitem_s *)malloc(sizeof(struct hashitem_s));
    if(!entry)
    {
        fprintf(stderr, "%s: failed to malloc hashtable item\r\n", SHELL_NAME);
        return NULL;
    }
    memset((void *)entry, 0, sizeof(struct hashitem_s));
    entry->name = __get_malloced_str(key);
    entry->refs = value;
    /*
     * we are acting on the premise that a newly added variable
     * is bound to be accessed sooner than later, this is why we
     * add the new variable to the start of the linked list for 
     * that bucket. Think about it, when you declare a variable,
     * you are probably going to use it soon, right? Or maybe wrong,
     * but this is how we do it here :)
     */
    int index = calc_hash(table, key);
    entry->next = table->items[index];
    table->items[index] = entry;
    table->used++;
    return entry;
}

struct hashitem_s *get_hash_item(struct hashtab_s *table, char *key)
{
    if(!table || !key) return NULL;
    int index = calc_hash(table, key);
    struct hashitem_s *entry = table->items[index];
    while(entry)
    {
        if(is_same_str(entry->name, key)) return entry;
        entry = entry->next;
    }
    return NULL;
}


void dump_hashtable(struct hashtab_s *table, char *format)
{
    if(!format) format = "%s=%s\n";
    if(table->used)
    {
        struct hashitem_s **h1 = table->items;
        struct hashitem_s **h2 = table->items + table->size;
        for( ; h1 < h2; h1++)
        {
            struct hashitem_s *entry = *h1;
            while(entry)
            {
                printf(format, entry->name, entry->val);
                entry = entry->next;
            }
        }
    }
}
