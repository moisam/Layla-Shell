/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2018, 2019 (c)
 * 
 *    file: pattern.c
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

#define _GNU_SOURCE         /* FNM_CASEFOLD */

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <regex.h>
#include <fnmatch.h>
#include <locale.h>
#include <glob.h>
#include <sys/stat.h>
#include "backend.h"
#include "../builtins/setx.h"
#include "../error/error.h"
#include "../debug.h"

struct char_class_s
{
    char *name;
    int   namelen;
    int (*func)(int c);
} char_class[] = 
{
    { "alnum" , 7, isalnum  },
    { "alpha" , 7, isalpha  },
    { "blank" , 7, isblank  },
    { "cntrl" , 7, iscntrl  },
    { "digit" , 7, isdigit  },
    { "graph" , 7, isgraph  },
    { "lower" , 7, islower  },
    { "print" , 7, isprint  },
    { "punct" , 7, ispunct  },
    { "space" , 7, isspace  },
    { "upper" , 7, isupper  },
    { "xdigit", 8, isxdigit },
};
const int char_class_count = sizeof(char_class)/sizeof(struct char_class_s);


int has_regex_chars(char *p, size_t len)
{
    char *p2 = p+len;
    char ob = 0, cb = 0;    /* count of opening and closing brackets */
    while(p < p2 && *p)
    {
        switch(*p)
        {
            case '*':
            case '?':
                return 1;
                
            case '[':
                ob++;
                break;
                
            case ']':
                cb++;
                break;
        }
        p++;
    }
    /* do we have a matching number of opening and closing brackets? */
    if(ob && ob == cb) return 1;
    return 0;
}

/*
 * find the shortest or longest prefix of str that matches
 * pattern, depending on the value of longest.
 * return value is the index of 1 after the last character
 * in the prefix, i.e. where you should put a '\0' to get 
 * the prefix.
 */
int match_prefix(char *pattern, char *str, int longest)
{
    if(!pattern || !str) return 0;
    char *s = str+1;
    char  c = *s;
    char *smatch = NULL;
    char *lmatch = NULL;
    while(c)
    {
        *s = '\0';
        if(match_filename(pattern, str, 0, 1))
        {
            if(!smatch)
            {
                if(!longest)
                {
                    *s = c;
                    return s-str;
                }
                smatch = s;
            }
            lmatch = s;
        }
        *s = c;
        c = *(++s);
    }
    /* check the result of the comparison */
    if(lmatch)
    {
        return lmatch-str;
    }
    if(smatch) return smatch-str;
    return 0;
}

/*
 * find the shortest or longest suffix of str that matches
 * pattern, depending on the value of longest.
 * return value is the index of the first character in the
 * matched suffix.
 */
int match_suffix(char *pattern, char *str, int longest)
{
    if(!pattern || !str) return 0;
    char *s = str+strlen(str)-1;
    char *smatch = NULL;
    char *lmatch = NULL;
    while(s > str)
    {
        if(match_filename(pattern, s, 0, 1))
        {
            if(!smatch)
            {
                if(!longest)
                {
                    return s-str;
                }
                smatch = s;
            }
            lmatch = s;
        }
        s--;
    }
    /* check the result of the comparison */
    if(lmatch)
    {
        return lmatch-str;
    }
    if(smatch) return smatch-str;
    return 0;
}

/*
 * print_err is a flag that tells us if we should output an
 * error message in case no match was found.
 */
int match_filename(char *pattern, char *str, int print_err, int ignore)
{
    if(!pattern || !str) return 0;
    /*
     * $FIGNORE is a non-POSIX extension that defines the set of 
     * file names that is ignored when performing file name matching.
     * bash has a similar $GLOBIGNORE variable.
     */
    char *fignore = get_shell_varp("FIGNORE", NULL);
    /* set up the flags */
    int flags = FNM_NOESCAPE | FNM_PATHNAME | FNM_LEADING_DIR;
    if( optionx_set(OPTION_NOCASE_MATCH)) flags |= FNM_CASEFOLD;
    if( optionx_set(OPTION_EXT_GLOB    )) flags |= FNM_EXTMATCH;
    if(!optionx_set(OPTION_DOT_GLOB    )) flags |= FNM_PERIOD  ;
    /* perform the match */
    if(optionx_set(OPTION_GLOB_ASCII_RANGES)) setlocale(LC_ALL, "C");
    int res = fnmatch(pattern, str, flags);
    if(optionx_set(OPTION_GLOB_ASCII_RANGES)) setlocale(LC_ALL, "");
    switch(res)
    {
        case 0:
            if(ignore && fignore)
            {
                //if(match_filename(fignore, str, print_err)) return 0;
                if(match_ignore(fignore, str)) return 0;
            }
            return 1;
        case FNM_NOMATCH: return 0;
        default:
            if(print_err) fprintf(stderr, "%s: failed to match filename(s)\n", SHELL_NAME);
            return 0;
    }
}


/* dummy function to satisfy scandir() */
static int one(const struct dirent *unused)
{
    return 1;
}

struct stat   statbuf;
struct dirent **eps;

/* scans the provided path to look for input files */
int scan_dir(char *path, int report_err)
{
    int n;
    n = scandir(path, &eps, one, alphasort);
    if(n >= 0)
    {
        return n;
    }
    else
    {
        if(report_err)
            BACKEND_RAISE_ERROR(FAILED_TO_OPEN_FILE, path, strerror(errno));
        return 0;
    }
}

char *__next_path    = NULL;
/* gets the next filename from the directory listing */
char *get_next_filename(char *__path, int *n, int report_err)
{
    static int   first_time = 1;
    static int   file_count = 0, index = 0;
    if(__next_path != __path)
    {
        __next_path = __path;
        first_time  = 1;
        index       = 0;
    }
    if(first_time)
    {
        if(!(file_count = scan_dir(__next_path, report_err)))
        {
            /* error or empty directory */
            if(n) *n = 0;
            return NULL;
        }
        first_time = 0;
        if(n) *n = file_count;
    }
loop:
    if(index >= file_count) return NULL;
    /* something funny happens after stating '.' & '..', the first file after
       those is not seen as a file but as a dir. So wipe out the struct to
       prevent this from happening.
     */
    memset((void *)&statbuf, 0, sizeof(struct stat));
    char *name = eps[index]->d_name;
    lstat(name, &statbuf);
    /* skip dot and dot-dot */
    if(S_ISDIR(statbuf.st_mode) &&
       (strcmp(name, "..") == 0 || strcmp(name, ".") == 0))
    {
        index++;
        goto loop;
    }
    return eps[index++]->d_name;
}


char **filename_expand(char *dir, char *path, glob_t *matches)
{
    /* to guard our caller from trying to free an unused struct in case of expansion failure */
    matches->gl_pathc = 0;
    matches->gl_pathv = NULL;
    if(option_set('f')) return NULL;
    if(!dir || !path) return NULL;
    if(strcmp(dir, cwd) != 0)
    {
        if(chdir(dir) == -1) return NULL;
    }
    char *fignore = get_shell_varp("GLOBIGNORE", NULL);
    //glob_t matches;
    /* set up the flags */
    int flags = GLOB_NOESCAPE;
    if(optionx_set(OPTION_ADD_SUFFIX)) flags |= GLOB_MARK  ;
    if(optionx_set(OPTION_DOT_GLOB  )) flags |= GLOB_PERIOD;
    if(option_set('B')) flags |= GLOB_BRACE;
    /* perform the match */
    if(optionx_set(OPTION_GLOB_ASCII_RANGES)) setlocale(LC_ALL, "C");
    int res = glob(path, flags, NULL, matches);
    if(optionx_set(OPTION_GLOB_ASCII_RANGES)) setlocale(LC_ALL, "");
    if(res != 0)
    {
        globfree(matches);
        if(dir != cwd) chdir(cwd);
        return NULL;
    }
    int i;
    if(fignore)
    {
        for(i = 0; i < matches->gl_pathc; i++)
        {
            if(match_ignore(fignore, matches->gl_pathv[i]))
            {
                free(matches->gl_pathv[i]);
                int j;
                for(j = i; j < matches->gl_pathc; j++)
                    matches->gl_pathv[j] = matches->gl_pathv[j+1];
                matches->gl_pathc--;
                i--;
            }
        }
    }
    if(dir != cwd) chdir(cwd);
    return matches->gl_pathv;
}

/*
 * test filename against a colon-separated pattern field to determine if it matches one of the
 * patterns in the field. used when performing filename expansion to determine which file names
 * to ignore. the pattern is usually the value of $FIGNORE, $GLOBIGNORE or $EXECIGNORE.
 */
int match_ignore(char *pattern, char *filename)
{
    char *e1 = pattern;
    int ignore = 0;
    while(*e1)
    {
        char *e2 = e1+1;
        while(*e2 && *e2 != ':') e2++;
        char c = *e2;
        *e2 = '\0';
        if(match_filename(e1, filename, 0, 0))
        {
            ignore = 1;
            *e2 = c;
            break;
        }
        *e2 = c;
        e1 = e2;
    }
    return ignore;
}
