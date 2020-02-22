/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
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
 * we will use FNV-1a hashing.. the following variables and functions implement
 * this hashing function.. these are the default values recommended by:
 * http://isthe.com/chongo/tech/comp/fnv/.
 */
const uint32_t fnv1a_prime = 0x01000193; /* 16777619 */
const uint32_t fnv1a_seed  = 0x811C9DC5; /* 2166136261 */

/*
 * has a byte using the FNV-1a hasing function.
 * returns the hashed byte.
 */
inline uint32_t fnv1a_hash_byte(unsigned char b, uint32_t hash)
{
    return (b ^ hash) * fnv1a_prime;
} 

/*
 * the FNV-1a hasing function.
 * returns a 32-bit hash index.
 */
uint32_t fnv1a(char *text, uint32_t hash)
{
    if(!text)
    {
        return 0;
    }
    unsigned char *p = (unsigned char *)text;
    while(*p)
    {
        hash = (*p++ ^ hash) * fnv1a_prime;
    }
    return hash;
}

/*
 * calculate and return the hash index of the given string.
 * 
 * TODO: If you want to use another hashing algorithm, change the
 *       function call to fnv1a() to any other function.
 */
uint32_t calc_hash(struct hashtab_s *table, char *text)
{
    if(!table || !text)
    {
        return 0;
    }
    return fnv1a(text, fnv1a_seed) % table->size;
}

/*************************************************
 * Functions for manipulating string hash tables.
 *************************************************/

/*
 * allocate a new hash table and initialize its structure.
 *
 * returns the table struct, or NULL if the table could not be allocated.
 */
struct hashtab_s *new_hashtable_sz(int size)
{
    struct hashtab_s *table = malloc(sizeof(struct hashtab_s));
    if(!table)
    {
        PRINT_ERROR("%s: insufficient memory for creating hash table\n", SHELL_NAME);
        return NULL;
    }
    table->size   = size;               /* use the given size */
    table->used   = 0;                  /* empty bucket list */
    size_t itemsz = size * sizeof(struct hashitem_s *);
    table->items  = malloc(itemsz);     /* alloc space for buckets */
    if(!table->items)
    {
        free(table);
        PRINT_ERROR("%s: insufficient memory for creating hash table\n", SHELL_NAME);
        return NULL;
    }
    memset(table->items, 0, itemsz);    /* init buckets list */
    return table;
}


/*
 * allocate a new hash table and initialize its structure.. the table is
 * given the default size, which is the value of the HASHTABLE_INIT_SIZE macro.
 *
 * returns the table struct, or NULL if the table could not be allocated.
 */
struct hashtab_s *new_hashtable(void)
{
    return new_hashtable_sz(HASHTABLE_INIT_SIZE);
}


/*
 * free the memory used by a hash table, as well as its contained strings.
 */
void free_hashtable(struct hashtab_s *table)
{
    if(!table)
    {
        return;
    }
    rem_all_items(table, 1);
    free(table);
}

/*
 * remove all items from a hash table, freeing memory used to store
 * the keys and values.. free_index is a flag that indicates if we want to
 * release the memory used to store the buckets list or not.
 */
void rem_all_items(struct hashtab_s *table, int free_index)
{
    if(!table || !table->items)
    {
        return;
    }
    /* check if there are used buckets in the hash table */
    if(table->used)
    {
        struct hashitem_s **h1 = table->items;
        struct hashitem_s **h2 = table->items + table->size;
        /* iterate through the buckets list */
        for( ; h1 < h2; h1++)
        {
            /* each bucket contains a linked list of items. iterate through them */
            struct hashitem_s *entry = *h1;
            while(entry)
            {
                struct hashitem_s *next = entry->next;
                /* free the key string */
                if(entry->name)
                {
                    free(entry->name);
                }
                /* free the value string */
                if(entry->val)
                {
                    free(entry->val);
                }
                /* free the entry itself and move to the next entry */
                free(entry);
                entry = next;
            }
        }
    }
    table->used = 0;
    if(free_index)
    {
        /* free the buckets list */
        free(table->items);
        table->items = NULL;
    }
}


/*
 * remove a string from a hash table, given pointers to the string and the
 * hash table.
 */
void rem_hash_item(struct hashtab_s *table, char *key)
{
    /* calc the key hash and get the table's bucket */
    int index = calc_hash(table, key);
    struct hashitem_s *e = table->items[index];
    struct hashitem_s *p = NULL;
    /*
     * as the bucket can contain more than one entry, our target entry can
     * be anywhere in the bucket's linked list.. iterate through the items
     * until we find our entry.
     */
    while(e)
    {
        /* found the string */
        if(is_same_str(e->name, key))
        {
            /*
             * if this is the first item in the list, adjust the table
             * pointer to point to the next item. Otherwise, ajust
             * the previous item's pointer to point to the next item.
             */
            if(!p)
            {
                table->items[index] = e->next;
            }
            else
            {
                p->next = e->next;
            }
            /* free the memory used by this entry's value */
            if(e->val)
            {
                free(e->val);
            }
            /* free the key string */
            free(e->name);
            /* and the entry itself */
            free(e);
            /* decrement the count of used entries in the table */
            table->used--;
            break;
        }
        /* check the next entry */
        p = e, e = e->next;
    }
}


/*
 * add a key/value pair to a symbol table.. first check if the key is already
 * in the table or not, so as not to duplicate an entry.. if not, create an entry
 * for the string, add it to the table, set its value and then return the new
 * entry.. if the entry is not already in the table, create a new entry, set its
 * value and return it.. in case of insufficient memory, return NULL.
 */
struct hashitem_s *add_hash_item(struct hashtab_s *table, char *key, char *value)
{
    if(!table || !key || !value)
    {
        return NULL;
    }
    /* do not duplicate an existing entry */
    struct hashitem_s *entry = NULL;
    /* entry exists */
    if((entry = get_hash_item(table, key)))
    {
        /* don't alloc new string if the new value is the same as the old value. */
        if(entry->val && value && is_same_str(entry->val, value))
        {
            return entry;
        }
        /* free the old value */
        if(entry->val)
        {
            free(entry->val);
        }
        /* save the new value */
        entry->val = __get_malloced_str(value);
        /* return the entry struct */
        return entry;
    }
    /* add a new entry */
    entry = malloc(sizeof(struct hashitem_s));
    if(!entry)
    {
        PRINT_ERROR("%s: failed to malloc hashtable item\n", SHELL_NAME);
        return NULL;
    }
    /* initialize it */
    memset(entry, 0, sizeof(struct hashitem_s));
    /* save the key/value pair */
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
    /* return the new entry */
    return entry;
}

/*
 * this function is similar to add_hash_item(), except it treats value as a literal
 * value, an unsigned int that is destined to be accessed as the 'refs'
 * union member. this function should only be called by the string buffer.
 * 
 * WARNING: DO NOT use this function at all, unless you are modifying the string
 *          buffer in strbuf.c and you know what you're doing.
 */
struct hashitem_s *add_hash_itemb(struct hashtab_s *table, char *key, long value)
{
    if(!table || !key)
    {
        return NULL;
    }
    /* do not duplicate an existing entry */
    struct hashitem_s *entry = NULL;
    if((entry = get_hash_item(table, key)))
    {
        entry->refs = value;
        return entry;
    }
    /* add a new entry */
    entry = malloc(sizeof(struct hashitem_s));
    if(!entry)
    {
        PRINT_ERROR("%s: failed to malloc hashtable item\n", SHELL_NAME);
        return NULL;
    }
    /* initialize it */
    memset(entry, 0, sizeof(struct hashitem_s));
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
    /* return the new entry */
    return entry;
}


/*
 * search for a string in a symbol table.
 * returns the entry for the given string, or NULL if its not found.
 */
struct hashitem_s *get_hash_item(struct hashtab_s *table, char *key)
{
    if(!table || !key)
    {
        return NULL;
    }
    /* hash the string */
    int index = calc_hash(table, key);
    /* get the table's bucket */
    struct hashitem_s *entry = table->items[index];
    /* search the bucket's linked list for the string */
    while(entry)
    {
        /* found the string */
        if(is_same_str(entry->name, key))
        {
            return entry;
        }
        /* not found. check the next entry */
        entry = entry->next;
    }
    /* string not found. return NULL */
    return NULL;
}


/*
 * dump a string hash table, by printing the symbols, their keys and values.
 * useful for debugging (but not currently used by the shell).
 */
void dump_hashtable(struct hashtab_s *table, char *format)
{
    if(!format)
    {
        format = "%s=%s\n";
    }
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
