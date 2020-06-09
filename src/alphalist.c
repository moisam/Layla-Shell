/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 *
 *    file: alphalist.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cmd.h"

/*
 * This file provides implementation to alphabetically-sorted lists. Each list is
 * represented by a struct alpha_list_s (which is defined in cmd.h). Working with
 * alpha lists is simple. We declare the list struct as follows:
 *
 *     struct alpha_list_s list;
 *
 * We then initialize the list by calling init_alpha_list():
 *
 *     init_alpha_list(&list);
 *
 * We then create a string by malloc'ing its memory, or alternatively by calling
 * alpha_list_make_str(), passing it the format string and its arguments in the
 * same way we do with printf et al.:
 *
 *     char *str = alpha_list_make_str("%s %s", arg1, arg2);
 *
 * We then call add_to_alpha_list() to add the string to the alpha list:
 *
 *     add_to_alpha_list(&list, str);
 *
 * We print the items in the list, each on a separate line, by calling
 * print_alpha_list(), passing it the list struct:
 *
 *     print_alpha_list(&list);
 *
 * After we've finished with the list, we free its memory by calling
 * free_alpha_list(), again passing it the list struct:
 *
 *     free_alpha_list(&list);
 */


/*
 * Initialize an alpha list struct (we don't alloc memory for the struct here,
 * you must alloc your own memory).
 */
void init_alpha_list(struct alpha_list_s *list)
{
    if(!list)
    {
        return;
    }
    
    memset(list, 0, sizeof(struct alpha_list_s));
}


/*
 * Free the memory used by an alpha list struct and its strings.
 */
void free_alpha_list(struct alpha_list_s *list)
{
    if(!list || list->count == 0)
    {
        return;
    }
    
    int count = list->count;
    
    while(count--)
    {
        free(list->items[count]);
    }
    
    free(list->items);
}


/*
 * Print the string in an alpha list, each on a separate line.
 */
void print_alpha_list(struct alpha_list_s *list)
{
    if(!list || list->count == 0)
    {
        return;
    }
    
    int i = 0, count = list->count;
    
    for( ; i < count; i++)
    {
        printf("%s\n", list->items[i]);
    }
}


/*
 * Helper function used by sort (see below) to sort two strings alphabetically.
 */
static int str_compare(const void *a, const void *b)
{
    return strcmp(*(const char**)a, *(const char**)b);
}


/*
 * Sort an alpha list's strings.
 */
void sort(char *list[], int count)
{
    qsort(list, count, sizeof(char *), str_compare);
}


/*
 * Add the pre-alloced string str to the given list (we don't alloc memory for
 * the string here, you must alloc your own memory).
 */
void add_to_alpha_list(struct alpha_list_s *list, char *str)
{
    if(!list || !str)
    {
        return;
    }
    
    /* extend the list or create it if necessary */
    if(check_buffer_bounds(&(list->count), &(list->len), &(list->items)))
    {
        list->items[list->count++] = str;
        sort(list->items, list->count);
    }
}


/*
 * Call vsnprintf() to make a string and return the malloc'd string.
 *
 * NOTE: This code is taken from the vfprintf manpage (see `man vfprintf`).
 */
char *alpha_list_make_str(const char *fmt, ...)
{
    int size = 0;
    char *p = NULL;
    va_list ap;

    /* Determine required size */
    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if(size < 0)
    {
        return NULL;
    }

    size++;             /* For '\0' */
    p = malloc(size);
    if(p == NULL)
    {
        return NULL;
    }

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if(size < 0)
    {
        free(p);
        return NULL;
    }

    return p;
}
