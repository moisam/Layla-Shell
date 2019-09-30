/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: wordexp.c
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

/* required macro definition for popen() and pclose() */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include "cmd.h"
#include "builtins/setx.h"
#include "symtab/symtab.h"
#include "backend/backend.h"
#include "error/error.h"
#include "debug.h"


/* special value to represent an invalid variable */
#define INVALID_VAR     ((char *)-1)


/*
 * convert the string *word to a cmd_token struct, so it can be passed to
 * functions such as word_expand().
 *
 * returns the malloc'd cmd_token struct, or NULL if insufficient memory.
 */
struct word_s *make_word(char *str)
{
    /* alloc struct memory */
    struct word_s *word = (struct word_s *)malloc(sizeof(struct word_s));
    if(!word)
    {
        return NULL;
    }
    /* alloc string memory */
    size_t  len  = strlen(str);
    char   *data = (char *)malloc(len+1);
    if(!data)
    {
        free(word);
        return NULL;
    }
    /* copy string */
    strcpy(data, str);
    word->data = data;
    word->len  = len;
    word->next = NULL;
    /* return struct */
    return word;
}


/*
 * free the memory used by a list of tokens.
 */
void free_all_words(struct word_s *first)
{
    while(first)
    {
        struct word_s *del = first;
        first = first->next;
        if(del->data)
        {
            /* free the token text */
            free(del->data);
        }
        /* free the token */
        free(del);
    }
}


/*
 * delete the character at the given index in the given str.
 */
void delete_char_at(char *str, size_t index)
{
    char *p1 = str+index;
    char *p2 = p1+1;
    while((*p1++ = *p2++))
    {
        ;
    }
}


/*
 * check if a char is an alphanumeric character.
 */
static inline int is_alphanum(char c)
{
  if(c >= 'A' && c <= 'Z')
  {
      return 1;
  }
  if(c >= 'a' && c <= 'z')
  {
      return 1;
  }
  if(c >= '0' && c <= '9')
  {
      return 1;
  }
  if(c == '_')
  {
      return 1;
  }
  return 0;
}


/*
 * extract a number from buf, starting from the character at index start
 * and ending at index end.
 */
long extract_num(char *buf, int start, int end)
{
    long n = 0;
    int  len = end-start;
    char buf2[len+1];
    char *strend;
    strncpy(buf2, buf+start, len);
    buf2[len] = '\0';
    n = strtol(buf2, &strend, 10);
    return (strend == buf2) ? 0 : n;
}


/*
 * check if the given str is a valid name.. POSIX says a names can consist of
 * alphanumeric chars and underscores, and start with an alphabetic char or underscore.
 *
 * returns 1 if str is a valid name, 0 otherwise.
 */
int is_name(char *str)
{
    /* names start with alpha char or an underscore... */
    if(!isalpha(*str) && *str != '_')
    {
        return 0;
    }
    /* ...and contain alphanumeric chars and/or underscores */
    while(*++str)
    {
        if(!isalnum(*str) && *str != '_')
        {
            return 0;
        }
    }
    return 1;
}


/*
 * find the closing quote that matches the opening quote, which is the first
 * char of the data string.
 * sq_nesting is a flag telling us if we should allow single quote nesting
 * (prohibited by POSIX, but allowed in ANSI-C strings).
 *
 * returns the zero-based index of the closing quote.. a return value of 0
 * means we didn't find the closing quote.
 */
size_t find_closing_quote(char *data, int sq_nesting)
{
    /* check the type of quote we have */
    char quote = data[0];
    if(quote != '\'' && quote != '"' && quote != '`')
    {
        return 0;
    }
    /* find the matching closing quote */
    size_t i = 0, len = strlen(data);
    while(++i < len)
    {
        if(data[i] == quote)
        {
            if(data[i-1] == '\\')
            {
                if(quote != '\'' || sq_nesting)
                {
                    continue;
                }
            }
            return i;
        }
    }
    return i;
}


/*
 * find the closing brace that matches the opening brace, which is the first
 * char of the data string.
 *
 * returns the zero-based index of the closing brace.. a return value of 0
 * means we didn't find the closing brace.
 */
size_t find_closing_brace(char *data)
{
    /* check the type of opening brace we have */
    char opening_brace = data[0], closing_brace;
    if(opening_brace != '{' && opening_brace != '(' && opening_brace != '[')
    {
        return 0;
    }
    /* determine the closing brace according to the opening brace */
    if(opening_brace == '{')
    {
        closing_brace = '}';
    }
    else if(opening_brace == '[')
    {
        closing_brace = ']';
    }
    else
    {
        closing_brace = ')';
    }
    /* find the matching closing brace */
    size_t ob_count = 1, cb_count = 0;
    size_t i = 0, len = strlen(data);
    while(++i < len)
    {
        if((data[i] == '"') || (data[i] == '\'') || (data[i] == '`'))
        {
            /* skip escaped quotes */
            if(data[i-1] == '\\')
            {
                continue;
            }
            /* skip quoted substrings */
            char quote = data[i];
            while(++i < len)
            {
                if(data[i] == quote && data[i-1] != '\\')
                {
                    break;
                }
            }
            if(i == len)
            {
                return 0;
            }
            continue;
        }
        /* keep the count of opening and closing braces */
        if(data[i-1] != '\\')
        {
            if(data[i] == opening_brace)
            {
                ob_count++;
            }
            else if(data[i] == closing_brace)
            {
                cb_count++;
            }
        }
        /* break when we have a matching number of opening and closing braces */
        if(ob_count == cb_count)
        {
            break;
        }
    }
    if(ob_count != cb_count)
    {
        return 0;
    }
    return i;
}


/*
 * substitute the substring of s1, from character start to character end,
 * with the s2 string.
 *
 * start should point to the first char to be deleted from s1.
 * end should point to the last char to be deleted from s, NOT the
 * char coming after it.
 *
 * returns the malloc'd new string, or NULL on error.
 */
char *substitute_str(char *s1, char *s2, size_t start, size_t end)
{
    /* get the prefix (the part before start) */
    char before[start+1];
    strncpy(before, s1, start);
    before[start]   = '\0';
    /* get the postfix (the part after end) */
    size_t afterlen = strlen(s1)-end+1;
    char after[afterlen];
    strcpy(after, s1+end+1);
    /* alloc memory for the new string */
    size_t totallen = start+afterlen+strlen(s2);
    char *final = (char *)malloc(totallen+1);
    if(!final)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "performing variable substitution", NULL);
        /* POSIX says non-interactive shell should exit on expansion errors */
        if(!option_set('i'))
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }
        return NULL;
    }
    if(!totallen)       /* empty string */
    {
        final[0] = '\0';
    }
    else                /* concatenate the three parts into one string */
    {
        strcpy(final, before);
        strcat(final, s2    );
        strcat(final, after );
    }
    /* return the new string */
    return final;
}


int substitute_word(char **pstart, char **p, size_t len, char *(func)(char *), int add_quotes)
{
    /* extract the word to be substituted */
    char *tmp = malloc(len+1);
    if(!tmp)
    {
        (*p) += len;
        return 0;
    }
    strncpy(tmp, *p, len);
    tmp[len--] = '\0';
    /* and expand it */
    char *tmp2;
    if(func)
    {
        tmp2 = func(tmp);
        free(tmp);
        /* if func is var_expand, we might have an INVALID_VAR result */
        if(tmp2 == INVALID_VAR)
        {
            (*p) += len;
            return 0;
        }
    }
    else
    {
        tmp2 = tmp;
    }
    /* error expanding the string. keep the original string as-is */
    if(!tmp2)
    {
        (*p) += len;
        return 0;
    }
    /* save our current position in the word */
    size_t i = (*p)-(*pstart);
    /* substitute the command output */
    tmp = quote_val(tmp2, add_quotes);
    free(tmp2);
    if(tmp)
    {
        /* substitute the expanded word */
        if((tmp2 = substitute_str(*pstart, tmp, i, i+len)))
        {
            /* adjust our pointer to point to the new string */
            free(*pstart);
            (*pstart) = tmp2;
            len = strlen(tmp);
        }
        free(tmp);
    }
    /* adjust our pointer to point to the new string */
    (*p) = (*pstart)+i+len-1;
    return 1;
}


/* 
 * restricted shells can't set/unset the values of SHELL, ENV, FPATH, 
 * or PATH. this function checks if the given name is one of these.
 * bash also restricts BASH_ENV.
 * zsh also restricts EGID, EUID, GID, HISTFILE, HISTSIZE, IFS, 
 * UID and USERNAME (among others we're not using in this shell).
 */
int is_restrict_var(char *name)
{
    if(strcmp(name, "SHELL"   ) == 0 || strcmp(name, "ENV"     ) == 0 ||
       strcmp(name, "FPATH"   ) == 0 || strcmp(name, "PATH"    ) == 0 ||
       strcmp(name, "EUID"    ) == 0 || strcmp(name, "UID"     ) == 0 ||
       strcmp(name, "EGID"    ) == 0 || strcmp(name, "GID"     ) == 0 ||
       strcmp(name, "HISTFILE") == 0 || strcmp(name, "HISTSIZE") == 0 ||
       strcmp(name, "IFS"     ) == 0 || strcmp(name, "USERNAME") == 0 ||
       strcmp(name, "USER"    ) == 0 || strcmp(name, "LOGNAME" ) == 0)
    {
        return 1;
    }
    return 0;
}


/*
 * find all shell variables that start with the given prefix.
 *
 * returns the list of matched variables separated by spaces, or NULL if
 * no matches found.
 */
char *get_all_vars(char *prefix)
{
    int    i, len = strlen(prefix);
    char  *buf = NULL;
    int    bufsz = 0;
    /* use the first character of $IFS if defined, otherwise use space */
    struct symtab_entry_s *IFS = get_symtab_entry("IFS");
    char   c = (IFS && IFS->val && IFS->val[0]) ? IFS->val[0] : ' ';
    char   sp[2] = { c, '\0' };
    struct symtab_stack_s *stack = get_symtab_stack();
    int    first = 1;
    /* search all the symbol tables in the symbol table stack */
    for(i = 0; i < stack->symtab_count; i++)
    {
        /* get the i-th symbol table */
        struct symtab_s *symtab = stack->symtab_list[i];

#ifdef USE_HASH_TABLES
    
        /* if we are implementing symbol tables as hash tables */
        if(symtab->used)
        {
            struct symtab_entry_s **h1 = symtab->items;
            struct symtab_entry_s **h2 = symtab->items + symtab->size;
            for( ; h1 < h2; h1++)
            {
                struct symtab_entry_s *entry = *h1;
                
#else

        /* if we are implementing symbol tables as linked lists */
        struct symtab_entry_s *entry  = symtab->first;
            
#endif

                /* check the next entry */
                while(entry)
                {
                    /* compare this entry to the prefix */
                    if(strncmp(entry->name, prefix, len) == 0)
                    {
                        /* add 2 for ' ' and '\0' */
                        int len2 = strlen(entry->name)+2;
                        /* first variable. alloc buffer */
                        if(!buf)
                        {
                            buf = malloc(len2);
                            if(!buf)
                            {
                                break;
                            }
                            buf[0] = '\0';
                        }
                        /* second and subsequent variables. extend buffer */
                        else
                        {
                            /* don't duplicate */
                            if(strstr(buf, entry->name))
                            {
                                entry = entry->next;
                                continue;
                            }
                            /* make some room */
                            char *buf2;
                            if(!(buf2 = realloc(buf, bufsz+len2)))
                            {
                                break;
                            }
                            buf = buf2;
                        }
                        bufsz += len2;
                        if(first)
                        {
                            first = 0;
                        }
                        else
                        {
                            strcat(buf, sp);
                        }
                        strcat(buf, entry->name);
                    }
                    entry = entry->next;
                }
                
#ifdef USE_HASH_TABLES

            }
        }
        
#endif

    }
    
    /*
     * get the special variables with the given prefix.
     */
    for(i = 0; i < special_var_count; i++)
    {
        char *v = special_vars[i].name;
        if(strncmp(v, prefix, len) == 0)
        {
            /* add 2 for ' ' and '\0' */
            int len2 = strlen(v)+2;
            /* first variable. alloc buffer */
            if(!buf)
            {
                buf = malloc(len2);
                if(!buf)
                {
                    break;
                }
                buf[0] = '\0';
            }
            /* second and subsequent variables. extend buffer */
            else
            {
                /* don't duplicate */
                if(strstr(buf, v))
                {
                    continue;
                }
                /* make some room */
                char *buf2;
                if(!(buf2 = realloc(buf, bufsz+len2)))
                {
                    break;
                }
                buf = buf2;
            }
            bufsz += len2;
            if(first)
            {
                first = 0;
            }
            else
            {
                strcat(buf, sp);
            }
            strcat(buf, v);
        }
    }
    /* return the list of matches */
    return buf;
}


/*
 * perform command substitutions.
 * the backquoted flag tells if we are called from a backquoted command substitution:
 *
 *    `command`
 *
 * or a regular one:
 *
 *    $(command)
 */
char *command_substitute(char *__cmd)
{
    char    b[1024];
    size_t  bufsz = 0;
    char   *buf   = NULL;
    char   *p     = NULL;
    int     i     = 0;
    int backquoted = (*__cmd == '`');

    /* 
     * fix cmd in the backquoted version. we will be modifying the command, hence
     * our use of __get_malloced_str() instead of get_malloced_str().
     *
     * we skip the first char (if using the old, backquoted version), or the first
     * two chars (if using the POSIX version).
     */
    char *cmd = __get_malloced_str(__cmd+(backquoted ? 1 : 2));
    char *cmd2 = cmd;
    if(!cmd)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "command substitution", NULL);
        return NULL;
    }
    size_t cmdlen = strlen(cmd);
    if(backquoted)
    {
        /* remove the last back quote */
        if(cmd[cmdlen-1] == '`')
        {
            cmd[cmdlen-1] = '\0';
        }
        /* fix the backslash-escaped chars */
        char *p1 = cmd;
        do
        {
            if(*p1 == '\\' &&
               (p1[1] == '$' || p1[1] == '`' || p1[1] == '\\'))
            {
                char *p2 = p1, *p3 = p1+1;
                while((*p2++ = *p3++))
                {
                    ;
                }
            }
        } while(*(++p1));
    }
    else
    {
        /* remove the last closing brace */
        if(cmd[cmdlen-1] == ')')
        {
            cmd[cmdlen-1] = '\0';
        }
    }

    FILE *fp = NULL;
    if(cmd2[0] == '<')
    {
        /*
         * handle the special case of $(<file), a shorthand for $(cat file).
         * this is a common non-POSIX extension seen in bash, ksh...
         */
        cmd = cmd2+1;
        while(isspace(*cmd))
        {
            cmd++;
        }
        if(!*cmd)
        {
            return NULL;
        }
        fp = fopen(cmd, "r");
    }
    else if(isdigit(cmd2[0]))
    {
        /*
         * handle the special case of $(n<#), which gives expands to the 
         * current byte offset for file descriptor n.
         * this is another non-POSIX extension from ksh.
         */
        cmd = cmd2;
        int n = 0;
        while(isdigit(*cmd))
        {
            n = (n*10) + ((*cmd)-'0');
            cmd++;
        }
        if(!*cmd)
        {
            free(cmd2);
            return NULL;
        }
        if(cmd[0] == '<' && cmd[1] == '#')
        {
            n = lseek(n, 0, SEEK_CUR);
            free(cmd2);
            b[0] = '\0';        /* just in case sprintf() fails for some reason */
            sprintf(b, "%d", n);
            buf = malloc(strlen(b)+1);
            if(!buf)
            {
                BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "command substitution", NULL);
                return NULL;
            }
            strcpy(buf, b);
            return buf;
        }
        /*
         * open a pipe for all other (normal) commands.
         */
        fp = popenr(cmd2);
        //fp = popen(cmd2, "r");
    }
    else
    {
        /*
         * open a pipe for all other (normal) commands.
         */
        fp = popenr(cmd2);
        //fp = popen(cmd2, "r");
    }
    /* check if we have opened the pipe */
    if(!fp)
    {
        free(cmd2);
        BACKEND_RAISE_ERROR(FAILED_TO_OPEN_PIPE, strerror(errno), NULL);
        return NULL;
    }
    /* read the command output */
    while((i = fread(b, 1, 1024, fp)))
    {
        /* first time. alloc buffer */
        if(!buf)
        {
            /* add 1 for the null terminating byte */
            buf = malloc(i+1);
            if(!buf)
            {
                goto fin;
            }
            p   = buf;
        }
        /* extend buffer */
        else
        {
            char *buf2 = realloc(buf, bufsz+i+1);
            //if(buf != buf2) free(buf);
            if(!buf2)
            {
                free(buf);
                buf = NULL;
                goto fin;
            }
            buf = buf2;
            p   = buf+bufsz;
        }
        bufsz += i;
        /* copy the input and add the null terminating byte */
        memcpy(p, b, i);
        p[i] = '\0';
    }
    if(!bufsz)
    {
        return NULL;
    }
    /* now remove any trailing newlines */
    i = bufsz-1;
    while(buf[i] == '\n' || buf[i] == '\r')
    {
        buf[i] = '\0';
        i--;
    }
    
fin:
    if(cmd2[0] == '<')
    {
        fclose(fp);
    }
    else
    {
        pclose(fp);
    }
    free(cmd2);
    /* free used memory */
    if(!buf)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "command substitution", NULL);
        free(cmd2);
        return NULL;
    }
    return buf;
}


/*
 * bash extension for variable expansion. it takes the form of:
 *       ${parameter@operator}
 * where operator is a single letter of the following:
 *       Q - value of parameter properly quoted.
 *       E - value of parameter with backslash escape sequences expanded,
 *           similar to ANSI-C strings.
 *       P - expand parameter as if it were a prompt string.
 *       A - expand parameter as an assignment statement.
 *       a - flag values representing parameter’s attributes (not used here).
 * 
 * arguments:
 *       op       - operator, one of the above.
 *       orig_val - variable's value, to be expanded and quoted.
 *       var_name - variable's name.
 *       name_len - var_name's length.
 * 
 * returns the malloc'd result of the expansion, or NULL if the expansion fails.
 */
char *var_info_expand(char op, char *orig_val, char *var_name, int name_len)
{
    char *sub, *tmp;
    switch(op)
    {
        case 'Q':
            return quote_val(orig_val, 1);
                            
        case 'E':
            return ansic_expand(orig_val);
                            
        case 'P':
            return __evaluate_prompt(orig_val);

        case 'A':
            tmp = quote_val(orig_val, 1);
            /* calc needed space */
            name_len += 6;
            if(tmp)
            {
                name_len += strlen(tmp);
            }
            /* alloc space */
            sub = malloc(name_len+1);
            if(!sub)
            {
                return NULL;
            }
            sprintf(sub, "let %s=%s", var_name, tmp);
            if(tmp)
            {
                free(tmp);
            }
            return sub;
    }
    /* unknown operator */
    return NULL;
}


/*
 * perform variable (parameter) expansion.
 * our options are:
 * syntax           POSIX description   var defined     var undefined
 * ======           =================   ===========     =============
 * $var             Substitute          var             nothing
 * ${var}           Substitute          var             nothing
 * ${var:-thing}    Use Deflt Values    var             thing (var unchanged)
 * ${var:=thing}    Assgn Deflt Values  var             thing (var set to thing)
 * ${var:?message}  Error if NULL/Unset var             print message and exit shell,
 *                                                      (if message is empty, print 
 *                                                      "var: parameter not set")
 * ${var:+thing}    Use Alt. Value      thing           nothing
 * ${#var}          Calculate String Length
 * 
 * Using the same options in the table above, but without the colon, results in
 * a test for a parameter that is unset. using the colon results in a test for a
 * parameter that is unset or null.
 */

/*
 * TODO: we should test our implementation of the following string processing 
 *       functions (see section 2.6.2 - Parameter Expansion in POSIX):
 * 
 *       ${parameter%[word]}      Remove Smallest Suffix Pattern
 *       ${parameter%%[word]}     Remove Largest Suffix Pattern
 *       ${parameter#[word]}      Remove Smallest Prefix Pattern
 *       ${parameter##[word]}     Remove Largest Prefix Pattern
 * 
 * TODO: implement the following string match/replace functions, which are
 *       non-POSIX extensions (bash, ksh, ...):
 * 
 *       ${parameter/pattern/string}
 *       ${parameter//pattern/string}
 *       ${parameter/#pattern/string}
 *       ${parameter/%pattern/string}
 */


/*
 * return the malloc'd string of the variable value val, or the length of that
 * value if get_length is non-zero.. if both pos_params and get_length are
 * non-zero, return the count of positional parameters.
 *
 * this function should not be called directly by any function outside of this
 * module (hence the double underscores that prefix the function name).
 */
char *__get_var_val(char *val, int get_length, int pos_params)
{
    char buf[32];
    if(get_length)
    {
        if(pos_params)
        {
            sprintf(buf, "%d", pos_param_count());
        }
        else
        {
            if(!val)
            {
                return __get_malloced_str("0");
            }
            sprintf(buf, "%lu", strlen(val));
        }
        return __get_malloced_str(buf);
    }
    else
    {
        /* "normal" variable value */
        char *mval = __get_malloced_str(val);
        /* POSIX says non-interactive shell should exit on expansion errors */
        if(!mval && !option_set('i'))
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }
        return mval ? : INVALID_VAR;
    }
}


/*
 * read input from stdin and substitute it for a variable (tcsh extension), or
 * return the length of that string if get_length is non-zero.
 * returns the malloc'd input string (or its length), or NULL in case of error.
 *
 * this function should not be called directly by any function outside of this
 * module (hence the double underscores that prefix the function name).
 */
char *__get_stdin_var(int get_length)
{
    char *tmp = NULL;
    /* stdin must be a terminal device */
    if(!isatty(0))
    {
        return NULL;
    }
    /* alloc memory for input */
    int line_max = get_linemax();
    char *in = malloc(line_max);
    if(!in)     /* insufficient memory */
    {
        return NULL;
    }
    /* turn on canonical mode so we can read from stdin */
    if(src->srctype == SOURCE_STDIN)
    {
        term_canon(1);
    }
    if(fgets(in, line_max, stdin))
    {
        /* get the length of input */
        if(get_length)
        {
            int len = strlen(in);
            char buf[len+1];
            sprintf(buf, "%d", len);
            tmp = __get_malloced_str(buf);
        }
        /* get the input string */
        else
        {
            tmp = __get_malloced_str(in);
        }
        /* free the buffer */
        free(in);
        /* return to non-canonical mode */
        if(src->srctype == SOURCE_STDIN)
        {
            term_canon(0);
        }
        return tmp;
    }
    /* return to non-canonical mode */
    if(src->srctype == SOURCE_STDIN)
    {
        term_canon(0);
    }
    /* failed to get input. return NULL */
    return NULL;
}


/*
 * perform variable (parameter) expansion.
 *
 * returns an malloc'd string of the expanded variable value, or NULL if the
 * variable is not defined or the expansion failed.
 *
 * this function should not be called directly by any function outside of this
 * module (hence the double underscores that prefix the function name).
 */
char *var_expand(char *__var_name)
{
    /* sanity check */
    if(!__var_name)
    {
       return NULL;
    }
    /*
     *  if the var substitution is in the $var format, remove the $.
     *  if it's in the ${var} format, remove the ${}.
     */
    /* skip the $ */
    __var_name++;
    size_t len = strlen(__var_name);
    if(*__var_name == '{')
    {
        /* remove the } */
        __var_name[len-1] = '\0';
        __var_name++;
    }
    /* check we don't have an empty varname */
    if(!*__var_name)
    {
       return NULL;
    }
    int get_length = 0;
    /* if varname starts with #, we need to get the string length */
    if(*__var_name == '#')
    {
        /* use of '#' should come with omission of ':' */
        if(strchr(__var_name, ':'))
        {
            BACKEND_RAISE_ERROR(INVALID_SUBSTITUTION, __var_name, NULL);
            /* POSIX says non-interactive shell should exit on expansion errors */
            EXIT_IF_NONINTERACTIVE();
            return INVALID_VAR;
        }
        /* 
         * make sure the caller meant ${#parameter}, which gets string length,
         * and it didn't mean ${#}, which gets # of positional parameters.
         */
        char c = __var_name[1];
        if(c != '\0' && c != '-' && c != '=' && c != '?' && c != '+')
        {
            get_length = 1;
            __var_name++;
        }
    }
    /* check we don't have an empty varname */
    if(!*__var_name)
    {
        return NULL;
    }
    /*
     * sanity-check the name. it should only include alphanumeric chars, or one of
     * the special parameter names, such as !, ?, # and so on.
     */
    char *p = __var_name;
    if(!is_alphanum(*p) && *p != '!' && *p != '?' && *p != '#' &&
                           *p != '$' && *p != '-' && *p != '@' &&
                           *p != '*' && *p != '<')
    {
        BACKEND_RAISE_ERROR(INVALID_SUBSTITUTION, __var_name, NULL);
        /* POSIX says non-interactive shell should exit on expansion errors */
        EXIT_IF_NONINTERACTIVE();
        return INVALID_VAR;
    }
    /*
     * search for a colon, which we use to separate the variable name from the
     * value or substitution we are going to perform on the variable.
     */
    //int   colon = 0;
    char *sub   = strchr(__var_name, ':');
    if(sub)     /* we have a colon + substitution */
    {
        /* colon = 1 */ ;
    }
    else        /* we have a substitution without a colon */
    {
        char *v = __var_name;
        /* don't mistake a '#' variable name for a #var length substitution */
        if(*v == '#' && v[1] == '\0')
        {
            /* skip the '#' so we won't entertain it again */
            v++;
        }
        /* make sure we don't mistake a special var for a substitution string */
        else if(*v == '-' || *v == '=' || *v == '?' || *v == '+' || *v == '@')
        {
            /* skip the '-', '=', '?', '+', or '@' so we won't entertain it again */
            v++;
        }
        /* search for the char that indicates what type of substitution we need to do */
        sub = strchr_any(v, "-=?+%#@");
    }
    /* get the length of the variable name (without the substitution part) */
    len = sub ? (size_t)(sub-__var_name) : strlen(__var_name);
    /* if we have a colon+substitution, skip the colon */
    if(sub && *sub == ':')
    {
        sub++;
    }
    /* copy the varname to a buffer */
    char var_name[len+1];
    strncpy(var_name, __var_name, len);
    var_name[len]   = '\0';
    /*
     * common extension (bash, ksh, ...) to return variables whose name
     * starts with a prefix. the format is ${!prefix*} or ${!prefix@}. the
     * latter expands each varname to a separate field if called within double
     * quotes (bash), but we treat both the same here for the sake of simplicity.
     */
    if(var_name[0] == '!' && (var_name[len-1] == '*' || var_name[len-1] == '@'))
    {
        /* remove the trailing '*' or '@' */
        var_name[len-1] = '\0';
        /* get all shell variables whose name starts with varname */
        return get_all_vars(var_name+1);
    }

    /*
     * commence variable substitution.
     */
    char *empty_val  = "";
    char *tmp        = (char *)NULL;
    char  setme      = 0;
    char *orig_val   = NULL;
    int   pos_params = 0;
    /* check if the requested varname is a special variable name and get its value */
    tmp = get_special_var(var_name);
    if(!tmp)
    {
        /* varname is not a special variable name. check if its a normal variable */
        tmp = get_shell_varp(var_name, empty_val);
    }
    /* save a pointer to the variable's value (we'll use it below) */
    orig_val = tmp;
    /* handle the $@ and $* special parameters as a special case */
    if(strcmp(var_name, "@") == 0 || strcmp(var_name, "*") == 0)
    {
        /* we can only return the array length of $@ or $* here */
        if(get_length)
        {
            return __get_var_val(var_name, get_length, 1);
        }
        /* handle $* and $@ as a special case */
        return pos_params_expand(var_name, 0);
    }
    /* tcsh extension to read directly from stdin */
    if(strcmp(var_name, "<") == 0)
    {
        return __get_stdin_var(get_length);
    }
    

    /*
     * first case: variable is unset or empty.
     */
    if(!tmp || tmp == empty_val)
    {
        /* no unset parameters are accepted */
        if(option_set('u') && !pos_params)
        {
            BACKEND_RAISE_ERROR(UNSET_VARIABLE, var_name, "parameter not set");
            if(!option_set('i'))
            {
                exit_gracefully(EXIT_FAILURE, NULL);
            }
            return INVALID_VAR;
        }
        /* do we have a substitution clause? */
        if(sub && *sub)
        {
            /* no colon, no substitution and variable unset/empty */
            /*
            if(!colon && tmp == empty_val)
            {
                return NULL;
            }
            */
            /* check the substitution operation we need to perform */
            switch(sub[0])
            {
                case '-':
                    tmp = sub+1;  /* use variable's default value */
                    break;
                    
                case '=':          /* assign the variable a value */
                    /* 
                     * only variables, not positional or special parameters can be
                     * assigned this way.
                     */
                    if(is_pos_param(var_name) || is_special_param(var_name))
                    {
                        BACKEND_RAISE_ERROR(INVALID_ASSIGNMENT, __var_name, NULL);
                        /* NOTE: this is not strict POSIX behaviour. see the table above */
                        if(!option_set('i'))
                        {
                            if(option_set('e'))
                            {
                                /* exit if non-interactive */
                                exit_gracefully(EXIT_FAILURE, NULL);
                            }
                            else
                            {
                                trap_handler(ERR_TRAP_NUM);
                            }
                        }
                        return INVALID_VAR;
                    }
                    tmp = sub+1;
                    /* 
                     * assign the EXPANSION OF tmp, not tmp
                     * itself, to var_name (we'll set the value below).
                     */
                    setme = 1;
                    break;
                
                case '?':          /* print error msg if variable is null/unset */
                    /*
                     * TODO: we should use the EXPANSION OF the string starting at (sub+1),
                     *       not the original word itself as passed to us.
                     */
                    if(sub[1] == '\0')
                    {
                        BACKEND_RAISE_ERROR(UNSET_VARIABLE, var_name, "parameter not set");
                    }
                    else
                    {
                        BACKEND_RAISE_ERROR(UNSET_VARIABLE, var_name, sub+1);
                    }
                    /* exit if non-interactive */
                    if(!option_set('i'))
                    {
                        exit_gracefully(EXIT_FAILURE, NULL);
                    }
                    return INVALID_VAR;
                
                /* use alternative value (we don't have alt. value here) */
                case '+':
                    return NULL;

                /*
                 * pattern matching notation. can't match anything
                 * if the variable is not defined, now can we?
                 */
                case '#':
                case '/':
                case '%':
                case '@':
                    return __get_var_val(tmp, get_length, pos_params);
                    
                default:                /* unknown operator */
                    return NULL;
            }
        }
        /* no substitution clause. return NULL as the variable is unset/null */
        else
        {
            //return NULL;
            return __get_malloced_str("");
        }
    }
    /*
     * second case: variable is set/not empty.
     */
    else
    {
        /* do we have a substitution clause? */
        if(sub && *sub)
        {
            /* check the substitution operation we need to perform */
            switch(sub[0])
            {
                /*
                 * TODO: implement the match/replace functions of ${parameter/pattern/string} 
                 *       et al. see bash manual page 35.
                 */
                case '/':
                    return __get_var_val(tmp, get_length, pos_params);
                    
                /* use alternative value */
                case '+':
                    tmp = sub+1;
                    break;

                /*
                 * bash extension for variable expansion. it takes the form of:
                 *       ${parameter@operator}
                 * where operator is a single letter Q, E, P, A or a.
                 */
                case '@':
                    sub = var_info_expand(sub[1], orig_val, var_name, len);
                    if(!sub)
                    {
                        return __get_var_val(tmp, get_length, pos_params);
                    }
                    return sub;
 
                /*
                 * for the prefix and suffix matching routines (below),
                 * when the parameter is @ or * the result is processed by
                 * __word_expand() without calling us.
                 * bash expands the pattern part, but ksh doesn't seem to do
                 * the same (as far as the manpage is concerned). we follow ksh.
                 */
                case '%':       /* match suffix */
                    sub++;
                    /* perform word expansion on the value */
                    p = word_expand_to_str(tmp);
                    /* word expansion failed */
                    if(!p)
                    {
                        EXIT_IF_NONINTERACTIVE();
                        return INVALID_VAR;
                    }
                    int longest = 0;
                    /* match the longest or shortest suffix */
                    if(*sub == '%')
                    {
                        longest = 1, sub++;
                    }
                    /* perform the match */
                    if((len = match_suffix(sub, p, longest)) == 0)
                    {
                        return p;
                    }
                    /* return the match */
                    char *p2 = get_malloced_strl(p, 0, len);
                    free(p);
                    return p2;

                case '#':       /* match prefix */
                    sub++;
                    /* perform word expansion on the value */
                    p = word_expand_to_str(tmp);
                    /* word expansion failed */
                    if(!p)
                    {
                        EXIT_IF_NONINTERACTIVE();
                        return INVALID_VAR;
                    }
                    longest = 0;
                    /* match the longest or shortest suffix */
                    if(*sub == '#')
                    {
                        longest = 1, sub++;
                    }
                    /* perform the match */
                    if((len = match_prefix(sub, p, longest)) == 0)
                    {
                        return p;
                    }
                    /* return the match */
                    p2 = get_malloced_strl(p, len, strlen(p)-len);
                    free(p);
                    return p2;

                case '-':          /* use variable's default value */
                case '=':          /* assign the variable a value */
                case '?':          /* print error msg if variable is null/unset */
                    return __get_var_val(tmp, get_length, pos_params);
                    
                default:
                    /* 
                     * ${parameter:offset}
                     * ${parameter:offset:length}
                     */
                    while(isspace(*sub))        /* skip optional spaces */
                    {
                        sub++;
                    }
                    /* search for the second colon (optional) */
                    tmp = strchr(sub, ':');
                    long off, len;
                    long sublen = strlen(sub);
                    if(tmp)     /* we have a colon */
                    {
                        off = extract_num(sub, 0, tmp-sub);
                        len = extract_num(sub, (tmp-sub)+1, sublen);
                    }
                    else        /* no colon */
                    {
                        off = extract_num(sub, 0, sublen);
                        len = strlen(orig_val)-off;
                    }
                    /* negative offsets count from the end of string */
                    long vallen = strlen(orig_val);
                    if(off < 0)
                    {
                        off += vallen;
                    }
                    if(len < 0)
                    {
                        /* both off and len are offsets now */
                        len += vallen;
                        if(len < off)
                        {
                            long j = len;
                            len = off;
                            off = j;
                        }
                        len -= off;
                    }
                    char *var_val = get_malloced_strl(orig_val, off, len);
                    /* POSIX says non-interactive shell should exit on expansion errors */
                    if(!var_val && !option_set('i'))
                    {
                        exit_gracefully(EXIT_FAILURE, NULL);
                    }
                    return var_val ? : INVALID_VAR;
                    //goto normal_value;
            }
        }
        /* no substitution clause. return the variable's original value */
        else
        {
            return __get_var_val(tmp, get_length, pos_params);
        }
    }
    /*
     * we have substituted the variable's value. now go POSIX style on it.
     */
    struct word_s *w = word_expand(tmp);
    if(!w)      /* word substitution failed */
    {
        return NULL;
    }
    /* do we need to set new value to the variable? */
    if(setme)
    {
        tmp = __tok_to_str(w);
        __set(var_name, tmp, 0, 0, 0);
        free(tmp);
    }
    char buf[32];
    if(get_length)
    {
        /* return the length of the variable's value */
        if(pos_params)
        {
            /* get positional parameters count */
            sprintf(buf, "%d", pos_param_count());
        }
        else
        {
            /* get value length */
            tmp = __tok_to_str(w);
            free_all_words(w);
            if(!tmp)
            {
                return __get_malloced_str("0");
            }
            sprintf(buf, "%lu", strlen(tmp));
            free(tmp);
        }
        return __get_malloced_str(buf);
    }
    else
    {
        /* return the list of expanded tokens */
        tmp = __tok_to_str(w);
        free_all_words(w);
        return tmp;
    }
}


/*
 * perform variable (parameter) expansion for the positional parameters.
 *
 * returns an malloc'd string of the positional parameters, or NULL if no
 * positional parameters are defined or the expansion failed.
 *
 * this function should not be called directly by any function outside of this
 * module (hence the double underscores that prefix the function name).
 */
char *pos_params_expand(char *tmp, int in_double_quotes)
{
    errno = 0;
    /* search for a colon, which introduces a substitution */
    char *sub = strchr(tmp, ':');
    char *p;
    /* do we have a substitution? */
    if(sub)
    {
        /* 
         * ${parameter:offset}
         * ${parameter:offset:length}
         */
        sub++;
        while(isspace(*sub))    /* skip optional spaces */
        {
            sub++;
        }
        /* get the second (optional) colon */
        char *tmp2 = strchr(sub, ':');
        long off, len;
        long sublen = strlen(sub);
        long count = pos_param_count()+1;
        if(count <= 0)          /* no positional parameters */
        {
            //return NULL;
            return __get_malloced_str("");
        }
        /* we have two colons in the substitution (so we have offset and length) */
        if(tmp2)
        {
            off = extract_num(sub, 0, tmp2-sub);
            len = extract_num(sub, (tmp2-sub)+1, sublen);
        }
        /* we have on colon in the substitution (so we have offset only) */
        else
        {
            off = extract_num(sub, 0, sublen);
            len = count-off;
        }
        /* negative offsets count from the end of the array */
        if(off < 0)
        {
            off += count;
        }
        if(len < 0)
        {
            /* both are offsets now */
            len += count;
            if(len < off)
            {
                long j = len;
                len = off;
                off = j;
            }
            len -= off;
        }
        p = get_pos_params_str(*tmp, in_double_quotes, off, len);
    }
    else if((sub = strchr(tmp+1, '@')))
    {
        /*
         * bash extension for variable expansion. it takes the form of:
         *       ${parameter@operator}
         * where operator is a single letter Q, E, P, A or a.
         */
        int  count = pos_param_count();
        char *subs[count+1];
        char op = *++sub;
        int  k, l;
        for(k = 1, l = 0; k <= count; k++)
        {
            struct symtab_entry_s *p = get_pos_param(k);
            if(!p || !p->val)
            {
                continue;
            }
            sub = var_info_expand(op, p->val, p->name, strlen(p->name));
            if(sub)
            {
                subs[l++] = sub;
            }
        }
        subs[l++] = NULL;
        /* convert the list to a string */
        p = list_to_str(subs, 0);
        /* free the memory used by the list strings */
        for(k = 0; subs[k]; k++)
        {
            free(subs[k]);
        }
    }
    else
    {
        /*
         * for the prefix and suffix matching routines, there is
         * a special case when the parameter is @ or *. we do this 
         * in order to follow what the major shells do, where the
         * matching operation is applied to each element in turn, and
         * the result is the list of all these elements after
         * processing.
         * bash expands the pattern part, but ksh doesn't seem to do
         * the same (as far as the manpage is concerned). we follow ksh.
         */
        sub = strchr(tmp, '#');
        if(!sub)
        {
            sub = strchr(tmp, '%');
        }
        if(!sub)
        {
            p = get_all_pos_params_str(*tmp, in_double_quotes);
        }
        else
        {
            int  count = pos_param_count();
            char *subs[count+1];
            char op = *sub++;
            int  longest = 0, k, l;
            int  (*func)(char *, char *, int) = (op == '#') ? match_prefix : match_suffix;
            int len;
            if(*sub == op)
            {
                longest = 1, sub++;
            }
            for(k = 1, l = 0; k <= count; k++)
            {
                struct symtab_entry_s *p = get_pos_param(k);
                if(!p || !p->val)
                {
                    continue;
                }
                if((len = func(sub, p->val, longest)) == 0)
                {
                    subs[l++] = __get_malloced_str(p->val);
                }
                else
                {
                    subs[l++] = get_malloced_strl(p->val, len, strlen(p->val)-len);
                }
            }
            subs[l++] = NULL;
            /* convert the list to a string */
            p = list_to_str(subs, 0);
            /* free the memory used by the list strings */
            for(k = 0; subs[k]; k++)
            {
                free(subs[k]);
            }
        }
    }
    /* return the result */
    return p ? : __get_malloced_str("");
}


/*
 * parse an ANSI-C string.. this is a string that appears in input in the form of
 * $'string'.. escape sequences such as \a, \b, \n, etc are allowed inside ANSI-C
 * strings, as well escaped single quotes.
 *
 * returns an malloc'd copy of the original string, with the escaped sequences
 * parsed, or NULL in case of insufficient memory.
 *
 * this function should not be called directly by any function outside of this
 * module (hence the double underscores that prefix the function name).
 */
char *ansic_expand(char *str)
{
    if(!str)
    {
        return NULL;
    }
    /* get a copy of the string to work on */
    str = __get_malloced_str(str);
    if(!str)
    {
        return NULL;
    }
    char *s = str;
    char  c;
    int   i, j, k;
    char  buf[8];
    while(*s)
    {
        /* parse escape sequences */
        if(*s =='\\')
        {
            /* remove the quote and substitute the special char */
            delete_char_at(s, 0);
            switch(*s)
            {
                case '0':
                    *s = '\0';
                    return str;
                    
                case 'a':
                    *s = '\a';
                    break;
                    
                case 'b':
                    *s = '\b';
                    break;
                    
                case 'e':
                case 'E':
                    *s = '\e';
                    break;
                    
                case 'f':
                    *s = '\f';
                    break;
                    
                case 'n':
                    *s = '\n';
                    break;
                    
                case 'r':
                    *s = '\r';
                    break;
                    
                case 't':
                    *s = '\t';
                    break;
                    
                case 'v':
                    *s = '\v';
                    break;
                    
                case '\\':
                case '\'':
                case  '"':
                case  '?':
                    break;
                    
                case 'x':
                    delete_char_at(s, 0);
                    /* we can have up to 2 hex digits */
                    i = 0;
                    if(isxdigit(*s  ))
                    {
                        c = get_xdigit(*s), i++;
                    }
                    if(isxdigit(s[1]))
                    {
                        c = c*16 + get_xdigit(s[1]), i++;
                    }
                    *s = c;
                    if(i != 1)
                    {
                        delete_char_at(s, 1);
                    }
                    break;

                /*
                 * we use UTF-8. see https://en.wikipedia.org/wiki/UTF-8
                 */
                case 'U':           /* unicode char \UHHHHHHHH */
                case 'u':           /* unicode char \uHHHH */
                    if(*s == 'u')
                    {
                        k = 4;
                    }
                    else
                    {
                        k = 8;
                    }
                    delete_char_at(s, 0);
                    j = 0;
                    for(i = 0; i < k; i++)
                    {
                        if(isxdigit(s[i]))
                        {
                            c = get_xdigit(s[i]);
                            j = j*16 + c;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if(i == 0)
                    {
                        break;
                    }
                    sprintf(buf, "%lc", j);
                    /* get the byte length of this UTF-8 codepoint */
                    if(j >= 0x0000 && j <= 0x007f)
                    {
                        j = 1;
                    }
                    else if(j >= 0x0080 && j <= 0x07ff)
                    {
                        j = 2;
                    }
                    else if(j >= 0x0800 && j <= 0xffff)
                    {
                        j = 3;
                    }
                    else
                    {
                        j = 4;
                    }
                    /*
                     * check if we have enough space.
                     * i is the byte length of the original codepoint.
                     * j is the byte length of the codepoint UTF-8 value.
                     */
                    char *p1, *p2;
                    if(i < j)
                    {
                        /* extend buf */
                        int l = strlen(s);
                        char *s2 = realloc(str, l+j-i+1);
                        if(!s2)
                        {
                            free(str);
                            return NULL;
                        }
                        s = s2+(s-str);
                        str = s2;
                        p1 = s+l;
                        p2 = p1+j-i;
                        while(p1 >= s)
                        {
                            *p2++ = *p1++;
                        }
                    }
                    else if(i > j)
                    {
                        /* shrink buf */
                        p1 = s+i;
                        p2 = s+j;
                        while((*p2++ = *p1++))
                        {
                            ;
                        }
                    }
                    /* now copy the UTF-8 codepoint */
                    p1 = buf;
                    while(*p1)
                    {
                        *s++ = *p1++;
                    }
                    s--;
                    break;
                    
                case 'c':           /* CTRL-char */
                    delete_char_at(s, 0);
                    c = *s;
                    if(c >= 'a' && c <= 'z')
                    {
                        c = c-'a'+1;
                    }
                    else if(c >= 'A' && c <= 'Z')
                    {
                        c = c-'A'+1;
                    }
                    else if(c >= '[' && c <= '_')
                    {
                        c = c-'['+0x1b;
                    }
                    else
                    {
                        break;
                    }
                    *s = c;
                    break;
                    
                default:
                    if(isdigit(*s))
                    {
                        c = (*s)-'0';
                        i = 1;
                        if(isdigit(s[1]))
                        {
                            c = c*8 + s[1]-'0', i++;
                        }
                        if(isdigit(s[2]))
                        {
                            c = c*8 + s[2]-'0', i++;
                        }
                        *s = c;
                        if(i != 1)
                        {
                            delete_char_at(s, 1), i--;
                        }
                        if(i != 1)
                        {
                            delete_char_at(s, 1);
                        }
                    }
                    break;
            }
        }
        s++;
    }
    return str;
}


/*
 * perform tilde expansion.
 *
 * returns the malloc'd expansion of the tilde prefix, NULL if expansion failed.
 */
char *tilde_expand(char *s)
{
    char *home = NULL;
    /*
     * TODO: we should add full support for the "Tilde Prefix" as defined in the
     *       POSIX standard (section 2.6.1).
     */
    size_t len      = strlen(s);
    char   *s2      = NULL;
    struct symtab_entry_s *entry;
    /* null tilde prefix. substitute with the value of home */
    if(len == 1)
    {
        home = get_home();
    }
    else
    {
        /* ksh and bash extensions */
        if(strcmp(s, "~+") == 0 || strcmp(s, "~-") == 0)
        {
            entry = get_symtab_entry((s[1] == '+') ? "PWD" : "OLDPWD");
            char *dir = entry ? entry->val : NULL;
            if(dir && dir[0] != '\0')
            {
                home = dir;
            }
        }
        /*
         * TODO: process numeric tilde prefixes such as ~+N, ~-N and ~N, which
         *       represent entries in the dirs stack (see bash manual pages 30 and 96).
         */
        else
        {
            /* we have a login name */
            struct passwd *pass;
            pass = getpwnam(s+1);
            if(pass)
            {
                home = pass->pw_dir;
            }
        }
    }
    /* return the home dir we've found */
    if(!home)
    {
        return NULL;
    }
    s2 = malloc(strlen(home)+1);
    if(!s2)
    {
        return NULL;
    }
    strcpy(s2, home);
    return s2;
}


/* 
 * perform word expansion on a single word.
 * head contains the word to be expanded.. in_heredoc tells us if we are performing the
 * word expansion inside a heredoc.. flags tell us if we should strip quotes and spaces
 * from the expanded word.
 *
 * returns the head of the linked list of the expanded fields and stores the last field
 * in the tail pointer.
 */
struct word_s *word_expand_one_word(char *orig_word)
{
    if(!orig_word)
    {
        return NULL;
    }
    char *pstart = malloc(strlen(orig_word)+1);
    if(!pstart)
    {
        return NULL;
    }
    strcpy(pstart, orig_word);
    char *p = pstart, *p2;
    char *tmp, *tmp2;
    char   c;
    size_t i = 0, k;
    size_t len;
    int in_double_quotes = 0;
    int in_var_assign = 0;
    int var_assign_eq = 0;
    int expanded = 0;
    char *(*func)(char *);

    do
    {
        switch(*p)
        {
            case '~':
                /* don't perform tilde expansion inside double quotes */
                if(in_double_quotes)
                {
                    break;
                }
                /* expand a tilde prefix only if:
                 * - it is the first unquoted char in the string.
                 * - it is part of a variable assignment, and is preceded by the first
                 *   equals sign or a colon.
                 */
                if(p == pstart || (in_var_assign && (p[-1] == ':' || (p[-1] == '=' && var_assign_eq == 1))))
                {
                    /* find the end of the tilde prefix */
                    int tilde_quoted = 0;
                    int endme = 0;
                    p2 = p+1;
                    while(*p2)
                    {
                        switch(*p2)
                        {
                            case '\\':
                                tilde_quoted = 1;
                                p2++;
                                break;
                                
                            case '"':
                            case '\'':
                                i = find_closing_quote(p2, 0);
                                if(i)
                                {
                                    tilde_quoted = 1;
                                    p2 += i;
                                }
                                break;
                                
                            case '/':
                                endme = 1;
                                break;
                                
                            case ':':
                                if(in_var_assign)
                                {
                                    endme = 1;
                                }
                                break;
                        }
                        if(endme)
                        {
                            break;
                        }
                        p2++;
                    }
                    /* if any part of the prefix is quoted, no expansion is done */
                    if(tilde_quoted)
                    {
                        /* just skip the tilde prefix */
                        p = p2;
                        break;
                    }
                    /* otherwise, extract the prefix */
                    len = p2-p;
                    substitute_word(&pstart, &p, len, tilde_expand, !in_double_quotes);
                    expanded = 1;
                }
                break;
                
            case '"':
                /* handle "$@" and "$*" specifically */
                if(strcmp(p, "\"$@\"") == 0 || strcmp(p, "\"$*\"") == 0)
                {
                    char tmp3[2];
                    tmp3[0] = p[2];
                    tmp3[1] = '\0';
                    len = 3;
                    i   = 0;
                    tmp = pos_params_expand(tmp3, 1);
                    if(tmp)
                    {
                        /* substitute the expanded word but leave the quotes */
                        i = p-pstart+1;
                        if((tmp2 = substitute_str(pstart, tmp, i, i+1)))
                        {
                            /* adjust our pointer to point to the new string */
                            free(pstart);
                            pstart = tmp2;
                            len = strlen(tmp);
                            expanded = 1;
                        }
                        free(tmp);
                    }
                    /* adjust our pointer to point to the new string */
                    p = pstart+i+len;
                }
                else
                {
                    /* toggle quote mode */
                    in_double_quotes = !in_double_quotes;
                }
                break;
                
            case '=':
                /* skip it if inside double quotes */
                if(in_double_quotes)
                {
                    break;
                }
                /* check the previous string is a valid var name */
                len = p-pstart;
                tmp = malloc(len+1);
                if(!tmp)
                {
                    fprintf(stderr, "%s: insufficient memory for internal buffers\n", SHELL_NAME);
                    break;
                }
                strncpy(tmp, pstart, len);
                tmp[len] = '\0';
                /*
                 * if the string before '=' is a valid var name, we have a variable
                 * assignment.. we set in_var_assign to indicate that, and we set
                 * var_assign_eq which indicates this is the first equals sign (we use
                 * this when performing tilde expansion -- see code above).
                 */
                if(is_name(tmp))
                {
                    in_var_assign = 1;
                    var_assign_eq++;
                    free(tmp);
                    break;
                }
                free(tmp);
                /*
                 * csh-like dirstack expansions take the form of '=n'; entries are zero-based.
                 * the special '=-' notation refers to the last entry in the stack.
                 */
                if(p == pstart || isspace(p[-1]))
                {
                    struct dirstack_ent_s *d = NULL;
                    if(isdigit(p[1]))
                    {
                        k = 0;
                        p2 = p+1;
                        while(*p2 && isdigit(*p2))
                        {
                            k = (k*10)+(*p2)-'0';
                            p2++;
                        }
                        d = get_dirstack_entryn(k, NULL);
                        if(!d)
                        {
                            p = p2-1;
                            break;
                        }
                        len = p2-p-1;
                    }
                    else if(p[1] == '-')
                    {
                        d = get_dirstack_entryn(stack_count-1, NULL);
                        if(!d)
                        {
                            p++;
                            break;
                        }
                        len = 1;
                    }
                    /* substitute the dirstack entry */
                    if(d)
                    {
                        /* save our current position in the word */
                        i = p-pstart;
                        /*
                         * get a quoted version of the expanded string, so we can insert it
                         * in the original word knowing that it won't cause any trouble
                         * when we perform quote removal later on.
                         */
                        tmp = quote_val(d->path, !in_double_quotes);
                        if(tmp)
                        {
                            /* substitute the expanded word */
                            if((tmp2 = substitute_str(pstart, tmp, i, len+1)))
                            {
                                /* adjust our pointer to point to the new string */
                                free(pstart);
                                pstart = tmp2;
                                len = strlen(tmp);
                            }
                            free(tmp);
                        }
                        /* adjust our pointer to point to the new string */
                        p = pstart+i+len;
                        expanded = 1;
                    }
                }
                break;
                
            case '\\':
                /* skip backslash (we'll remove it later on) */
                p++;
                break;
                
            case '\'':
                /* if inside double quotes, treat the single quote as a normal char */
                if(in_double_quotes)
                {
                    break;
                }
                /* skip everything, up to the closing single quote */
                p += find_closing_quote(p, 0);
                break;
                
            case '`':
                /* find the closing back quote */
                if((len = find_closing_quote(p, 0)) == 0)
                {
                    /* not found. bail out */
                    break;
                }
                /* otherwise, extract the command and substitute its output */
                substitute_word(&pstart, &p, len+1, command_substitute, 0);
                expanded = 1;
                break;
                
#if 0
            case '(':                               /* non-POSIX arithmetic expression: ((expr)) */
                if(p[1] == '(')
                {
                    /* find the closing brace */
                    if((len = find_closing_brace(p)) == 0)
                    {
                        /* not found. bail out */
                        break;
                    }
                    /* otherwise, extract the arithmetic expression and substitute its value */
                    substitute_word(&pstart, &p, len, arithm_expand, 0);
                }
                break;
#endif
                
            /*
             * the $ sign might introduce:
             * - ANSI-C strings: $''
             * - arithmetic expansions (non-POSIX): $[] and $(())
             * - parameter expansions: ${var} or $var
             * - command substitutions: $()
             * - arithmetic expansions (POSIX): $()
             */
            case '$':
                c = p[1];
                switch(c)
                {
                    /*
                     * ANSI-C string 
                     */
                    case '\'':
                        /* find the closing quote */
                        if((len = find_closing_quote(p+1, 1)) == 0)
                        {
                            /* not found. bail out */
                            break;
                        }
                        /* otherwise, extract the string and substitute its value */
                        substitute_word(&pstart, &p, len+2, ansic_expand, 0);
                        expanded = 1;
                        break;
                        
                /*
                 * $[ ... ] is a deprecated form of integer arithmetic, similar to (( ... )).
                 */
                    case '{':
                    case '[':
                        /* find the closing quote */
                        if((len = find_closing_brace(p+1)) == 0)
                        {
                            /* not found. bail out */
                            break;
                        }
                        /* otherwise, extract the expression and substitute its value */
                        func = (c == '[') ? arithm_expand : var_expand;
                        /*
                         *  calling var_expand() might return an INVALID_VAR result which
                         *  makes the following call fail.
                         */
                        if(!substitute_word(&pstart, &p, len+2, func, 0))
                        {
                            free(pstart);
                            return NULL;
                        }
                        expanded = 1;
                        break;
                        
                    /*
                     * arithmetic expansion $(()) or command substitution $().
                     */
                    case '(':
                        /* check if we have one or two opening braces */
                        i = 0;
                        if(p[2] == '(')
                        {
                            i++;
                        }
                        /* find the closing quote */
                        if((len = find_closing_brace(p+1)) == 0)
                        {
                            /* not found. bail out */
                            break;
                        }
                        /*
                         * otherwise, extract the expression and substitute its value.
                         * if we have one brace (i == 0), we'll perform command substitution.
                         * otherwise, arithmetic expansion.
                         */
                        func = i ? arithm_expand : command_substitute;
                        substitute_word(&pstart, &p, len+2, func, 0);
                        expanded = 1;
                        break;
                        
                    case '#':
                        /*
                         * special variable substitution.
                         * $#@ and $#* both give the same result as $# (ksh extension).
                         */
                        if(p[2] == '@' || p[2] == '*')
                        {
                            delete_char_at(p, 2);
                        }
                        substitute_word(&pstart, &p, 2, var_expand, 0);
                        expanded = 1;
                        break;
                        
                    case '@':
                    case '*':
                    case '!':
                    case '?':
                    case '$':
                    case '-':
                    case '_':
                    case '<':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        substitute_word(&pstart, &p, 2, var_expand, 0);
                        expanded = 1;
                        break;
                        
                    default:
                        /* var names must start with an alphabetic char or _ */
                        if(!isalpha(p[1]) && p[1] != '_')
                        {
                            break;
                        }
                        p2 = p+1;
                        /* get the end of the var name */
                        while(*p2)
                        {
                            if(!isalnum(*p2) && *p2 != '_')
                            {
                                break;
                            }
                            p2++;
                        }
                        /* empty name */
                        if(p2 == p+1)
                        {
                            break;
                        }
                        /* perform variable expansion */
                        substitute_word(&pstart, &p, p2-p, var_expand, 0);
                        expanded = 1;
                        break;
                }
                break;

            default:
                /*
                 * we should not have whitespace chars in tokens, as the
                 * parser should have got rid of these. however, shit happens.
                 * for example, when we are passed an alias string from the
                 * backend executor. alias names almost always contain whispaces.
                 * we need to manually remove the whitespace chars and split the
                 * token in two (but beware not to remove whitespaces from within
                 * quoted strings and heredocs).
                 */
                if(isspace(*p) && !in_double_quotes)
                {
                    expanded = 1;
                }
                break;
        }
    } while(*(++p));
    
    /* if we performed word expansion, do field splitting */
    struct word_s *words = NULL;
    if(expanded)
    {
        words = field_split(pstart);
    }
    
    /* no expansion done, or no field splitting done */
    if(!words)
    {
        words = make_word(pstart);
        /* error making word struct */
        if(!words)
        {
            fprintf(stderr, "%s: insufficient memory\n", SHELL_NAME);
            free(pstart);
            return NULL;
        }
    }
    free(pstart);
    return words;
}


/*
 * perform brace expansion, followed by word expansion on each word that resulted from the
 * brace expansion.. if no brace expansion is done, performs word expansion on the given word.
 * head contains the word to be expanded.. in_heredoc tells us if we are performing the
 * word expansion inside a heredoc.. flags tell us if we should strip quotes and spaces
 * from the expanded word.
 *
 * returns the head of the linked list of the expanded fields and stores the last field
 * in the tail pointer.
 */
struct word_s *word_expand(char *orig_word)
{
    size_t count = 0, i;
    char **list = brace_expand(orig_word, &count);
    /* if no braces expanded, go directly to word expansion */
    if(!list)
    {
        list = malloc(sizeof(char *));
        if(!list)
        {
            return NULL;
        }
        list[0] = get_malloced_str(orig_word);
        count = 1;
    }
    /* expand the braces and do word expansion on each resultant field */
    struct word_s *wordlist = NULL, *listtail = NULL;
    for(i = 0; i < count; i++)
    {
        struct word_s *w = word_expand_one_word(list[i]);
        if(w)
        {
            /* add the words list */
            if(wordlist)
            {
                listtail->next = w;
            }
            else
            {
                wordlist = w;
                listtail = w;
            }
            /* go to the last word */
            while(listtail->next)
            {
                listtail = listtail->next;
            }
        }
    }
    /* free used memory */
    for(i = 0; i < count; i++)
    {
        free_malloced_str(list[i]);
    }
    free(list);

    /* perform pathname expansion and quote removal */
    wordlist = pathnames_expand(wordlist);
    remove_quotes(wordlist);

    /* return the expanded list */
    return wordlist;
}


struct word_s *pathnames_expand(struct word_s *words)
{
    /* no pathname expansion if -f option is not set */
    if(!option_set('f'))
    {
    	return words;
    }

    char *root = "/";
    struct word_s *w = words;
    struct word_s *pw = NULL;
    while(w)
    {
        char *dir = NULL;
        char *p = w->data;
        /* check if we should perform filename globbing */
        if(!has_regex_chars(p, strlen(p)))
        {
        	pw = w;
            w = w->next;
            continue;
        }
        if(*p == '/')
        {
            dir = root;
        }
        else
        {
            dir = cwd ;
        }
        glob_t glob;
        char **matches = filename_expand(dir, p, &glob);
        /* no matches found */
        if(!matches || !matches[0])
        {
            globfree(&glob);
            /* remove the word (bash extension) */
            if(optionx_set(OPTION_NULL_GLOB))
            {
            	if(w == words)
            	{
            		w = w->next;
            		words->next = NULL;
            		free_all_words(words);
            		words = w;
            	}
            	else
            	{
                	pw->next = w->next;
            		w->next = NULL;
            		free_all_words(w);
            		w = pw->next;
            	}
        		continue;
            }
            /* print error and bail out (bash extension) */
            if(optionx_set(OPTION_FAIL_GLOB))
            {
                fprintf(stderr, "%s: file globbing failed for %s\n", SHELL_NAME, p);
                return NULL;
            }
        }
        else
        {
            /* save the matches */
        	struct word_s *head = NULL, *tail = NULL;
            size_t j = 0;
            for( ; j < glob.gl_pathc; j++)
            {
                if(matches[j][0] == '.' && (matches[j][1] == '.' || matches[j][1] == '\0'))
                {
                    continue;
                }
                if(!head)
                {
                	head = make_word(matches[j]);
                	tail = head;
                }
                else
                {
                	tail->next = make_word(matches[j]);
                	if(tail->next)
                	{
                		tail = tail->next;
                	}
                }
            }
            /* add the new list */
    		tail->next = w->next;
    		w->next = NULL;
    		free_all_words(w);
    		w = tail;
        	if(w == words)
        	{
        		words = head;
        	}
        	/* free the matches list */
            globfree(&glob);
            /* finished */
        }
        pw = w;
        w = w->next;
    }
    return words;
}


void remove_quotes(struct word_s *wordlist)
{
    if(!wordlist)
    {
        return;
    }

	int in_double_quotes = 0;
    struct word_s *word = wordlist;
    char *p;
    while(word)
    {
    	p = word->data;
    	while(*p)
    	{
    		switch(*p)
    		{
    			case '"':
                    /* toggle quote mode */
                    in_double_quotes = !in_double_quotes;
					delete_char_at(p, 0);
                    break;

                case '\'':
                    delete_char_at(p, 0);
                    /* find the closing quote */
                    while(*p && *p != '\'')
                    {
                        p++;
                    }
                    /* and remove it */
                    if(*p == '\'')
                    {
                        delete_char_at(p, 0);
                    }
                    break;

                case '`':
                    delete_char_at(p, 0);
                    break;

                case '\v':
                case '\f':
    			case '\t':
                case '\r':
    			case '\n':
    			    /*
    			     *  convert whitespace chars inside double quotes to spaces,
    			     *  which is non-POSIX, but done by all major shells.
    			     */
    			    /*
    			    if(in_double_quotes)
    			    {
    			        *p = ' ';
    			    }
    			    */
                    p++;
                    break;

    			case '\\':
    				if(in_double_quotes)
    				{
    					switch(p[1])
    					{
    						/*
    						 * in double quotes, backslash preserves its special quoting
    						 * meaning only when followed by one of the following chars.
    						 */
    						case  '$':
    						case  '`':
    						case  '"':
    						case '\\':
    						case '\n':
    							delete_char_at(p, 0);
    							p++;
    							break;

    						default:
    							p++;
    							break;
    					}
    				}
    				else
    				{
    					/* parse single-character backslash quoting. */
    					delete_char_at(p, 0);
						p++;
    				}
    				break;

    			default:
            		p++;
    				break;
    		}
    	}
    	word = word->next;
    }
}


/*
 * A simple shortcut to perform word-expand on a string,
 * returning the result as a string.
 */
char *word_expand_to_str(char *word)
{
    struct word_s *w = word_expand(word);
    if(!w)
    {
        return NULL;
    }
    char *res = __tok_to_str(w);
    free_all_words(w);
    return res;
}


/*
 * check if char c is a valid $IFS character.
 *
 * returns 1 if char c is an $IFS character, 0 otherwise.
 */
static inline int is_IFS_char(char c, char *IFS)
{
    if(!*IFS)
    {
        return 0;
    }
    do
    {
        if(c == *IFS)
        {
            return 1;
        }
    } while(*++IFS);
    return 0;
}


/*
 * skip all whitespace characters that are part of $IFS.
 */
void skip_IFS_whitespace(char **__str, char *__IFS)
{
    char *IFS = __IFS;
    char *s   = *__str;
    do
    {
        if(*s == *IFS)
        {
            s++;
            IFS = __IFS-1;
        }
    } while(*++IFS);
    *__str = s;
}


/*
 * skip $IFS delimiters, which can be whitespace characters as well as other chars.
 */
void skip_IFS_delim(char *str, char *IFS_space, char *IFS_delim, size_t *_i, size_t len)
{
    size_t i = *_i;
    while((i < len) && is_IFS_char(str[i], IFS_space))
    {
        i++;
    }
    while((i < len) && is_IFS_char(str[i], IFS_delim))
    {
        i++;
    }
    while((i < len) && is_IFS_char(str[i], IFS_space))
    {
        i++;
    }
    *_i = i;
}


/*
 * convert the words resulting from a word expansion into separate fields.
 *
 * returns a pointer to the first field, NULL if no field splitting was done.
 */
struct word_s *field_split(char *str)
{
    struct symtab_entry_s *entry = get_symtab_entry("IFS");
    char *IFS = entry ? entry->val : NULL;
    /* POSIX says no IFS means: "space/tab/NL" */
    if(!IFS)
    {
        IFS = " \t\n";
    }
    /* POSIX says empty IFS means no field splitting */
    if(IFS[0] == '\0')
    {
        return (struct word_s *)0;
    }
    /* get the IFS spaces and delimiters separately */
    char IFS_space[16];
    char IFS_delim[16];
    if(strcmp(IFS, " \t\n") == 0)   /* "standard" IFS */
    {
        IFS_space[0] = ' ' ;
        IFS_space[1] = '\t';
        IFS_space[2] = '\n';
        IFS_space[3] = '\0';
        IFS_delim[0] = '\0';
    }
    else                            /* "custom" IFS */
    {
        char *p  = IFS;
        char *sp = IFS_space;
        char *dp = IFS_delim;
        do
        {
            if(isspace(*p))
            //if(*p == ' ' || *p == '\t' || *p == '\n')
            {
                *sp++ = *p++;
            }
            else
            {
                *dp++ = *p++;
            }
        } while(*p);
        *sp = '\0';
        *dp = '\0';
    }

    size_t len    = strlen(str);
    size_t i      = 0, j = 0, k;
    int    fields = 1;
    char   quote  = 0;
    /* skip any leading whitespaces in the string */
    skip_IFS_whitespace(&str, IFS_space);
    /* estimate the needed number of fields */
    do
    {
        /* skip escaped chars */
        if(str[i] == '\\')
        {
            /* backslash has no effect inside single quotes */
            if(quote != '\'')
            {
                i++;
            }
            continue;
        }
        /* don't count whitespaces inside quotes */
        else if(str[i] == '\'' || str[i] == '"' || str[i] == '`')
        {
            if(quote == str[i])
            {
                quote = 0;
            }
            else
            {
                quote = str[i];
            }
            continue;
        }
        if(quote)
        {
            continue;
        }
        if(is_IFS_char(str[i], IFS_space) || is_IFS_char(str[i], IFS_delim))
        {
            skip_IFS_delim(str, IFS_space, IFS_delim, &i, len);
            if(i < len)
            {
                fields++;
            }
        }
    } while(++i < len);
    /* we have only one field. no field splitting needed */
    if(fields == 1)
    {
        return (struct word_s *)NULL;
    }
    struct word_s *first_field = (struct word_s *)NULL;
    struct word_s *cur         = (struct word_s *)NULL;
    /* create the fields */
    i     = 0;
    j     = 0;
    quote = 0;
    do
    {
        /* skip escaped chars */
        if(str[i] == '\\')
        {
            /* backslash has no effect inside single quotes */
            if(quote != '\'')
            {
                i++;
            }
            continue;
        }
        /* skip single quoted substrings */
        else if(str[i] == '\'')
        {
            char *p = str+i+1;
            while(*p && *p != '\'')
            {
                p++;
            }
            i = p-str;
            continue;
        }
        /* remember if we're inside/outside double and back quotes */
        else if(str[i] == '"' || str[i] == '`')
        {
            if(quote == str[i])
            {
                quote = 0;
            }
            else
            {
                quote = str[i];
            }
            continue;
        }
        /* skip normal characters if we're inside quotes */
        if(quote)
        {
            continue;
        }
        /*
         * delimit the field if we have an IFS space or delimiter char, or if
         * we reached the end of the input string.
         */
        if(is_IFS_char(str[i], IFS_space) ||
           is_IFS_char(str[i], IFS_delim) || (i == len))
        {
            /* copy the field text */
            char *tmp = (char *)malloc(i-j+1);
            /* TODO: do something better than bailing out here */
            if(!tmp)
            {
                BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "making fields", NULL);
                return first_field;
            }
            strncpy(tmp, str+j, i-j);
            tmp[i-j] = '\0';
            /* create a new struct for the field */
            struct word_s *fld = (struct word_s *)malloc(sizeof(struct word_s));
            /* TODO: do something better than bailing out here */
            if(!fld)
            {
                free(tmp);
                return first_field;
            }
            fld->data = tmp;
            fld->len  = i-j;
            fld->next = 0;
            if(!first_field)
            {
                first_field = fld;
            }
            if(!cur)
            {
                cur  = fld;
            }
            else
            {
                cur->next = fld;
                cur       = fld;
            }
            k = i;
            /* skip trailing IFS spaces/delimiters */
            skip_IFS_delim(str, IFS_space, IFS_delim, &i, len);
            j = i;
            if(i != k)
            {
                i--;     /* go back one step so the loop will work correctly */
            }
        }
    } while(++i <= len);
    return first_field;
}
