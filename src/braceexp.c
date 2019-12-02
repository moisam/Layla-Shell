/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: braceexp.c
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

#include <ctype.h>
#include <stdlib.h>
#include "cmd.h"
#include "debug.h"

char *add_pre_post(char *str, char *pre, char *post);
char **get_brace_list(char *str, size_t end, size_t *count);
char **get_letter_list(char *xs, char *ys, char *zs, size_t *count);
char **get_num_list(char *xs, char *ys, char *zs, size_t *count);
int add_to_list(char ***__list, char *p0, char *p1, size_t *list_count, size_t *list_size);
int remove_from_list(char **list, int i, size_t *list_count);


/*
 * perform brace expansion, which converts constructs such as [1..10..2] to the string list
 * 1,3,5,7,9. Brace expansions can contain either a range (..) or a comma-separated list.
 * we convert the brace expression into a string array, which is then parsed for the other
 * word expansions.
 *
 * returns the list of brace-expanded words, NULL on error.
 */
char **brace_expand(char *str, size_t *count)
{
    /* check the brace expansion option is set (bash) */
    if(!option_set('B'))
    {
        return NULL;
    }
    size_t i;
    char *p = str;
    char **list = NULL, **list2;
    size_t list_count = 0;
    size_t list_size  = 0;
    size_t list2_count = 0, j = 0;
    
loop:
    while(*p)
    {
        switch(*p)
        {
            /* skip single quoted strings */
            case '\'':
                while(*(++p) && *p != '\'')
                {
                    ;
                }
                break;
                
            /* parse brace expression */
            case '{':
                /* ignore escaped braces and variable expansion braces */
                if(p > str && (p[-1] == '\\' || p[-1] == '$'))
                {
                    break;
                }
                /* find the closing brace */
                i = find_closing_brace(p);
                /* closing brace not found */
                if(i == 0)
                {
                    break;
                }
                /* expand the brace list */
                if((list2 = get_brace_list(p, i, &list2_count)))
                {
                    /* get the prefix (the part before the opening brace) */
                    char *pre = "";
                    if(p > str)
                    {
                        pre = get_malloced_strl(str, 0, p-str);
                    }
                    /* get the postfix (the part after the closing brace) */
                    char *post = p+i+1;
                    /*
                     * affix the parts before and after the brace expression to each item in the list.
                     */
                    for(i = 0; i < list2_count; i++)
                    {
                        list2[i] = add_pre_post(list2[i], pre, post);
                    }
                    /*
                     * remove the original string, containing the brace expression we've just expanded.
                     */
                    if(list)
                    {
                        remove_from_list(list, j, &list_count);
                    }
                    /*
                     * add the expanded items to the list.
                     */
                    for(i = 0; i < list2_count; i++)
                    {
                        char *item = list2[i];
                        if(!add_to_list(&list, item, item+strlen(item), &list_count, &list_size))
                        {
                            goto err;
                        }
                    }
                    /* free temp memory */
                    for(i = 0; i < list2_count; i++)
                    {
                        free_malloced_str(list2[i]);
                    }
                    free(list2);
                    /* continue parsing from the next item, after the one we've just parsed */
                    p = list[j];
                    str = p;
                    goto loop;
                }
                break;
        }
        p++;
    }
    if(list && j < list_count-1)
    {
        p = list[++j];
        str = p;
        goto loop;
    }
    *count = list_count;
    return list;
    
err:
    if(list)
    {
        for(i = 0; i < list_count; i++)
        {
            free_malloced_str(list[i]);
        }
        free(list);
    }
    return NULL;
}


/*
 * affix the parts before and after the brace expression to each resultant string.
 * this is because a brace expression usually comes in the middle of a string, such
 * as "/usr/{local,include}", which gives us "/usr/local" and "/usr/include". in this
 * case, "/usr/" is the prefix, and the suffix is an empty string.
 */
char *add_pre_post(char *str, char *pre, char *post)
{
    char buf[strlen(pre)+strlen(str)+strlen(post)+1];
    if(!*pre && !*post)
    {
        return str;
    }
    strcpy(buf, pre);
    strcat(buf, str);
    strcat(buf, post);
    char *str2 = get_malloced_str(buf);
    if(!str2)
    {
        return str;
    }
    free_malloced_str(str);
    return str2;
}

/*
 * parse a brace expression and return the string array that results from expanding
 * the expression. the first char of str must be '{', and the char at str[end] must
 * be '}'. we store the number of strings in the resultant list in count, so the caller
 * knows how many items to process.
 */
char **get_brace_list(char *str, size_t end, size_t *count)
{
    char *p0 = str+1;
    char *p1 = p0;
    char *p2 = str+end;
    char **list = NULL, **list2 = NULL;
    size_t list_count = 0, list2_count = 0;
    size_t list_size  = 0;
    int ob = 1, cb = 0;
    size_t i;
    
    while(p1 <= p2)
    {
        switch(*p1)
        {
            /* brace expressions can be nested, but we don't process nested expressions here */
            case '{':
                ob++;
                i = find_closing_brace(p1);
                if(i == 0)
                {
                    break;
                }
                p1 += i;
                cb++;
                break;
                
            /*
             * this explicitly is the very last '}'. others should be processed in the '{' case above.
             */
            case '}':
                cb++;
                /* fall through */
                
            case ',':
                if(p1 == str || p1[-1] == '\\')
                {
                    continue;
                }
                if(!add_to_list(&list, p0, p1, &list_count, &list_size))
                {
                    goto err;
                }
                p0 = p1+1;
                break;

            case '.':
                if(p1[1] != '.')
                {
                    break;
                }
                char *x, *y, *z = NULL;
                /* get the first number */
                x = get_malloced_strl(p0, 0, p1-p0);
                if(!x)
                {
                    goto err;
                }
                p0 = p1+2;
                if(!*p0)
                {
                    goto err;
                }
                p1 = p0;
                /* get the second number, after the '..' */
                while(p1 < p2 && *p1 != '.')
                {
                    if(*p1 == '{')
                    {
                        i = find_closing_brace(p1);
                        p1 += i;
                    }
                    p1++;
                }
                if(*p1 && *p1 != '.' && *p1 != '}')
                {
                    goto err;
                }
                y = get_malloced_strl(p0, 0, p1-p0);
                if(!y)
                {
                    goto err;
                }
                /* get the optional third number, after the '..' */
                if(*p1 == '.')
                {
                    p0 = p1+2;
                    p1 = p2;
                    z = get_malloced_strl(p0, 0, p1-p0);
                    if(!z)
                    {
                        goto err;
                    }
                }
                p0 = p1+1;
                /* letter range in the form [x..y[..z]] */
                if(isalpha(*x) && x[1] == '\0' && isalpha(*y) && y[1] == '\0')
                {
                    list2 = get_letter_list(x, y, z, &list2_count);
                    if(!list2)
                    {
                        goto err;
                    }
                }
                /* number range in the form [n1..n2[..step]] */
                else if(isdigit(*x) && isdigit(*y))
                {
                    list2 = get_num_list(x, y, z, &list2_count);
                    if(!list2)
                    {
                        goto err;
                    }
                }
                else
                {
                    break;
                }
                /* add the sublist */
                char **l = list2;
                i = list2_count;
                while(i--)
                {
                    char *item = *l;
                    if(!add_to_list(&list, item, item+strlen(item), &list_count, &list_size))
                    {
                        goto err2;
                    }
                    l++;
                }
                for(i = 0; i < list2_count; i++)
                {
                    free_malloced_str(list2[i]);
                }
                free(list2);
                list2 = NULL;
                break;
        }
        p1++;
    }
    *count = list_count;
    return list;
    
err2:
    if(list2)
    {
        for(i = 0; i < list2_count; i++)
        {
            free_malloced_str(list2[i]);
        }
        free(list2);
    }
    
err:
    if(list)
    {
        for(i = 0; i < list_count; i++)
        {
            free_malloced_str(list[i]);
        }
        free(list);
    }
    return NULL;
}


/*
 * get the letter range that is in the form [x..y[..z]].
 *
 * returns a string list, or NULL in case of error.
 */
char **get_letter_list(char *xs, char *ys, char *zs, size_t *count)
{
    char x = *xs, y = *ys;
    int  z = zs ? atol(zs) : 1;
    int  i = 0;
    char l = x, **p;
    char buf[4];
    char **list = NULL;
    *count = 0;
    if(z < 0)
    {
        z = -z;
    }
    else if(z == 0)
    {
        z = 1;
    }

    if(x <= y)
    {
        while(l <= y)
        {
            i++, l += z;
        }
        list = malloc(i * sizeof(char *));
        if(!list)
        {
            return NULL;
        }
        *count = i;
        p = list;
        while(i--)
        {
            buf[0] = x; buf[1] = '\0';
            *p = get_malloced_str(buf);
            if(*p)
            {
                p++;
            }
            x += z;
        }
    }
    else
    {
        while(l >= y)
        {
            i++, l -= z;
        }
        list = malloc(i * sizeof(char *));
        if(!list)
        {
            return NULL;
        }
        *count = i;
        p = list;
        while(i--)
        {
            buf[0] = x; buf[1] = '\0';
            *p = get_malloced_str(buf);
            if(*p)
            {
                p++;
            }
            x -= z;
        }
    }
    return list;
}


/*
 * get the number range that is in the form [n1..n2[..step]].
 *
 * returns a string list, or NULL in case of error.
 */
char **get_num_list(char *xs, char *ys, char *zs, size_t *count)
{
    long x = atol(xs), y = atol(ys);
    long z = zs ? atol(zs) : 1;
    long i = 0, l = x;
    char **p;
    char buf[32];
    char **list = NULL;
    *count = 0;
    if(z < 0)
    {
        z = -z;
    }
    else if(z == 0)
    {
        z = 1;
    }
    
    if(x <= y)
    {
        while(l <= y)
        {
            i++, l += z;
        }
        list = malloc(i * sizeof(char *));
        if(!list)
        {
            return NULL;
        }
        *count = i;
        p = list;
        while(i--)
        {
            sprintf(buf, "%ld", x);
            *p = get_malloced_str(buf);
            if(*p)
            {
                p++;
            }
            x += z;
        }
    }
    else
    {
        while(l >= y)
        {
            i++, l -= z;
        }
        list = malloc(i * sizeof(char *));
        if(!list)
        {
            return NULL;
        }
        *count = i;
        p = list;
        while(i--)
        {
            sprintf(buf, "%ld", x);
            *p = get_malloced_str(buf);
            if(*p)
            {
                p++;
            }
            x -= z;
        }
    }
    return list;
}


/*
 * add a string to a string list.. the string to be added starts at *p0 and ends at *p1.
 * list_size tells us the count of strings in the list, so that we can realloc the list
 * if the list_count reaches the list_size.
 *
 * returns 1 of the string is added successfully to the list, 0 in case of error.
 * list_count is incremented by 1.
 */
int add_to_list(char ***__list, char *p0, char *p1, size_t *list_count, size_t *list_size)
{
    char **list = *__list;
    int i;
    if(!list)
    {
        list = malloc(8 * sizeof(char *));
        if(!list)
        {
            return 0;
        }
        (*list_size) = 8;
    }
    else if((*list_count) == (*list_size))
    {
        i = (*list_size) << 1;
        list = realloc(list, i * sizeof(char *));
        if(!list)
        {
            return 0;
        }
        (*list_size) = i;
    }
    char *str = get_malloced_strl(p0, 0, p1-p0);
    if(!str)
    {
        return 0;
    }
    list[(*list_count)] = str;
    (*list_count) = (*list_count)+1;
    (*__list) = list;
    return 1;
}


/*
 * remove the i-th string from the string list.
 *
 * returns 1 of the string is successfully removed from the list, 0 in case of error.
 * list_count is decremented by 1.
 */
int remove_from_list(char **list, int i, size_t *list_count)
{
    free_malloced_str(list[i]);
    int j = i;
    int k = (*list_count)-1;
    for( ; j < k; j++)
    {
        list[j] = list[j+1];
    }
    list[j] = NULL;
    *list_count = k;
    return 1;
}
