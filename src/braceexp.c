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
char **get_brace_list(char *str, size_t end, int *count);
char **get_letter_list(char *xs, char *ys, char *zs, int *count);
char **get_num_list(char *xs, char *ys, char *zs, int *count);
int add_to_list(char ***__list, char *p0, char *p1, int *list_count, int *list_size);
int remove_from_list(char **list, int i, int *list_count);
char **brace_expand(char *str, int *count);


char **brace_expand(char *str, int *count)
{
    size_t i;
    char *p = str;
    char **list = NULL, **list2;
    int list_count = 0;
    int list_size  = 0;
    int list2_count = 0, j = 0;
    
loop:
    while(*p)
    {
        switch(*p)
        {
            case '\'':
                while(*(++p) && *p != '\'') ;
                break;
                
            case '{':
                if(p > str && p[-1] == '\\') break;
                i = find_closing_brace(p);
                if(i == 0) break;
                if((list2 = get_brace_list(p, i, &list2_count)))
                {
                    char *pre = "";
                    if(p > str) pre = get_malloced_strl(str, 0, p-str);
                    char *post = p+i+1;
                    for(i = 0; i < list2_count; i++)
                    {
                        list2[i] = add_pre_post(list2[i], pre, post);
                    }
                    if(list)
                    {
                        remove_from_list(list, j, &list_count);
                    }
                    for(i = 0; i < list2_count; i++)
                    {
                        char *item = list2[i];
                        if(!add_to_list(&list, item, item+strlen(item), &list_count, &list_size)) goto err;
                    }
                    for(i = 0; i < list2_count; i++) free_malloced_str(list2[i]);
                    free(list2);
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
        for(i = 0; i < list_count; i++) free_malloced_str(list[i]);
        free(list);
    }
    return NULL;
}

char *add_pre_post(char *str, char *pre, char *post)
{
    char buf[strlen(pre)+strlen(str)+strlen(post)+1];
    if(!*pre && !*post) return str;
    strcpy(buf, pre);
    strcat(buf, str);
    strcat(buf, post);
    char *str2 = get_malloced_str(buf);
    if(!str2) return str;
    free_malloced_str(str);
    return str2;
}

char **get_brace_list(char *str, size_t end, int *count)
{
    char *p0 = str+1;
    char *p1 = p0;
    char *p2 = str+end;
    char **list = NULL, **list2 = NULL;
    int list_count = 0, list2_count = 0;
    int list_size  = 0;
    int ob = 1, cb = 0;
    int i;
    
    while(p1 <= p2)
    {
        switch(*p1)
        {
            case '{':
                ob++;
                i = find_closing_brace(p1);
                if(i == 0) break;
                //p1 += (i-1);
                p1 += i;
                cb++;
                break;
                
            case '}':
                cb++;
                /* fall through */
                
            case ',':
                if(p1 == str || p1[-1] == '\\') continue;
                /*
                if(ob > 1 && ob == cb+1)
                {
                    char *s = get_malloced_strl(p0, 0, p1-p0);
                    if(!s) goto err;
                    char **list2 = __brace_expand(s);
                }
                */
                if(!add_to_list(&list, p0, p1, &list_count, &list_size)) goto err;
                p0 = p1+1;
                break;

            case '.':
                if(p1[1] != '.') break;
                char *x, *y, *z = NULL;
                x = get_malloced_strl(p0, 0, p1-p0);
                if(!x) goto err;
                p0 = p1+2;
                if(!*p0) goto err;
                p1 = p0;
                while(p1 < p2 && *p1 != '.')
                {
                    if(*p1 == '{')
                    {
                        i = find_closing_brace(p1);
                        p1 += i;
                    }
                    p1++;
                }
                if(*p1 && *p1 != '.' && *p1 != '}') goto err;
                y = get_malloced_strl(p0, 0, p1-p0);
                if(!y) goto err;
                if(*p1 == '.')
                {
                    p0 = p1+2;
                    p1 = p2;
                    /*
                    p1 = p0;
                    while(p1 < p2)
                    {
                        if(*p1 == '{')
                        {
                            i = find_closing_brace(p1);
                            p1 += i;
                        }
                        else if(p1[-1] != '\\' && (*p1 == ',' || *p1 == '.')) break;
                        p1++;
                    }
                    */
                    z = get_malloced_strl(p0, 0, p1-p0);
                    if(!z) goto err;
                }
                p0 = p1+1;
                /* letter range in the form [x..y[..z]] */
                if(isalpha(*x) && x[1] == '\0' && isalpha(*y) && y[1] == '\0')
                {
                    list2 = get_letter_list(x, y, z, &list2_count);
                    if(!list2) goto err;
                }
                /* number range in the form [n1..n2[..step]] */
                else if(isdigit(*x) && isdigit(*y))
                {
                    list2 = get_num_list(x, y, z, &list2_count);
                    if(!list2) goto err;
                }
                else break;
                /* add the sublist */
                char **l = list2;
                i = list2_count;
                while(i--)
                {
                    char *item = *l;
                    if(!add_to_list(&list, item, item+strlen(item), &list_count, &list_size)) goto err2;
                    l++;
                }
                for(i = 0; i < list2_count; i++) free_malloced_str(list2[i]);
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
        for(i = 0; i < list2_count; i++) free_malloced_str(list2[i]);
        free(list2);
    }
    
err:
    if(list)
    {
        for(i = 0; i < list_count; i++) free_malloced_str(list[i]);
        free(list);
    }
    return NULL;
}

char **get_letter_list(char *xs, char *ys, char *zs, int *count)
{
    char x = *xs, y = *ys;
    int  z = zs ? atol(zs) : 1;
    int  i = 0;
    char l = x, **p;
    char buf[4];
    char **list = NULL;
    *count = 0;
    if(z < 0) z = -z;
    else if(z == 0) z = 1;

    if(x <= y)
    {
        while(l <= y) i++, l += z;
        list = malloc(i * sizeof(char *));
        if(!list) return NULL;
        *count = i;
        p = list;
        while(i--)
        {
            buf[0] = x; buf[1] = '\0';
            *p = get_malloced_str(buf);
            if(*p) p++;
            x += z;
        }
    }
    else
    {
        while(l >= y) i++, l -= z;
        list = malloc(i * sizeof(char *));
        if(!list) return NULL;
        *count = i;
        p = list;
        while(i--)
        {
            buf[0] = x; buf[1] = '\0';
            *p = get_malloced_str(buf);
            if(*p) p++;
            x -= z;
        }
    }
    return list;
}

char **get_num_list(char *xs, char *ys, char *zs, int *count)
{
    int  x = atol(xs), y = atol(ys);
    int  z = zs ? atol(zs) : 1;
    int  i = 0, l = x;
    char **p;
    char buf[32];
    char **list = NULL;
    *count = 0;
    if(z < 0) z = -z;
    else if(z == 0) z = 1;
    
    if(x <= y)
    {
        while(l <= y) i++, l += z;
        list = malloc(i * sizeof(char *));
        if(!list) return NULL;
        *count = i;
        p = list;
        while(i--)
        {
            sprintf(buf, "%ld", x);
            *p = get_malloced_str(buf);
            if(*p) p++;
            x += z;
        }
    }
    else
    {
        while(l >= y) i++, l -= z;
        list = malloc(i * sizeof(char *));
        if(!list) return NULL;
        *count = i;
        p = list;
        while(i--)
        {
            sprintf(buf, "%ld", x);
            *p = get_malloced_str(buf);
            if(*p) p++;
            x -= z;
        }
    }
    return list;
}

int add_to_list(char ***__list, char *p0, char *p1, int *list_count, int *list_size)
{
    char **list = *__list;
    int i;
    if(!list)
    {
        list = malloc(8 * sizeof(char *));
        if(!list) return 0;
        (*list_size) = 8;
    }
    else if((*list_count) == (*list_size))
    {
        i = (*list_size) << 1;
        list = realloc(list, i * sizeof(char *));
        if(!list) return 0;
        (*list_size) = i;
    }
    char *str = get_malloced_strl(p0, 0, p1-p0);
    if(!str) return 0;
    list[(*list_count)] = str;
    (*list_count) = (*list_count)+1;
    (*__list) = list;
    return 1;
}

int remove_from_list(char **list, int i, int *list_count)
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
