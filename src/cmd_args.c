/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: cmd_args.c
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


struct cmd_token empty_cmd_token = { .data = "", .len = 0, .next = NULL };

#define IS_QUOTE(c)         ((c == '`') || (c == '\'') || (c == '"'))
#define INVALID_VAR     ((char *)-1)

char *__do_ansic(char *str);

/* defined in cmdline.c */
extern char  *stdin_filename;


void free_all_tokens(struct cmd_token *first)
{
    while(first)
    {
        struct cmd_token *del = first;
        first = first->next;
        if(del->data) free(del->data);
        free(del);
    }
}

static inline int is_alphanum(char c)
{
  if(c >= 'A' && c <= 'Z') return 1;
  if(c >= 'a' && c <= 'z') return 1;
  if(c >= '0' && c <= '9') return 1;
  if(c == '_') return 1;
  return 0;
}

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
 * restricted shells can't set/unset the values of SHELL, ENV, FPATH, 
 * or PATH. this function checks if the given name is one of these.
 * bash also restricts BASH_ENV.
 */
int is_restrict_var(char *name)
{
    if(strcmp(name, "SHELL") == 0 || strcmp(name, "ENV" ) == 0 ||
       strcmp(name, "FPATH") == 0 || strcmp(name, "PATH") == 0)
        return 1;
    return 0;
}

char *get_all_vars(char *prefix)
{
    int i;
    int len = strlen(prefix);
    char *buf = NULL;
    int bufsz = 0;
    /* use the first character of $IFS if defined, otherwise use space */
    struct symtab_entry_s *IFS = get_symtab_entry("IFS");
    char c = (IFS && IFS->val && IFS->val[0]) ? IFS->val[0] : ' ';
    char sp[2] = { c, '\0' };
    struct symtab_stack_s *stack = get_symtab_stack();
    int first = 1;

    for(i = 0; i < stack->symtab_count; i++)
    {
        struct symtab_s *symtab = stack->symtab_list[i];
    
#ifdef USE_HASH_TABLES
    
        if(symtab->used)
        {
            struct symtab_entry_s **h1 = symtab->items;
            struct symtab_entry_s **h2 = symtab->items + symtab->size;
            for( ; h1 < h2; h1++)
            {
                struct symtab_entry_s *entry = *h1;
                
#else

        struct symtab_entry_s *entry  = symtab->first;
            
#endif
            
                while(entry)
                {
                    if(strncmp(entry->name, prefix, len) == 0)
                    {
                        /* add 2 for ' ' and '\0' */
                        int len2 = strlen(entry->name)+2;
                        if(!buf)
                        {
                            buf = malloc(len2);
                            if(!buf) break;
                            buf[0] = '\0';
                        }
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
                            if(!(buf2 = realloc(buf, bufsz+len2))) break;
                            buf = buf2;
                        }
                        bufsz += len2;
                        if(first) first = 0;
                        else strcat(buf, sp);
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
     * get the special variables.
     */
    for(i = 0; i < special_var_count; i++)
    {
        char *v = special_vars[i].name;
        if(strncmp(v, prefix, len) == 0)
        {
            /* add 2 for ' ' and '\0' */
            int len2 = strlen(v)+2;
            if(!buf)
            {
                buf = malloc(len2);
                if(!buf) break;
                buf[0] = '\0';
            }
            else
            {
                /* don't duplicate */
                if(strstr(buf, v))
                {
                    continue;
                }
                /* make some room */
                char *buf2;
                if(!(buf2 = realloc(buf, bufsz+len2))) break;
                buf = buf2;
            }
            bufsz += len2;
            if(first) first = 0;
            else strcat(buf, sp);
            strcat(buf, v);
        }
    }
    return buf;
}


#define RESTORE_TRAPS_AND_EXIT(retval)                  \
do {                                                    \
    /* restore the saved traps and free used memory */  \
    restore_trap("DEBUG" , debug);                      \
    restore_trap("RETURN", ret  );                      \
    restore_trap("ERR"   , err  );                      \
    set_option('e', esave);                             \
    return (retval);                                    \
} while(0)

/*
 * the backquoted flag tells if we are called from a backquoted command substitution:
 *    `command`
 * or a regular one:
 *    $(command)
 */
char *__do_command(char *__cmd, int backquoted)
{
    char    b[1024];
    size_t  bufsz = 0;
    char   *buf   = NULL;
    char   *p     = NULL;
    int     i     = 0;

    /* 
     * fix cmd in the backquoted version. we will be modifying the command, hence
     * our use of __get_malloced_str() instead of get_malloced_str().
     */
    char *cmd = __get_malloced_str(__cmd);
    if(!cmd)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "command substitution", NULL);
        return NULL;
    }
    if(backquoted)
    {
        char *p1 = cmd;
        do
        {
            if(*p1 == '\\' &&
               (p1[1] == '$' || p1[1] == '`' || p1[1] == '\\'))
            {
                char *p2 = p1, *p3 = p1+1;
                while((*p2++ = *p3++)) ;
            }
        } while(*(++p1));
    }
    struct cmd_token *tok = make_cmd_token(cmd);
    free(cmd);
    struct cmd_token *tail;
    /*
     * word-expand the commandline, but don't strip the quotes (so that quoted command words will
     * result in the correct number of command arguments in the subshell).
     */
    struct cmd_token *t = word_expand(tok, &tail, 0, 0);
    if(!t) return NULL;
    char *cmd2 = __tok_to_str(t);
    free_all_tokens(t);

    /*
     * reset the DEBUG trap if -o functrace (-T) is not set, and the ERR trap
     * if -o errtrace (-E) is not set. traced functions inherit both traps
     * from the calling shell (bash).
     */
    struct trap_item_s *debug = NULL, *err = NULL, *ret = NULL;
    if(!option_set('T'))
    {
        debug = save_trap("DEBUG" );
        ret   = save_trap("RETURN");
    }
    if(!option_set('E')) err = save_trap("ERR");
    /*
     * the -e (errexit) option is reset in subshells if inherit_errexit
     * is not set (bash).
     */
    int esave = option_set('e');
    if(!optionx_set(OPTION_INHERIT_ERREXIT)) set_option('e', 0);

    FILE *fp = NULL;
    if(cmd2[0] == '<')
    {
        /*
         * handle the special case of $(<file), a shorthand for $(cat file).
         * this is a common non-POSIX extension seen in bash, ksh...
         */
        cmd = cmd2+1;
        while(isspace(*cmd)) cmd++;
        if(!*cmd) RESTORE_TRAPS_AND_EXIT(NULL);
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
            RESTORE_TRAPS_AND_EXIT(NULL);
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
                RESTORE_TRAPS_AND_EXIT(NULL);
            }
            strcpy(buf, b);
            RESTORE_TRAPS_AND_EXIT(buf);
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
    
    if(!fp)
    {
        free(cmd2);
        BACKEND_RAISE_ERROR(FAILED_TO_OPEN_PIPE, strerror(errno), NULL);
        RESTORE_TRAPS_AND_EXIT(NULL);
    }

    while((i = fread(b, 1, 1024, fp)))
    {
        if(!buf)
        {
            buf = malloc(i);
            if(!buf) goto fin;
            p   = buf;
        }
        else
        {
            char *buf2 = realloc(buf, bufsz+i);
            //if(buf != buf2) free(buf);
            if(!buf2)
            {
                buf = NULL;
                free(buf);
                goto fin;
            }
            buf = buf2;
            p   = buf+bufsz;
        }
        bufsz += i;
        memcpy(p, b, i);
    }
    if(!bufsz)
    {
        RESTORE_TRAPS_AND_EXIT(NULL);
    }
    /* now remove any trailing newlines */
    i = bufsz-1;
    while(buf[i] == '\n' || buf[i] == '\r')
    {
        buf[i] = '\0';
        i--;
    }
    
fin:
    if(cmd2[0] == '<') fclose(fp);
    else               pclose(fp);
    free(cmd2);
    /* restore the saved traps and free used memory */
    restore_trap("DEBUG" , debug);
    restore_trap("RETURN", ret  );
    restore_trap("ERR"   , err  );
    set_option('e', esave);
    if(!buf)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "command substitution", NULL);
        free(cmd2);
        return NULL;
    }
    return buf;
}


long int doop(long int n1, long int n2, char op)
{
    switch(op)
    {
        case '+': return n1+n2;
        case '-': return n1-n2;
        case '*': return n1*n2;
        case '/': return n1/n2;
        case '%': return n1%n2;
        case '>': return (n1 > n2) ? 1 : 0;
        case '<': return (n1 < n2) ? 1 : 0;
        case '1': return (n1 >= n2) ? 1 : 0;
        case '2': return (n1 >= n2) ? 1 : 0;
        case '3': return (n1 != n2) ? 1 : 0;
        case '4': return (n1 == n2) ? 1 : 0;
        case '5': return ++n1;
        case '6': return --n1;
        case '=': return n1;
        //case '^': return pow(n1, n2);
    }
    return n1;
}


char get_xdigit(char c)
{
    if(c >= '0' && c <= '9')      c -= '0';
    else if(c >= 'a' && c <= 'z') c = c-'a'+10;
    else                          c = c-'A'+10;
    return c;
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
 */
char *__do_var_info(char op, char *orig_val, char *var_name, int name_len)
{
    char *sub;
    switch(op)
    {
        case 'Q':
            return quote_val(orig_val);
                            
        case 'E':
            return __do_ansic(orig_val);
                            
        case 'P':
            return __evaluate_prompt(orig_val);

        case 'A':
            /* calc needed space */
            name_len = name_len+6+strlen(orig_val);
            /* alloc space */
            sub = malloc(name_len+1);
            if(!sub) return NULL;
            char *tmp = quote_val(orig_val);
            sprintf("let %s=%s", var_name, tmp);
            if(tmp) free(tmp);
            return sub;
    }
    return NULL;
}

/*
 * our options are:
 *                  POSIX description   var defined     var undefined
 * ========         =================   ===========     =============
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

char *__do_var(char *__var_name, struct cmd_token **tokens)
{
    if(!__var_name) return (char *)0;
    int get_length = 0;
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
    if(!*__var_name) return (char *)0;
    /* sanity check the name */
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

    int   colon = 0;
    char *sub   = strchr(__var_name, ':');
    if(sub)
    {
        colon = 1;
    }
    else
    {
        char *v = __var_name;
        /* don't mistake a '#' variable name */
        if(*v == '#' && v[1] == '\0') v++;
        /* make sure we don't mistake a special var for a substitution string */
        else if(*v == '-' || *v == '=' || *v == '?' || *v == '+' || *v == '@') v++;
        sub = strchr_any(v, "-=?+%#@");
    }
    int len = sub ? sub-__var_name : strlen(__var_name);
    if(sub && *sub == ':') sub++;
    char var_name[len+1];
    strncpy(var_name, __var_name, len);
    var_name[len]   = '\0';
    
    /*
     * common extension (bash, ksh, ...) to return variables whose name
     * starts with a prefix. the format is ${!prefix*} or ${!prefix@}. the
     * latter expands each varname to a separate field if called within double
     * quotes (bash), but we treat both the same here for simplicity.
     */
    if(var_name[0] == '!' && (var_name[len-1] == '*' || var_name[len-1] == '@'))
    {
        var_name[len-1] = '\0';
        return get_all_vars(var_name+1);
    }

    /*
     * commence variable substitution.
     */
    char *empty_val = "";
    char *tmp       = (char *)0;
    char setme      = 0;
    char *orig_val  = NULL;
    int   pos_params = 0;
    /* try special variable names */
    tmp = get_special_var(var_name);
    if(!tmp)
    {
        struct symtab_entry_s *entry = get_symtab_entry(var_name);
        if(entry)
        {
            tmp = entry->val;
        }
        else tmp = empty_val;
    }
    orig_val = tmp;
    if(strcmp(var_name, "@") == 0 || strcmp(var_name, "*") == 0)
    {
        pos_params = 1;
        if(get_length) goto normal_value;
        return NULL;
    }
    if(strcmp(var_name, "<") == 0)      /* tcsh extension to read directly from stdin */
    {
        if(!isatty(0)) return NULL;
        int line_max = get_linemax();
        char *in = malloc(line_max);
        if(!in) return NULL;
        if(src->filename == stdin_filename) term_canon(1);
        if(fgets(in, line_max, stdin))
        {
            if(get_length)
            {
                len = strlen(in);
                char buf[len+1];
                sprintf(buf, "%d", len);
                tmp = __get_malloced_str(buf);
            }
            else tmp = __get_malloced_str(in);
            free(in);
            if(src->filename == stdin_filename) term_canon(0);
            return tmp;
        }
        if(src->filename == stdin_filename) term_canon(0);
        return NULL;
    }
    
    /*
     * variable is unset or empty.
     */
    if(!tmp || tmp == empty_val)
    {
        /* no unset parameters are accepted */
        if(option_set('u') && !pos_params)
        {
            BACKEND_RAISE_ERROR(UNSET_VARIABLE, var_name, "parameter not set");
            if(!option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
            return INVALID_VAR;
        }
        
        if(sub)
        {
            if(!colon && tmp == empty_val) return NULL;
            switch(sub[0])
            {
                case '-':
                    tmp = sub+1;  /* 3rd case in the table above */
                    break;
                    
                case '=':          /* 4th case in the table above */
                    /* 
                     * only variables, not positional or special parameters can be
                     * assigned this way.
                     */
                    if(is_pos_param(var_name) || is_special_param(var_name))
                    {
                        BACKEND_RAISE_ERROR(INVALID_ASSIGNMENT, __var_name, NULL);
                        /* NOTE: this is not strict POSIX behaviour. see the table. */
                        if(!option_set('i'))
                        {
                            if(option_set('e'))
                                exit_gracefully(EXIT_FAILURE, NULL);
                            else
                                trap_handler(ERR_TRAP_NUM);
                        }
                        return INVALID_VAR;
                    }
                    tmp = sub+1;
                    /* 
                     * assign the EXPANSION OF tmp, not tmp
                     * itself, to var_name.
                     */
                    //__set(var_name, word_expand_to_str(tmp), 0, 0);
                    setme = 1;
                    break;
                
                case '?':          /* 5th case in the table above */
                    /*
                     * TODO: we should use the EXPANSION OF the string starting at (sub+1),
                     *       not the original word itself as passed to us.
                     */
                    if(sub[1] == '\0')
                        BACKEND_RAISE_ERROR(UNSET_VARIABLE, var_name, "parameter not set");
                    else
                        BACKEND_RAISE_ERROR(UNSET_VARIABLE, var_name, sub+1);
                    if(!option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
                    return INVALID_VAR;
                    break;
                
                /* 6th case in the table above */
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
                    goto normal_value;
                    
                default:
                    return NULL;
            }
        }
        else return NULL;
    }
    /*
     * variable is set/not empty.
     */
    else
    {
        /* 6th case in the table above */
        if(sub)
        {
            switch(sub[0])
            {
                /*
                 * TODO: implement the match/replace functions of ${parameter/pattern/string} 
                 *       et al. see bash manual page 35.
                 */
                case '/':
                    goto normal_value;
                    
                case '+':
                    tmp = sub+1;
                    break;

                /*
                 * bash extension for variable expansion. it takes the form of:
                 *       ${parameter@operator}
                 * where operator is a single letter Q, E, P, A or a.
                 */
                case '@':
                    sub = __do_var_info(sub[1], orig_val, var_name, len);
                    if(!sub) goto normal_value;
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
                    p = word_expand_to_str(tmp);
                    if(!p)
                    {
                        EXIT_IF_NONINTERACTIVE();
                        return INVALID_VAR;
                    }
                    int longest = 0;
                    if(*sub == '%') longest = 1, sub++;
                    if((len = match_suffix(sub, p, longest)) == 0)
                    {
                        return p;
                    }
                    char *p2 = get_malloced_strl(p, 0, len);
                    free(p);
                    return p2;

                case '#':       /* match prefix */
                    sub++;
                    p = word_expand_to_str(tmp);
                    if(!p)
                    {
                        EXIT_IF_NONINTERACTIVE();
                        return INVALID_VAR;
                    }
                    longest = 0;
                    if(*sub == '#') longest = 1, sub++;
                    if((len = match_prefix(sub, p, longest)) == 0)
                    {
                        return p;
                    }
                    p2 = get_malloced_strl(p, len, strlen(p)-len);
                    free(p);
                    return p2;

                case '-':
                case '=':
                case '?':
                    goto normal_value;
                    
                default:
                    /* 
                     * ${parameter:offset}
                     * ${parameter:offset:length}
                     */
                    while(isspace(*sub)) sub++;
                    tmp = strchr(sub, ':');
                    long off, len;
                    long sublen = strlen(sub);
                    if(tmp)
                    {
                        off = extract_num(sub, 0, tmp-sub);
                        len = extract_num(sub, (tmp-sub)+1, sublen);
                    }
                    else
                    {
                        off = extract_num(sub, 0, sublen);
                        len = strlen(orig_val)-off;
                    }
                    /* negative offsets count from the end of string */
                    long vallen = strlen(orig_val);
                    if(off < 0) off += vallen;
                    if(len < 0)
                    {
                        /* both are offsets now */
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
                    if(!var_val && !option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
                    return var_val ? : INVALID_VAR;
                    //goto normal_value;
            }
        }
        else
        {
            goto normal_value;
        }
    }
    /* do POSIX style on the word */
    struct cmd_token *tok = make_cmd_token(tmp);
    struct cmd_token *tail;
    struct cmd_token *t = word_expand(tok, &tail, 0, 1);
    if(!t) return (char *)0;
    
    if(setme)
    {
        /*
         * NOTE: this is not very effective, as tmp might be
         *       expanded again down there.
         */
        tmp = __tok_to_str(t);
        __set(var_name, tmp, 0, 0, 0);
        free(tmp);
    }
    
    char buf[32];
    if(get_length)
    {
        if(pos_params)
        {
            sprintf(buf, "%d", pos_param_count());
        }
        else
        {
            tmp = __tok_to_str(t);
            free_all_tokens(t);
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
        *tokens = t;
        return (char *)0;
    }
    
normal_value:
    if(get_length)
    {
        if(pos_params)
        {
            sprintf(buf, "%d", pos_param_count());
        }
        else
        {
            if(!tmp)
            {
                return __get_malloced_str("0");
            }
            sprintf(buf, "%lu", strlen(tmp));
        }
        return __get_malloced_str(buf);
    }
    else
    {
        /* "normal" variable value */
        char *var_val = __get_malloced_str(tmp);
        /* POSIX says non-interactive shell should exit on expansion errors */
        if(!var_val && !option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
        return var_val ? : INVALID_VAR;
    }
}


struct cmd_token *__do_pos_params(char *tmp, size_t len, char in_double_quotes)
{
    errno = 0;
    char *sub = strchr(tmp, ':');
    struct cmd_token *p;
    if(sub)
    {
        /* 
         * ${parameter:offset}
         * ${parameter:offset:length}
         */
        sub++;
        while(isspace(*sub)) sub++;
        char *tmp2 = strchr(sub, ':');
        long off, len;
        long sublen = strlen(sub);
        long count = pos_param_count()+1;
        if(count <= 0) return NULL;
        if(tmp2)
        {
            off = extract_num(sub, 0, tmp2-sub);
            len = extract_num(sub, (tmp2-sub)+1, sublen);
        }
        else
        {
            off = extract_num(sub, 0, sublen);
            len = count-off;
        }
        /* negative offsets count from the end of array */
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
        p = get_pos_params(*tmp, in_double_quotes, off, len);
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
            if(!p || !p->val) continue;
            sub = __do_var_info(op, p->val, p->name, strlen(p->name));
            if(sub) subs[l++] = sub;
        }
        subs[l++] = NULL;
        char *res = list_to_str(subs, 0);
        for(k = 0; subs[k]; k++) free(subs[k]);
        p = (struct cmd_token *)malloc(sizeof(struct cmd_token));
        if(p)
        {
            p->data = res;
            p->len  = strlen(res);
            p->next = NULL;
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
        if(!sub) sub = strchr(tmp, '%');
        if(!sub)
        {
            p = get_all_pos_params(*tmp, in_double_quotes);
        }
        else
        {
            int  count = pos_param_count();
            char *subs[count+1];
            char op = *sub++;
            int  longest = 0, k, l;
            int  (*func)(char *, char *, int) = (op == '#') ? match_prefix : match_suffix;
            if(*sub == op) longest = 1, sub++;
            for(k = 1, l = 0; k <= count; k++)
            {
                struct symtab_entry_s *p = get_pos_param(k);
                if(!p || !p->val) continue;
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
            char *res = list_to_str(subs, 0);
            for(k = 0; subs[k]; k++) free(subs[k]);
            p = (struct cmd_token *)malloc(sizeof(struct cmd_token));
            if(p)
            {
                p->data = res;
                p->len  = strlen(res);
                p->next = NULL;
            }
        }
    }
    return p ? p : &empty_cmd_token;
}


char *__do_ansic(char *str)
{
    if(!str) return NULL;
    str = __get_malloced_str(str);
    if(!str) return NULL;
    char *s = str;
    char  c;
    int   i, j, k;
    char  buf[8];
    while(*s)
    {
        if(*s =='\\')
        {
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
                    i = 0;
                    if(isxdigit(*s  )) c = get_xdigit(*s), i++;
                    if(isxdigit(s[1])) c = c*16 + get_xdigit(s[1]), i++;
                    *s = c;
                    if(i != 1) delete_char_at(s, 1);
                    break;

                /*
                 * we use UTF-8. see https://en.wikipedia.org/wiki/UTF-8
                 */
                case 'U':           /* unicode char \UHHHHHHHH */
                    k = 8;
                    
                case 'u':           /* unicode char \uHHHH */
                    if(*s == 'u') k = 4;
                    delete_char_at(s, 0);
                    j = 0;
                    for(i = 0; i < k; i++)
                    {
                        if(isxdigit(s[i]))
                        {
                            c = get_xdigit(s[i]);
                            j = j*16 + c;
                        }
                        else break;
                    }
                    if(i == 0) break;
                    sprintf(buf, "%lc", j);
                    /* get the byte length of this UTF-8 codepoint */
                    if     (j >= 0x0000 && j <= 0x007f) j = 1;
                    else if(j >= 0x0080 && j <= 0x07ff) j = 2;
                    else if(j >= 0x0800 && j <= 0xffff) j = 3;
                    else                                j = 4;
                    /*
                     * check if we have space.
                     * i is the byte length of the original codepoint.
                     * j is the byte length of the codepoint UTF-8 value.
                     */
                    char *p1, *p2;
                    if(i < j)
                    {
                        /* extend buf */
                        int l = strlen(s);
                        char *s2 = realloc(str, l+j-i+1);
                        if(!s2) { free(str); return NULL; }
                        s = s2+(s-str);
                        str = s2;
                        p1 = s+l;
                        p2 = p1+j-i;
                        while(p1 >= s) *p2++ = *p1++;
                    }
                    else if(i > j)
                    {
                        /* shrink buf */
                        p1 = s+i;
                        p2 = s+j;
                        while((*p2++ = *p1++)) ;
                    }
                    /* now copy the UTF-8 codepoint */
                    p1 = buf;
                    while(*p1) *s++ = *p1++;
                    s--;
                    break;
                    
                case 'c':           /* CTRL-char */
                    delete_char_at(s, 0);
                    c = *s;
                    if(c >= 'a' && c <= 'z') c = c-'a'+1;
                    else if(c >= 'A' && c <= 'Z') c = c-'A'+1;
                    else if(c >= '[' && c <= '_') c = c-'['+0x1b;
                    else break;
                    *s = c;
                    break;
                    
                default:
                    if(isdigit(*s))
                    {
                        c = (*s)-'0';
                        i = 1;
                        if(isdigit(s[1])) c = c*8 + s[1]-'0', i++;
                        if(isdigit(s[2])) c = c*8 + s[2]-'0', i++;
                        *s = c;
                        if(i != 1) delete_char_at(s, 1), i--;
                        if(i != 1) delete_char_at(s, 1);
                    }
                    break;
            }
        }
        s++;
    }
    return str;
}

/*
 * start should point to the first char to be deleted from s.
 * end should point to the last char to be deleted from s, NOT the
 * char coming after it.
 */
char *__substitute(char *s, char *val, size_t start, size_t end)
{
    char before[start+1];
    strncpy(before, s, start);
    before[start]   = '\0';
    size_t afterlen = strlen(s)-end+1;
    char after[afterlen];
    strcpy(after, s+end+1);
    size_t totallen = start+afterlen+strlen(val);
    char *final = (char *)malloc(totallen+1);
    if(!final)
    {
        BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "performing variable substitution", NULL);
        /* POSIX says non-interactive shell should exit on expansion errors */
        if(!option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
        return NULL;
    }
    if(!totallen)
    {
        final[0] = '\0';
    }
    else
    {
        strcpy(final, before);
        strcat(final, val   );
        strcat(final, after );
    }
    return final;
}

void __substitute_var(struct cmd_token *tok, char *val, size_t start, size_t end)
{
    char *new = __substitute(tok->data, val, start, end);
    if(!new) return;
    free(tok->data);
    tok->data = new;
    tok->len  = strlen(new);
}


/*
 * sq_nesting is a flag telling us if we should allow
 * single quote nesting (prohibited by POSIX, but allowed
 * in ANSI-C strings).
 */
size_t find_closing_quote(char *data, int sq_nesting)
{
    char quote = data[0];
    if(quote != '\'' && quote != '"' && quote != '`') return 0;
    size_t i = 0, len = strlen(data);
    while(++i < len)
    {
        if(data[i] == quote)
        {
            if(data[i-1] == '\\')
            {
                if(quote != '\'' || sq_nesting) continue;
            }
            return i;
        }
    }
    return i;
}

size_t find_closing_brace(char *data)
{
    char opening_brace = data[0], closing_brace;
    if(opening_brace != '{' && opening_brace != '(' && opening_brace != '[') return 0;
    
    if(opening_brace == '{') closing_brace = '}';
    else if(opening_brace == '[') closing_brace = ']';
    else closing_brace = ')';
    
    size_t ob_count = 1, cb_count = 0;
    size_t i = 0, len = strlen(data);
    while(++i < len)
    {
        if((data[i] == '"') || (data[i] == '\'') || (data[i] == '`'))
        {
            if(data[i-1] == '\\') continue;
            char quote = data[i];
            while(++i < len)
            {
                if(data[i] == quote && data[i-1] != '\\') break;
            }
            if(i == len) return 0;
            continue;
        }
        if((data[i] == opening_brace) && (data[i-1] != '\\')) ob_count++;
        if((data[i] == closing_brace) && (data[i-1] != '\\')) cb_count++;
        if(ob_count == cb_count) break;
    }
    if(ob_count != cb_count) return 0;
    return i;
}

char *tilde_expand(char *s, size_t *_i, int in_var_assign)
{
    /* TODO: we should add full support for the "Tilde Prefix" as defined in the
     *       POSIX standard (section 2.6.1).
     */
    size_t i        = *_i;
    size_t len      = strlen(s);
    char   *s2      = NULL;
    char has_quotes = 0;
    struct symtab_entry_s *entry;
    if(s[0] == '~')
    {
        /* find the length of the tilde prefix */
        while(++i < len)
        {
            if(IS_QUOTE(s[i])) { has_quotes = !has_quotes; continue; }
            if(s[i] == '/' && s[i-1] != '\\' && !has_quotes) break;
            if(s[i] == ':' && s[i-1] != '\\' && !has_quotes && in_var_assign) break;
            //if(s[i] == '+' && s[i-1] != '\\' && !has_quotes) break;
            //if(s[i] == '-' && s[i-1] != '\\' && !has_quotes) break;
        }
        if(has_quotes)
        {
            i = 0;
        }
        /* null tilde prefix */
        else if(i == 1)
        {
            entry = get_symtab_entry("HOME");
            char *home = entry ? entry->val : NULL;
            if(home && home[0] != '\0')
            {
                s2 = __substitute(s, home, 0, 0);
                i  = strlen(home);
            }
            else
            {
                /* 
                 * POSIX doesn't what to do in this case (i.e. when $HOME is unset). bash uses 
                 * the home dir of the user currently executing the shell. we follow bash here.
                 */
                struct passwd *pass;
                pass = getpwuid(getuid());
                if(pass)
                {
                    s2 = __substitute(s, pass->pw_dir, 0, i-1);
                    i  = strlen(pass->pw_dir);
                }
                else i = 0;
            }
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
                    s2 = __substitute(s, dir, 0, i-1);
                    i  = strlen(dir);
                }
                else i = 0;
            }
            /*
             * TODO: process numeric tilde prefixes such as ~+N, ~-N and ~N, which
             *       represent entries in the dirs stack (see bash manual pages 30 and 96).
             */
            else
            {
                /* we have a login name */
                char login[i+1];
                strncpy(login, s+1, i-1);
                login[i] = '\0';
                struct passwd *pass;
                pass = getpwnam(login);
                if(pass)
                {
                    s2 = __substitute(s, pass->pw_dir, 0, i-1);
                    i  = strlen(pass->pw_dir);
                }
                else i = 0;
            }
        }
    }
    *_i = i;
    return s2;
}

/*
 * this function does the actual heavy work of variable and command
 * substitution. it calls any of the __do_command(), __do_var() and
 * __do_arithmetic() functions as appropriate. the return result is 
 * a token list containing the result fields.
 */
struct cmd_token *__word_expand(struct cmd_token *head, size_t i, size_t j, 
                                size_t len, char cmd, char in_double_quotes)
{
    char   *empty_val = "";
    size_t tmplen     = j-i;
    char tmp[tmplen+1];
    strncpy(tmp, (head->data)+i, tmplen);
    tmp[tmplen] = '\0';
    char *var_val;
    struct cmd_token *first = NULL;
    
    /* handle $* and $@ as a special case */
    if(*tmp == '*' || *tmp == '@')
    {
        return __do_pos_params(tmp, len, in_double_quotes);
    }
    
    /*
     * NOTE: POSIX says:
     *       The results of command substitution shall not be processed for further
     *       tilde expansion, parameter expansion, command substitution, or 
     *       arithmetic expansion.
     */
    switch(cmd)
    {
        case '`' : var_val = __do_command(tmp, 1 ); break;
        case '{' : var_val = __do_var(tmp, &first); break;
        case '[' : var_val = __do_arithmetic(tmp ); break;
        case '(' : var_val = __do_command(tmp, 0 ); break;
        case '\'': var_val = __do_ansic(tmp      ); break;
        default:
            BACKEND_RAISE_ERROR(EXPANSION_ERROR, tmp, NULL);
            EXIT_IF_NONINTERACTIVE();
            return NULL;    /* unknown command */
    }

    if(var_val == INVALID_VAR) return NULL;
    if(!var_val) var_val = empty_val;
    if(first) return first;
    if(var_val == empty_val) return &empty_cmd_token;
    struct cmd_token *res;
    if(in_double_quotes)
    {
        /* TODO:
         *      If command substitution occurs inside double-quotes:
         *      - Pathname expansion shall not be performed on the results of 
         *        the expansion.
         *      - Field splitting shall not be performed on the results of the
         *        expansion, with the exception of '@'.
         */
        res = make_cmd_token(var_val);
        free(var_val);
        return res;
    }
    else
    {
        first = __make_fields(var_val);
        /* no field splitting done? */
        if(!first)
        {
            res = make_cmd_token(var_val);
            free(var_val);
            return res;
        }
        else
        {
            return first;
        }
    }
}


/*
 * after word expansion and substitution, we need to do some housekeeping.
 * this function adjusts the pointers used by the word_expand() function,
 * so the latter can continue scanning the input string properly. specifically,
 * this function handles the edge case where substitution occurs in the middle
 * of a string, e.g.
 *     some_$(Substitute)_something
 * 
 * the __word_expand() function will return the $(Substitute) part, which we need
 * to insert in the middle of the string. we add the first field of the expanded
 * string to the head ("some_"), then we check to see if there are more characters
 * in the string. if there are (the "_something" part), we create another cmd_token
 * to hold that tail, then add it to the list and adjust the head and i pointers
 * accordingly.
 */
void __word_expand2(struct cmd_token **head, struct cmd_token *var, size_t *i, size_t j, size_t *len)
{
    var->len = strlen(var->data);
    __substitute_var(*head, var->data, *i, j-1);
    *i = var->len + *i;
    *len = (*head)->len;
    if(var->next == NULL)
    {
        if(var != &empty_cmd_token) free_all_tokens(var);
    }
    else
    {
        (*head)->next = var->next;
        if(*i >= *len)
        {
            if(var->data) free(var->data);
            free(var);
            return;
        }
        /* add the original word's tail to the new last word */
        char *s = get_malloced_str(var->data + (*i));
        if(var->data) free(var->data);
        free(var);
        (*head)->data[*i] = '\0';
        (*head)->len = *i;
        while((*head)->next)
        {
            (*head) = (*head)->next;
        }
        __substitute_var(*head, s, (*head)->len, (*head)->len);
        free_malloced_str(s);
        *len = (*head)->len;
        *i = 0;
    }
    /* the loop in word_expand() will increment i, so reduce one
     * from i to compensate for this.
     */
    (*i)--;
}


/* Step 4: Variable substitution */
struct cmd_token *word_expand_one_word(struct cmd_token *head, struct cmd_token **tail, int in_heredoc, int strip_quotes)
{
    struct cmd_token *res = head;
    if(!head || !head->data) return res;
    *tail = head;
    char   c;
    size_t i = 0, j = 0, k;
    size_t len;
    char in_double_quotes = 0;
    char has_quotes = 0;    /* flag to tell us if the command word contains ' or ".
                             * if after expansion we have empty field, and flag is not set,
                             * empty field will be removed from the final field list.
                             */
    struct cmd_token *var;
    
    
    /*
     * NOTE: a terrible hack is going on here. as in_heredoc == 1 indicates we
     *       are called from do_io_here(), we use in_heredoc == -1 to indicate
     *       we are called from do_simple_command() when it is processing a
     *       NODE_ASSIGNMENT variable assignment.
     *       we use this to set the in_var_assign flag for tilde_expand() so it
     *       knows to delimit tilde prefixes with '/ only or also with ':'.
     * 
     * TODO: so far, in tilde_expand() we are only expanding the very first tilde
     *       prefix, i.e. the one at the beginning of the word. we should comply
     *       with POSIX and support multiple tilde prefixes that can happen
     *       anywhere in an assignment, following any unqouted colon.
     */
    if(in_heredoc <= 0)
    {
        char *s = tilde_expand(head->data, &i, in_heredoc ? 1 : 0);
        if(s)
        {
            free(head->data);
            head->data = s;
            head->len  = strlen(s);
        }
        in_heredoc = 0;
    }

    do
    {
        char *data = head->data;
        len = head->len;
        switch(data[i])
        {
            case ':':
            case '=':
                if(data[i+1] == '~')
                {
                    i++;
                    j = 0;
                    char *s = tilde_expand(head->data+i, &j, in_heredoc ? 1 : 0);
                    if(s)
                    {
                        __substitute_var(head, s, i, i+j);
                        free(s);
                        i  += j;
                        len = head->len;
                    }
                }
                /*
                 * csh-like dirstack expansions take the form of '=n'. entries are zero-based.
                 * the special '=-' notation refers to the last entry in the stack.
                 */
                else if(!i || isspace(data[i-1]))
                {
                    if(isdigit(data[i+1]))
                    {
                        k = 0;
                        j = i+1;
                        while(data[j] && isdigit(data[j]))
                        {
                            k = (k*10)+data[j]-'0';
                            j++;
                        }
                        struct dirstack_ent_s *d = get_dirstack_entryn(k, NULL);
                        if(!d)
                        {
                            i = j;
                            break;
                        }
                        var = make_cmd_token(d->path);
                        __word_expand2(&head, var, &i, j, &len);
                    }
                    else if(data[i+1] == '-')
                    {
                        struct dirstack_ent_s *d = get_dirstack_entryn(stack_count-1, NULL);
                        if(!d)
                        {
                            i += 2;
                            break;
                        }
                        var = make_cmd_token(d->path);
                        __word_expand2(&head, var, &i, i+2, &len);
                    }
                }
                break;

            case ' ':
            case '\t':
            case '\n':
            case '\r':
            case '\v':
            case '\f':
                /*
                 * we should not have whitespace chars in tokens, as the
                 * parser should have got rid of these. however, shit happens.
                 * for example, when we are passed an alias string from the
                 * backend executor. alias names almost always contain whispaces.
                 * we need to manually remove the whitespace chars and split the
                 * token in two (but beware not to remove whitespaces from within
                 * quoted strings and heredocs).
                 */
                if(in_heredoc || in_double_quotes) break;
                char *t = data+i;
                while(*t && isspace(*t)) t++;
                data[i]    = '\0';
                head->len  = i;
                if(*t)
                {
                    struct cmd_token *new = make_cmd_token(t);
                    if(!new) break;
                    new->next  = head->next;
                    head->next = new;
                    head       = new;
                    len        = head->len;
                    i          = -1;
                }
                break;
                
            case '"':
                if(!in_heredoc)
                {
                    in_double_quotes = !in_double_quotes;
                    has_quotes = 1;
                    if(strip_quotes) delete_char_at(head->data, i--);
                    len--;
                }
                break;
                
            case '\\':
                if(in_double_quotes || in_heredoc)
                {
                    char c = head->data[i+1];
                    switch(c)
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
                            delete_char_at(head->data, i);
                            len--;
                            break;
                    }
                }
                else
                {
                    /* parse single-character backslash quoting. */
                    delete_char_at(head->data, i);
                    len--;
                }
                break;
                
            case '\'':
                if(in_double_quotes) { break; }
                j = find_closing_quote(head->data+i, 0);
                j += i;
                has_quotes = 1;
                if(strip_quotes)
                {
                    delete_char_at(head->data, j);
                    delete_char_at(head->data, i);
                    len -= 2;
                    i = j-2;
                }
                else i = j;
                break;
                
            case '`':
                j = i;
                j += find_closing_quote(head->data+i, 0);
                if(j == len) return res;
                if(strip_quotes)
                {
                    delete_char_at(head->data, j);
                    delete_char_at(head->data, i);
                    j   -= 1;
                    len -= 2;
                }
                else
                {
                    i++; j--;
                }
                if((var = __word_expand(head, i, j, len, '`', in_double_quotes)))
                    __word_expand2(&head, var, &i, j, &len);
                break;
                
            case '(':                               /* non-POSIX arithmetic expression: ((expr)) */
                if(head->data[i+1] == '(')
                {
                    j = find_closing_brace(head->data+i);
                    if(j == 0) return res;
                    j += (i);
                    delete_char_at(head->data, j  );    /* ) */
                    delete_char_at(head->data, j-1);    /* ) */
                    delete_char_at(head->data, i  );    /* ( */
                    delete_char_at(head->data, i  );    /* ( */
                    len -= 4;
                    j   -= 3;
                    if((var = __word_expand(head, i, j, len, '[', in_double_quotes)))
                        __word_expand2(&head, var, &i, j, &len);
                }
                break;
                
            case '$':
                c = head->data[i+1];
                if(c == '\'')         /* ANSI-C string */
                {
                    j = find_closing_quote(head->data+i+1, 1);
                    if(j == 0) return res;
                    j += (i+1);
                    delete_char_at(head->data, j);          /* ' */
                    delete_char_at(head->data, i);          /* $ */
                    delete_char_at(head->data, i);          /* ' */
                    j   -= 2;
                    len -= 3;
                    if((var = __word_expand(head, i, j, len, '\'', in_double_quotes)))
                        __word_expand2(&head, var, &i, j, &len);
                }
                /*
                 * $[ ... ] is a deprecated form of integer arithmetic, similar to (( ... )).
                 */
                else if(c == '{' || c == '[')
                {
                    j = find_closing_brace(head->data+i+1);
                    if(j == 0) return res;
                    j += (i+1);
                    delete_char_at(head->data, j);          /* } */
                    delete_char_at(head->data, i);          /* $ */
                    delete_char_at(head->data, i);          /* { */
                    j   -= 2;
                    len -= 3;
                    if((var = __word_expand(head, i, j, len, c, in_double_quotes)))
                        __word_expand2(&head, var, &i, j, &len);
                }
                else if(c == '(')
                {
                    char double_braces = 0;
                    if(head->data[i+2] == '(') { double_braces = 1; }
                    j = find_closing_brace(head->data+i+1);
                    if(j == 0) return res;
                    j += (i+1);
                    if(double_braces)
                    {
                        delete_char_at(head->data, j  );    /* ) */
                        delete_char_at(head->data, j-1);    /* ) */
                        delete_char_at(head->data, i  );    /* $ */
                        delete_char_at(head->data, i  );    /* ( */
                        delete_char_at(head->data, i  );    /* ( */
                        len -= 5;
                        j   -= 4;
                        if((var = __word_expand(head, i, j, len, '[', in_double_quotes)))
                            __word_expand2(&head, var, &i, j, &len);
                    }
                    else
                    {
                        delete_char_at(head->data, j  );    /* ) */
                        delete_char_at(head->data, i  );    /* $ */
                        delete_char_at(head->data, i  );    /* ( */
                        len -= 3;
                        j   -= 2;
                        if((var = __word_expand(head, i, j, len, '(', in_double_quotes)))
                            __word_expand2(&head, var, &i, j, &len);
                    }
                }
                else
                {
                    j = i;
                    if((c >= '0' && c <= '9') ||
                        c == '@' || c == '*'  || c == '#' ||
                        c == '!' || c == '?'  || c == '$' ||
                        c == '-' || c == '<')
                    {
                        j++;
                        /*
                         * ksh-extension. $#@ and $#* both give the same
                         * result as $#.
                         */
                        char c2 = head->data[i+2];
                        if(c == '#' && (c2 == '@' || c2 == '*'))
                        {
                            delete_char_at(head->data, i+2);
                            len--;
                        }
                    }
                    else
                    {
                        if(!isalpha(head->data[i+1]) && head->data[i+1] != '_') { break; }
                        while(++j < len)
                        {
                            if(!isalnum(head->data[j]) && head->data[j] != '_') break;
                        }
                        j--;
                    }
                    delete_char_at(head->data, i);
                    len--;
                    if((var = __word_expand(head, i, j, len, '{', in_double_quotes)))
                        __word_expand2(&head, var, &i, j, &len);
                }
                break;                
        }
    } while(++i < len);
    /* NOTE: terrible hack. we may have a hanging ending double quote. remove it */
    if(in_double_quotes && i && head->data[len-1] == '"' && strip_quotes)
    {
        head->data[len-1] = head->data[len];
        len--;
    }
    head->len = len;
    if(len == 0 && !has_quotes) return NULL;
    return res;
}

struct cmd_token *word_expand(struct cmd_token *head, struct cmd_token **tail, int in_heredoc, int strip_quotes)
{
    int count = 0, i;
    char **list = brace_expand(head->data, &count);
    /* if no braces expanded, go directly to word expansion */
    if(!list) return word_expand_one_word(head, tail, in_heredoc, strip_quotes);
    /* expand the braces and do word expansion on each resultant field */
    struct cmd_token *wordlist = NULL, *listtail = NULL;
    for(i = 0; i < count; i++)
    {
        struct cmd_token *t = make_cmd_token(list[i]);
        struct cmd_token *w = word_expand_one_word(t, tail, in_heredoc, strip_quotes);
        if(w)
        {
            if(wordlist)
            {
                listtail->next = w;
                listtail = w;
            }
            else
            {
                wordlist = w;
                listtail = w;
            }
        }
        else free_all_tokens(t);
    }
    /* free used memory */
    for(i = 0; i < count; i++) free_malloced_str(list[i]);
    free(list);
    *tail = listtail;
    return wordlist;
}

/*
 * A simple shortcut to perform word-expand on a string,
 * returning the result as a string.
 */
char *word_expand_to_str(char *word)
{
    struct cmd_token *tok = make_cmd_token(word);
    struct cmd_token *tail;
    struct cmd_token *t = word_expand(tok, &tail, 0, 1);
    if(!t)
    {
        free_all_tokens(tok);
        return NULL;
    }
    char *res = __tok_to_str(t);
    free_all_tokens(t);
    return res;
}


static inline int is_IFS_char(char c, char *IFS)
{
    if(!*IFS) return 0;
    do
    {
        if(c == *IFS) return 1;
    } while(*++IFS);
    return 0;
}

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

void skip_IFS_delim(char *str, char *IFS_space, char *IFS_delim, size_t *_i, size_t len)
{
    size_t i = *_i;
    while((i < len) && is_IFS_char(str[i], IFS_space)) i++;
    while((i < len) && is_IFS_char(str[i], IFS_delim)) i++;
    while((i < len) && is_IFS_char(str[i], IFS_space)) i++;
    *_i = i;
}

struct cmd_token *__make_fields(char *str)
{
    struct symtab_entry_s *entry = get_symtab_entry("IFS");
    char *IFS = entry ? entry->val : NULL;
    /* POSIX says no IFS means: "space/tab/NL" */
    if(!IFS) IFS = " \t\n";
    /* no field splitting */
    if(IFS[0] == '\0') return (struct cmd_token *)0;
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
            if(*p == ' ' || *p == '\t' || *p == '\n') *sp++ = *p++;
            else *dp++ = *p++;
        } while(*p);
        *sp = '\0';
        *dp = '\0';
    }

    size_t len    = strlen(str);
    size_t i      = 0, j = 0, k;
    int    fields = 1;
    char   quote  = 0;
    skip_IFS_whitespace(&str, IFS_space);
    /* estimate the needed number of fields */
    do
    {
        if(str[i] == '\'' || str[i] == '"' || str[i] == '`')
        {
            if(quote == str[i]) quote = 0;
            else                quote = str[i];
            continue;
        }
        if(quote) continue;
        if(is_IFS_char(str[i], IFS_space) || is_IFS_char(str[i], IFS_delim))
        {
            skip_IFS_delim(str, IFS_space, IFS_delim, &i, len);
            if(i < len) fields++;
        }
    } while(++i < len);
    if(fields == 1) return (struct cmd_token *)0;
    struct cmd_token *first_field = (struct cmd_token *)0;
    struct cmd_token *cur         = (struct cmd_token *)0;
    /* create fields */
    i     = 0;
    j     = 0;
    quote = 0;
    do
    {
        if(str[i] == '\'')
        {
            char *p = str+i+1;
            while(*p && *p != '\'') p++;
            i = p-str;
            continue;
        }
        else if(str[i] == '"' || str[i] == '`')
        {
            if(quote == str[i]) quote = 0;
            else                quote = str[i];
            continue;
        }
        
        if(quote) continue;

        if(is_IFS_char(str[i], IFS_space) ||
           is_IFS_char(str[i], IFS_delim) || (i == len))
        {
            char *tmp = (char *)malloc(i-j+1);
            /* TODO: do something better than bailing out here */
            if(!tmp)
            {
                BACKEND_RAISE_ERROR(INSUFFICIENT_MEMORY, "making fields", NULL);
                return first_field;
            }
            strncpy(tmp, str+j, i-j);
            tmp[i-j] = '\0';
            struct cmd_token *fld = (struct cmd_token *)malloc(sizeof(struct cmd_token));
            /* TODO: do something better than bailing out here */
            if(!fld)
            {
                free(tmp);
                return first_field;
            }
            fld->data = tmp;
            fld->len  = i-j;
            fld->next = 0;
            if(!first_field) first_field = fld;
            if(!cur) cur  = fld;
            else
            {
                cur->next = fld;
                cur       = fld;
            }
            k = i;
            skip_IFS_delim(str, IFS_space, IFS_delim, &i, len);
            j = i;
            if(i != k) i--;     /* go back one step so the loop will work correctly */
        }
    } while(++i <= len);
    return first_field;
}


void delete_char_at(char *str, size_t index)
{
    char *p1 = str+index;
    char *p2 = p1+1;
    while((*p1++ = *p2++)) ;
}

/* this function splits the original word, then inserts the newly expanded
 * field (or fields) between the split parts. For example, the following
 * expansion:
 *    spl`ls`it
 * should result in the following tokens:
 *    sp + first file name
 *    ... rest of files names
 *    last file name + it
 * 
 * this is not me screwing with your mind. this is POSIX!
 */
struct cmd_token *make_head_tail_tokens(struct cmd_token *tok, struct cmd_token *fld, 
                                        size_t len, size_t i, size_t j)
{
    //if(!tok || !fld) return NULL;
    /* make tail token */
    struct cmd_token *lfld = fld;
    if(lfld)
    {
        while(lfld->next) lfld = lfld->next;
    }
    char *tmp;
    /* if we are at the end of the original text, nothing to do. */
    if(j > len)
    {
        tmp = (char *)malloc((len-j)+strlen(lfld->data)+1);
        if(!tmp) return tok;
        strcpy(tmp, tok->data+j+1);
        strcat(tmp, lfld->data);
        free(lfld->data);
        lfld->data = tmp;
        lfld->len  = strlen(tmp);
        lfld->next = NULL;
    }
    /* make head token */
    if(i)
    {
        tmp = (char *)malloc(i+strlen(fld->data));
        if(!tmp) return tok;
        strncpy(tmp, tok->data, i);
        tmp[i] = '\0';
        strcat(tmp, fld->data);
        free(tok->data);
        tok->data = tmp;
        tok->len  = strlen(tmp);
        tok->next = fld->next;
        free(fld->data);
    }
    else
    {
        if(tok->data) free(tok->data);
        tok->data = fld->data;
        tok->len  = fld->len;
    }
    //free(fld);
    return lfld;
}


void purge_tokens(struct cmd_token *tok)
{
    int i = 0;
    while(tok)
    {
        tok = tok->next;
        i++;
    }
    __asm__ __volatile__("xchg %%bx, %%bx":::);
}
