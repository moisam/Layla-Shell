/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2018, 2019, 2020, 2024 (c)
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
#include "../include/debug.h"
#include "../include/dstring.h"


#define APPEND(c)                           \
{                                           \
    buf[0] = (c);                           \
    buf[1] = '\0';                          \
    if(!str_append(&pat_string, buf, 1))    \
    {                                       \
        goto memerr;                        \
    }                                       \
}

#define APPEND2(c1, c2)                     \
{                                           \
    buf[0] = (c1);                          \
    buf[1] = (c2);                          \
    buf[2] = '\0';                          \
    if(!str_append(&pat_string, buf, 2))    \
    {                                       \
        goto memerr;                        \
    }                                       \
}


/*
 * Check if the string *p has any regular expression (regex) characters,
 * which are *, ?, [ and ].
 */
int has_glob_chars(char *p, size_t len)
{
    char *p2 = p+len;
    char ob = 0, cb = 0;    /* Count of opening and closing brackets */
    
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
    
    /* Do we have a matching number of opening and closing brackets? */
    if(ob && ob == cb)
    {
        return 1;
    }
    
    return 0;
}


/*
 * Convert a shell matching pattern (such as the one used in case conditionals)
 * to the format accepted by the regex() function. This conversion includes
 * converting certain characters, such as:
 * 
 * Shell pattern            regex() pattern
 * -------------            ---------------
 * *                        .*
 * ?                        .
 * "string"                 escape some characters & remove quotes
 * 'string'                 same as above
 * 
 * The following characters are escaped if they appear inside quotes, so that
 * regex() will treat them literally:
 * 
 * * ? ( | & ) [ ] \
 * 
 */
char *shell_pattern_to_regex(char *pat)
{
    struct dstring_s pat_string;
    size_t len = strlen(pat);
    size_t i;
    char *p = pat;
    char *p2;
    char buf[4];
    char c;
    
    if(!init_str(&pat_string, len+1))
    {
        INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "pattern matching");
        return NULL;
    }
    
    while(*p)
    {
        switch(*p)
        {
            case '"':
            case '\'':
                i = find_closing_quote(p, 0, 0);
#if 0
                for(p2 = p+1; *p2; p2++)
                {
                    if(*p2 == '\\')
                    {
                        p2++;
                        continue;
                    }
                    
                    if(*p2 == *p)
                    {
                        break;
                    }
                }
#endif
                
                if(i == 0)
                {
                    PRINT_ERROR(SHELL_NAME, "pattern has unbalanced quotes");
                    free_str(&pat_string);
                    return NULL;
                }
                
                c = *p;     /* remember the quote char */
                p2 = p+i;
                
                while(++p < p2)
                {
                    switch(*p)
                    {
                        case '*':
                        case '?':
                        case '(':
                        case '|':
                        case '&':
                        case ')':
                        case '[':
                        case ']':
                            APPEND2('\\', *p);
                            break;
                        
                        case '\\':
                            /* quote \ if it is inside single quotes */
                            if(c == '\'')
                            {
                                APPEND2('\\', *p);
                                break;
                            }
                            
                            /* otherwise fall through */
                            __attribute__((fallthrough));
                            
                        default:
                            APPEND(*p);
                            break;
                    }
                }

                break;

#if 0
            case '\\':
                APPEND('\\');

                /* don't leave a hanging \ at the end of the pattern */
                if(p[1] == '\0')
                {
                    APPEND('\\');
                }
                break;
#endif
                
            case '[':
                /*
                 * bash and ksh treat unbalanced '[' as a regular char, we need
                 * to escape the unbalanced '['. We need to beware of the case
                 * where a '[' is the first char in the backet list, i.e. [[].
                 */
                p2 = p;
                if(p[1] == '[')
                {
                    p2++;
                }
                
                i = find_closing_brace(p2, 0);
                
                if(i == 0)
                {
                    APPEND2('\\', *p);
                }
                else
                {
                    APPEND(*p);
                    
                    /*
                     * Avoid calling find_closing_brace() again by manually 
                     * adding the second '['.
                     */
                    if(p2 != p)
                    {
                        APPEND(*p);
                        p++;
                    }
                }
                
                break;
                
            case '?':
                APPEND('.');
                break;
                            
            case '*':
                /* add . if asterisk was not preceded by ], ), or * */
                if((p == pat) || 
                   (p[-1] != ']' && p[-1] != ')' && p[-1] != '*'))
                {
                    APPEND2('.', *p);
                }
                else
                {
                    APPEND(*p);
                }
                
                /* collapse any series of asterisks into one '*' */
                while(p[1] == '*')
                {
                    p++;
                }
                
                break;
                
            default:
                APPEND(*p);
                break;
        }
        
        p++;
    }
    
    if(len)
    {
        APPEND('\0');
    }

    return pat_string.buf_base;
    
memerr:
    INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "pattern matching");
    free_str(&pat_string);
    return NULL;
}


/*
 * find the shortest or longest prefix of str that matches pattern, depending 
 * on the value of longest.
 * 
 * return value is the index of 1 after the last character in the prefix, 
 * i.e. where you should put a '\0' to get the prefix.
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
    
    /* Check the result of the comparison */
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
 * Find the shortest or longest suffix of str that matches pattern, depending 
 * on the value of longest.
 * 
 * Return value is the index of the first character in the matched suffix.
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
    
    /* Check the result of the comparison */
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
 * Check if the string str matches the given pattern.
 * 'print_err' is a flag that tells us if we should output an error message 
 * in case no match was found.
 *
 * Returns 1 if we have a match, 0 otherwise.
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
    /* Set up the flags */
    int flags = FNM_NOESCAPE | FNM_PATHNAME | FNM_LEADING_DIR;
    
    if( optionx_set(OPTION_NOCASE_MATCH)) flags |= FNM_CASEFOLD;
    if( optionx_set(OPTION_EXT_GLOB    )) flags |= FNM_EXTMATCH;
    if(!optionx_set(OPTION_DOT_GLOB    )) flags |= FNM_PERIOD  ;
    
    /* Perform the match */
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
                PRINT_ERROR(SHELL_NAME, "failed to match filename(s)");
            }
            return 0;
    }
}


/*
 * Similar to match_filename(), but matches strings for variable expansion
 * and others.
 *
 * Returns 1 if we have a match, 0 otherwise.
 */
int match_pattern(char *pattern, char *str)
{
    if(!pattern || !str)
    {
        return 0;
    }

    /* Set up the flags */
    int flags = FNM_LEADING_DIR;
    if(optionx_set(OPTION_NOCASE_MATCH))
    {
        flags |= FNM_CASEFOLD;
    }

    if(optionx_set(OPTION_EXT_GLOB))
    {
        flags |= FNM_EXTMATCH;
    }

    /* Perform the match */
    int res;
    
    if(optionx_set(OPTION_GLOB_ASCII_RANGES))
    {
        setlocale(LC_ALL, "C");
    }
    
    
    char *p = shell_pattern_to_regex(pattern);
    if(!p)
    {
        return 0;
    }
    
    debug ("pattern='%s', p='%s', str='%s'\n", pattern, p, str);
    res = match_pattern_ext(p, str);
    free(p);
    
    if(optionx_set(OPTION_GLOB_ASCII_RANGES))
    {
        setlocale(LC_ALL, "");
    }
    
    debug ("res = %d\n", res);
    
    return res;
}


/*
 * Match a string to a pattern using POSIX extended regex syntax (used when
 * the =~ operator is passed to the test builtin).
 */
int match_pattern_ext(char *pattern, char *str)
{
    /* Set up the flags */
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
        fprintf(stderr, "%s: regex match failed: ", SOURCE_NAME);
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

    regmatch_t marr[1] = { { -1, -1 } }; /* init to shut gcc up */
    int match = 0;
    result = regexec(&regex, str, 1, marr, 0);
    if(!result)
    {
        /*
         * regexec() can return success, even if only a part of the string
         * matched the pattern. Ensure we match the whole string by checking
         * that the matched substring begins and ends with our original string.
         */
        match = (marr[0].rm_so == 0 && str[marr[0].rm_eo] == '\0');
    }
    else if(result != REG_NOMATCH)
    {
        /* function returned an error. get size of buffer required for error message. */
        size_t length = regerror(result, &regex, NULL, 0);
        char buffer[length];
        (void) regerror(result, &regex, buffer, length);
        fprintf(stderr, "%s: regex match failed: %s\r\n", SOURCE_NAME, buffer);
    }

    /* Free the memory allocated from regcomp(). */
    regfree(&regex);
    return match;
}


/* Dummy function to satisfy scandir() */
static int one(const struct dirent *unused __attribute__((unused)))
{
    return 1;
}

static struct stat   statbuf;
struct dirent **eps;

/*
 * Scan the provided path to look for input files.
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
            PRINT_ERROR(SHELL_NAME, "failed to open `%s`: %s", path, strerror(errno));
        }
        return 0;
    }
}


char *__next_path = NULL;

/*
 * Gets the next filename from the directory listing.
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
            /* Error or empty directory */
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
     * Something funny happens after stating '.' & '..', the first file after
     * those is not seen as a file but as a dir. So wipe out the struct to
     * prevent this from happening.
     */
    memset(&statbuf, 0, sizeof(struct stat));
    
    char *name = eps[index]->d_name;
    lstat(name, &statbuf);
    
    /* Skip dot and dot-dot */
    if(S_ISDIR(statbuf.st_mode) &&
       (strcmp(name, "..") == 0 || strcmp(name, ".") == 0))
    {
        index++;
        goto loop;
    }
    
    return eps[index++]->d_name;
}


/*
 * Perform pathname (or filename) expansion, matching files in the given *dir to the
 * given *path, which is treated as a regex pattern that specifies which filename(s)
 * we should match.
 *
 * Returns a char ** pointer to the list of matched filenames, or NULL if nothing matched.
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

    /* Set up the flags */
    int flags = 0;
    if(optionx_set(OPTION_DOT_GLOB  )) flags |= GLOB_PERIOD;
    if(option_set('B')) flags |= GLOB_BRACE;

    /* Perform the match */
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
 * Test filename against a colon-separated pattern field to determine if it 
 * matches one of the patterns in the field. Used when performing filename 
 * expansion to determine which file names to ignore. The pattern is usually 
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
