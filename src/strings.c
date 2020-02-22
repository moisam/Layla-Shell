/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 *
 *    file: strings.c
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
#include <unistd.h>
#include <sys/time.h>
#include "cmd.h"
#include "debug.h"

/****************************************
 *
 * String manipulation functions
 *
 ****************************************/

/*
 * convert string str letters to uppercase.
 *
 * returns 1 on success, 0 if a NULL str pointer is given.
 */
int strupper(char *str)
{
    if(!str)
    {
        return 0;
    }
    while(*str)
    {
        *str = toupper(*str);
        str++;
    }
    return 1;
}


/*
 * convert string str letters to lowercase.
 *
 * returns 1 on success, 0 if a NULL str pointer is given.
 */
int strlower(char *str)
{
    if(!str)
    {
        return 0;
    }
    while(*str)
    {
        *str = tolower(*str);
        str++;
    }
    return 1;
}


/*
 * append character chr to string str at position pos (zero based).
 */
inline void strcat_c(char *str, int pos, char chr)
{
    str[pos++] = chr;
    str[pos  ] = '\0';
}


/*
 * search string for any one of the passed characters.
 *
 * returns a char pointer to the first occurence of any of the characters,
 * NULL if none found.
 */
char *strchr_any(char *string, char *chars)
{
    if(!string || !chars)
    {
        return NULL;
    }
    char *s = string;
    while(*s)
    {
        char *c = chars;
        while(*c)
        {
            if(*s == *c)
            {
                return s;
            }
            c++;
        }
        s++;
    }
    return NULL;
}


/*
 * returns 1 if the two strings are the same, 0 otherwise.
 */
int is_same_str(char *s1, char *s2)
{
    if((strlen(s1) == strlen(s2)) && strcmp(s1, s2) == 0)
    {
        return 1;
    }
    return 0;
}


/*
 * return the passed string value, quoted in a format that can
 * be used for reinput to the shell.. if the 'add_quotes' flag is non-zero,
 * the function surrounds val in double quotes.. if escape_sq is non-zero,
 * all single quotes are backslash-escaped.
 */
char *quote_val(char *val, int add_quotes, int escape_sq)
{
    /* escape single quotes only if not inside double quotes */
    escape_sq = (escape_sq && !add_quotes);

    char *res = NULL;
    size_t len;
    
    /* empty string */
    if(!val || !*val)
    {
        len = add_quotes ? 3 : 1;
        res = malloc(len);
        if(!res)
        {
            return NULL;
        }
        strcpy(res, add_quotes ? "\"\"" : "");
        return res;
    }

    /* count the number of quotes needed */
    len = 0;
    char *v = val, *p;
    while(*v)
    {
        switch(*v)
        {
            case '\'':
                if(escape_sq)
                {
                    len++;
                }
                break;
                    
            case '\\':
            case  '`':
            case  '$':
            case  '"':
                len++;
                break;
        }
        v++;
    }
    len += strlen(val);
    /* add two for the opening and closing quotes (optional) */
    if(add_quotes)
    {
        len += 2;
    }
    /* alloc memory for quoted string */
    res = malloc(len+1);
    if(!res)
    {
        return NULL;
    }
    p = res;
    /* add opening quote (optional) */
    if(add_quotes)
    {
        *p++ = '"';
    }
    /* copy quoted val */
    v = val;
    while(*v)
    {
        switch(*v)
        {
            case '\'':
                if(escape_sq)
                {
                    /* add '\' for quoting */
                    *p++ = '\\';
                }
                /* copy char */
                *p++ = *v++;
                break;
                    
            case '\\':
            case  '`':
            case  '$':
            case  '"':
                /* add '\' for quoting */
                *p++ = '\\';
                /* copy char */
                *p++ = *v++;
                break;

            default:
                /* copy next char */
                *p++ = *v++;
                break;
        }
    }
    /* add closing quote (optional) */
    if(add_quotes)
    {
        *p++ = '"';
    }
    *p = '\0';
    return res;
}


/*
 * convert a string array to a single string with members
 * separated by spaces.. the last member must be NULL, or else
 * we will loop until we SIGSEGV! a non-zero dofree argument
 * causes the passed list to be freed (if the caller is too
 * lazy to free its own memory).
 */
char *list_to_str(char **list)
{
    int i;
    int len = 0;
    int count = 0;
    char *p, *p2;
    
    if(!list)
    {
        return NULL;
    }

    /* get the count */
    for(i = 0; list[i]; i++)
    {
        ;
    }
    count = i;
    
    int lens[count];
    /* get total length and each item's length */
    for(i = 0; i < count; i++)
    {
        /* add 1 for the separating space char */
        lens[i] = strlen(list[i])+1;
        len += lens[i];
    }
    
    p = malloc(len+1);
    if(!p)
    {
        return NULL;
    }
    *p = 0;
    p2 = p;
    
    /* now copy the items */
    for(i = 0; i < count; i++)
    {
        sprintf(p, "%s ", list[i]);
        p += lens[i];
    }
    p[-1]= '\0';
    
    return p2;
}


/*
 * get the system-defined maximum line length.. if no value is defined by the
 * system, use our own default value.
 */
int get_linemax(void)
{
    int line_max;
#ifdef LINE_MAX
    line_max = LINE_MAX;
#else
    line_max = sysconf(_SC_LINE_MAX);
    if(line_max <= 0)
    {
        line_max = DEFAULT_LINE_MAX;
    }
#endif
    return line_max;
}


/*
 * get the next entry in a colon-separated string, such as the ones we use in
 * shell variables $SHELLOPTS, $MAILPATH and $HISTCONTROL.
 * the 'colon_list' argument contains a pointer to the colon-separated string, 
 * in which we'll store a pointer to our current position in the string, so 
 * that each call to this function, using the same argument, will keep 
 * processing the list until it's finished.. the caller should initialize 
 * 'colon_list', and shouldn't modify it once we're called.
 * 
 * returns the malloc'd next entry, or NULL if we reached the end of the string,
 * or an error occurrs.
 */
char *next_colon_entry(char **colon_list)
{
    char *s1 = *colon_list;
    char *res = NULL;
    
    while(*s1 && *s1 == ':')
    {
        s1++;
    }
    
    if(!*s1)
    {
        return NULL;
    }
    
    char *s2 = s1+1;
    while(*s2 && *s2 != ':')
    {
        s2++;
    }
    
    int len = s2-s1;
    res = malloc(len+1);
    if(!res)
    {
        return NULL;
    }
    
    memcpy(res, s1, len);
    res[len] = '\0';

    (*colon_list) = s2;
    return res;
}


/*
 * similar to next_colon_entry(), except that it creates a pathname by
 * concatenating the next colon entry and the given filename.. used by
 * cd when it searches $CDPATH to find a directory, by search_path() in
 * helpfunc.c when it searches $PATH to find a file, etc.
 * if 'use_dot' is non-zero, we prepend './' to the pathname if the next 
 * colon entry is an empty string.
 * 
 * returns the malloc'd pathname, or NULL if we reached the end of the string,
 * or an error occurrs.
 */
char *next_path_entry(char **colon_list, char *filename, int use_dot)
{
    char *s1 = colon_list ? *colon_list : NULL;
    char *s2 = s1;
    
    if(!s2 || !*s2)
    {
        return NULL;
    }
    
    /* skip to the next colon */
    while(*s2 && *s2 != ':')
    {
        s2++;
    }
    
    /* get path lengths */
    size_t flen = strlen(filename);
    size_t plen = s2-s1;
    /*
     * we add three for \0 terminator, possible /, and leading dot
     * (in case the path is NULL and we need to append ./)
     */
    int tlen = plen+flen+3;
    char *path = malloc(tlen);
    if(!path)
    {
        return NULL;
    }

    /* empty colon entry */
    if(!plen)
    {
        if(use_dot)
        {
            strcpy(path, "./");
        }
        else
        {
            path[0] = '\0';
        }
    }
    /* non-empty colon entry */
    else
    {
        strncpy(path, s1, plen);
        if(path[plen-1] != '/')
        {
            path[plen  ] = '/';
            path[plen+1] = '\0';
        }
        else
        {
            path[plen  ] = '\0';
        }
    }

    /* form the new path as the path segment plus the filename */
    strcat(path, filename);
    
    /* skip the colons */
    while(*s2 && *s2 == ':')
    {
        s2++;
    }
    (*colon_list) = s2;
    
    /* return the result */
    return path;
}


/*
 * alloc memory for, or extend the host (or user) names buffer if needed..
 * in the first call, the buffer is initialized to 32 entries.. subsequent
 * calls result in the buffer size doubling, so that it becomes 64, 128, ...
 * count is the number of used entries in the buffer, while len is the number
 * of alloc'd entries (size of buffer divided by sizeof(char **)).
 *
 * returns 1 if the buffer is alloc'd/extended, 0 otherwise.
 */
int check_buffer_bounds(int *count, int *len, char ***buf)
{
    if(*count >= *len)
    {
        if(!(*buf))
        {
            /* first call. alloc memory for the buffer */
            *buf = malloc(32*sizeof(char **));
            if(!(*buf))
            {
                return 0;
            }
            *len = 32;
        }
        else
        {
            /* subsequent calls. extend the buffer */
            int newlen = (*len) << 1;
            char **hn2 = realloc(*buf, newlen*sizeof(char **));
            if(!hn2)
            {
                return 0;
            }
            *buf = hn2;
            *len = newlen;
        }
    }
    return 1;
}


/*
 * convert a time value of the format [secs[.usecs]] to two separate parts.
 * 
 * returns 1 if the conversion is successful, 0 otherwise.
 */
int get_secs_usecs(char *str, struct timeval *res)
{
    char *strend;
    long i, j;
    
    if(!str || !*str)
    {
        return 0;
    }

    i = strtol(str, &strend, 10);

    if(*strend)
    {
        if(*strend == '.')
        {
            str = strend+1;
            j = strtol(str, &strend, 10);
            if(*strend)
            {
                return 0;
            }
            res->tv_usec = j;
        }
        else
        {
            return 0;
        }
    }
    
    res->tv_sec = i;
    return 1;
}


/*
 * alloc 'size' bytes and return the pointer.. also, make the string 'ready for reading'
 * by putting null at the first byte.
 */
char *alloc_string_buf(int size)
{
    char *s = malloc(size);
    if(!s)
    {
        return NULL;
    }
    s[0] = '\0';
    return s;
}


/*
 * check the string buffer's and extend it if needed, adjusting the pointers if
 * the buffer is realloced.
 * 
 * returns 1 on success, 0 on failure.
 */
int may_extend_string_buf(char **buf, char **buf_end, char **buf_ptr, int *buf_size)
{
    /* if buffer is full, extend it */
    if(buf_ptr == buf_end)
    {
        (*buf_size) *= 2;
        
        char *buf2 = realloc(buf, (*buf_size));
        if(!buf2)
        {
            return 0;
        }

        (*buf_ptr) = buf2 + ((*buf_end) - (*buf));
        (*buf) = buf2;
        (*buf_end) = (*buf) + (*buf_size) - 1;
    }

    return 1;
}
