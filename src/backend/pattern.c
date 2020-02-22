/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2018, 2019, 2020 (c)
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


/*
 * check if the string *p has any regular expression (regex) characters,
 * which are *, ?, [ and ].
 */
int has_glob_chars(char *p, size_t len)
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
    if(ob && ob == cb)
    {
        return 1;
    }
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
    if(!pattern || !str)
    {
        return 0;
    }

    char *s = str+1;
    char  c = *s;
    char *smatch = NULL;
    char *lmatch = NULL;
    while(c)
    {
        *s = '\0';
        if(match_pattern(pattern, str))
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
    
    if(smatch)
    {
        return smatch-str;
    }
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
    if(!pattern || !str)
    {
        return 0;
    }
    char *s = str+strlen(str)-1;
    char *smatch = NULL;
    char *lmatch = NULL;
    while(s > str)
    {
        if(match_pattern(pattern, s))
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
    
    if(smatch)
    {
        return smatch-str;
    }
    return 0;
}


/*
 * check if the string str matches the given pattern.
 * print_err is a flag that tells us if we should output an
 * error message in case no match was found.
 *
 * returns 1 if we have a match, 0 otherwise.
 */
int match_filename(char *pattern, char *str, int print_err, int ignore)
{
    if(!pattern || !str)
    {
        return 0;
    }
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
                if(match_ignore(fignore, str))
                {
                    return 0;
                }
            }
            return 1;
            
        case FNM_NOMATCH:
            return 0;
            
        default:
            if(print_err)
            {
                PRINT_ERROR("%s: failed to match filename(s)\n", SHELL_NAME);
            }
            return 0;
    }
}


/*
 * similar to match_filename(), but matches strings for variable expansion
 * and others.
 *
 * returns 1 if we have a match, 0 otherwise.
 */
int match_pattern(char *pattern, char *str)
{
    if(!pattern || !str)
    {
        return 0;
    }

    /* set up the flags */
    int flags = FNM_LEADING_DIR;
    if(optionx_set(OPTION_NOCASE_MATCH))
    {
        flags |= FNM_CASEFOLD;
    }

    if(optionx_set(OPTION_EXT_GLOB))
    {
        flags |= FNM_EXTMATCH;
    }

    /* perform the match */
    if(optionx_set(OPTION_GLOB_ASCII_RANGES))
    {
        setlocale(LC_ALL, "C");
    }
    
    int res = fnmatch(pattern, str, flags);
    
    if(optionx_set(OPTION_GLOB_ASCII_RANGES))
    {
        setlocale(LC_ALL, "");
    }

    return (res == 0) ? 1 : 0;
}


/*
 * match a string to a pattern using POSIX extended regex syntax (used when
 * the =~ operator is passed to the test builtin).
 */
int match_pattern_ext(char *pattern, char *str)
{
    /* set up the flags */
    int flags = REG_EXTENDED;
    if(optionx_set(OPTION_NOCASE_MATCH))
    {
        flags |= REG_ICASE;
    }

    regex_t regex;
    int result = regcomp (&regex, pattern, flags);

    /* Non-zero result means error. */
    if(result)
    {
        fprintf(stderr, "%s: regex match failed: ", SHELL_NAME);
        switch(result)
        {
            case REG_BADBR:
                fprintf(stderr, "invalid ‘\\{…\\}’ construct\n");
                break;
                
            case REG_BADPAT:
                fprintf(stderr, "syntax error\n");
                break;
                
            case REG_BADRPT:
            fprintf(stderr, "repetition operator (‘?’ or ‘*’) in a bad position\n");
                break;
                
            case REG_ECOLLATE:
                fprintf(stderr, "invalid collating element\n");
                break;
                
            case REG_ECTYPE:
                fprintf(stderr, "invalid character class name\n");
                break;
                
            case REG_EESCAPE:
                fprintf(stderr, "regex ends with ‘\\’\n");
                break;
                
            case REG_ESUBREG:
                fprintf(stderr, "invalid number in the ‘\\digit’ construct\n");
                break;
                
            case REG_EBRACK:
                fprintf(stderr, "unbalanced square brackets\n");
                break;
                
            case REG_EPAREN:
                fprintf(stderr, "extended regex with unbalanced parentheses, "
                                "or basic regex with unbalanced ‘\\(’ and ‘\\)’\n");
                break;
                
            case REG_EBRACE:
                fprintf(stderr, "unbalanced ‘\\{’ and ‘\\}’\n");
                break;
                
            case REG_ERANGE:
                fprintf(stderr, "range expression has invalid point(s)\n");
                break;
                
            case REG_ESPACE:
                fprintf(stderr, "%s\n", strerror(ENOMEM));
                break;
                
            default:
                fprintf(stderr, "unknown syntax error\n");
        }
        return 0;
    }

    int match = 0;
    result = regexec(&regex, str, 0, NULL, 0);
    if(!result)
    {
        match = 1;
    }
    else if(result != REG_NOMATCH)
    {
        /* function returned an error. get size of buffer required for error message. */
        size_t length = regerror (result, &regex, NULL, 0);
        char buffer[length];
        (void) regerror (result, &regex, buffer, length);
        fprintf(stderr, "%s: regex match failed: %s\r\n", SHELL_NAME, buffer);
    }

    /* Free the memory allocated from regcomp(). */
    regfree (&regex);
    return match;
}


/* dummy function to satisfy scandir() */
static int one(const struct dirent *unused __attribute__((unused)))
{
    return 1;
}

struct stat   statbuf;
struct dirent **eps;

/*
 * scan the provided path to look for input files.
 */
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
        {
            PRINT_ERROR("%s: failed to open `%s`: %s\n", 
                        SHELL_NAME, path, strerror(errno));
        }
        return 0;
    }
}


char *__next_path    = NULL;

/*
 * gets the next filename from the directory listing.
 */
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
            if(n)
            {
                *n = 0;
            }
            return NULL;
        }
        first_time = 0;
        if(n)
        {
            *n = file_count;
        }
    }
loop:
    if(index >= file_count)
    {
        return NULL;
    }
    /*
     * something funny happens after stating '.' & '..', the first file after
       those is not seen as a file but as a dir. So wipe out the struct to
       prevent this from happening.
     */
    memset(&statbuf, 0, sizeof(struct stat));
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


/*
 * perform pathname (or filename) expansion, matching files in the given *dir to the
 * given *path, which is treated as a regex pattern that specifies which filename(s)
 * we should match.
 *
 * returns a char ** pointer to the list of matched filenames, or NULL if nothing matched.
 */
char **get_filename_matches(char *pattern, glob_t *matches)
{
    /* to guard our caller from trying to free an unused struct in case of expansion failure */
    matches->gl_pathc = 0;
    matches->gl_pathv = NULL;

    if(!pattern)
    {
        return NULL;
    }

    char *fignore = get_shell_varp("GLOBIGNORE", NULL);

    /* set up the flags */
    int flags = 0;
    if(optionx_set(OPTION_DOT_GLOB  )) flags |= GLOB_PERIOD;
    if(option_set('B')) flags |= GLOB_BRACE;

    /* perform the match */
    if(optionx_set(OPTION_GLOB_ASCII_RANGES)) setlocale(LC_ALL, "C");
    int res = glob(pattern, flags, NULL, matches);
    if(optionx_set(OPTION_GLOB_ASCII_RANGES)) setlocale(LC_ALL, "");

    if(res != 0)
    {
        globfree(matches);
        return NULL;
    }

    size_t i;
    if(fignore)
    {
        for(i = 0; i < matches->gl_pathc; i++)
        {
            if(match_ignore(fignore, matches->gl_pathv[i]))
            {
                free(matches->gl_pathv[i]);
                size_t j;
                for(j = i; j < matches->gl_pathc; j++)
                {
                    matches->gl_pathv[j] = matches->gl_pathv[j+1];
                }
                matches->gl_pathc--;
                i--;
            }
        }
    }

    return matches->gl_pathv;
}


/*
 * test filename against a colon-separated pattern field to determine if it 
 * matches one of the patterns in the field. used when performing filename 
 * expansion to determine which file names to ignore. the pattern is usually 
 * the value of $FIGNORE, $GLOBIGNORE or $EXECIGNORE.
 */
int match_ignore(char *pattern, char *filename)
{
    int ignore = 0;
    char *s;
    while((s = next_colon_entry(&pattern)))
    {
        if(match_filename(s, filename, 0, 0))
        {
            ignore = 1;
            free(s);
            break;
        }
        free(s);
    }

    return ignore;
}
