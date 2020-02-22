/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: params.c
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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "cmd.h"
#include "symtab/symtab.h"
#include "debug.h"

/* the exit status of the last command executed */
int exit_status = 0;

/*
 * the current subshell level (how many subshells have we started in tandem).
 * incremented every time the shell forks a subshell.
 */
int executing_subshell = 0;

/*
 * the current shell level (how many times has lsh been invoked).
 * incremented on shell startup.
 */
int shell_level = 0;;

/* defined in builtins/newgrp.c */
int get_supp_groups(char *name, gid_t gid, gid_t **_supp_groups, int *_n);


/*
 * set the exit status of the last command executed in both the global
 * exit_status variable and the $? shell variable.
 * this function examines the status argument to extract the actual
 * exit status.. if the flag is 0, it sets the exit status to status,
 * otherwise it uses the <wait.h> macros to get the status.
 */
void set_exit_status(int status)
{
    /* we need to extract the exit status code */
    if(WIFEXITED(status))
    {
        status = WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status))
    {
        status = WTERMSIG(status) + 128;
    }
    else if(WIFSTOPPED(status))
    {
        status = WSTOPSIG(status) + 128;
    }
    set_internal_exit_status(status & 0xff);
}


/*
 * set the exit status of the last command executed in both the global
 * exit_status variable and the $? shell variable.
 * this function is used by the shell builtins and functions to set
 * the exit status, without applying the macros from <wait.h>.
 */
void set_internal_exit_status(int status)
{
    char status_str[16];
    sprintf(status_str, "%u", status);
    struct symtab_entry_s *entry = get_symtab_entry("?");
    if(entry)
    {
        symtab_entry_setval(entry, status_str);
    }
    exit_status = status;
}


/*
 * reset the positional parameters by setting the value of each parameter to
 * NULL, followed by setting the value of $# to zero.
 */
void reset_pos_params(void)
{
    /* unset all positional params */
    struct symtab_entry_s *hash = get_symtab_entry("#");
    if(hash && hash->val)
    {
        int params = atoi(hash->val);
        if(params != 0)
        {
            int  j = 1;
            char buf[32];
            for( ; j <= params; j++)
            {
                sprintf(buf, "%d", j);
                struct symtab_entry_s *entry = get_symtab_entry(buf);
                if(entry && entry->val)
                {
                    symtab_entry_setval(entry, NULL);
                }
            }
        }
        symtab_entry_setval(hash, "0");
    }
}


/*
 * return the symbol table entry for positional parameter i,
 * which is the value of shell variable $i.
 */
struct symtab_entry_s *get_pos_param(int i)
{
    char buf[16];
    sprintf(buf, "%d", i);
    return get_symtab_entry(buf);
}


/*
 * return 1 if name is a valid positional parameter name, 0 otherwise.
 * this function only checks the validity of the name, it does not check if
 * the positional parameter is actually set or not.
 */
int is_pos_param(char *name)
{
    /*
     * one-digit positional parameters, $0-$9 (although $0 is technically
     * a special parameter, not a positional parameter).
     */
    if(name[1] == '\0')
    {
        if(name[0] >= '0' && name[0] <= '9')
        {
            return 1;
        }
        return 0;
    }
    /* multi-digit positional parameter names must consist solely of digits */
    char *p = name;
    while(*p >= '0' && *p <= '9')
    {
        p++;
    }
    if(*p == '\0')
    {
        return 1;
    }
    return 0;
}


/*
 * return 1 if name is a valid special parameter name, 0 otherwise.
 * this function only checks the validity of the name, it does not check if
 * the special parameter is actually set or not.
 */
int is_special_param(char *name)
{
    switch(name[0])
    {
        case '#':
        case '?':
        case '-':
        case '$':
        case '!':
        case '@':
        case '*':
            /* all special parameters have one-letter names */
            if(name[1] == '\0')
            {
                return 1;
            }
            __attribute__((fallthrough));

        default:
            return 0;
    }
}


/*
 * return the positional parameter count, which we get from the shell variable $#.
 */
int pos_param_count(void)
{
    return get_shell_vari("#", -1);
}

char  buf[32];

/*
    Excerpt from POSIX:
$@
    Expands to the positional parameters, starting from one. When the 
    expansion occurs within double-quotes, and where field splitting is 
    performed, each positional parameter shall expand as a separate field, 
    with the provision that the expansion of the first parameter shall 
    still be joined with the beginning part of the original word (assuming 
    that the expanded parameter was embedded within a word), and the 
    expansion of the last parameter shall still be joined with the last 
    part of the original word. If there are no positional parameters, the 
    expansion of '@' shall generate zero fields, even when '@' is 
    double-quoted.
$*
    Expands to the positional parameters, starting from one. When the 
    expansion occurs within a double-quoted string, it shall expand to a 
    single field with the value of each parameter separated by the first 
    character of the IFS variable, or by a <space> if IFS is unset. If IFS 
    is set to a null string, this is not equivalent to unsetting it; its 
    first character does not exist, so the parameter values are concatenated.
*/

/*
 * return the values of all positional parameters, NULL if there is none.
 */
char *get_all_pos_params_str(char which, int quoted)
{
    /* get the count of positional parameters */
    int pos_params_count = get_shell_varl("#", 0);
    if(pos_params_count <= 0)
    {
        return NULL;
    }
    return get_pos_params_str(which, quoted, 1, pos_params_count);
}


/*
 * return the values of positional parameters starting from parameter 'offset' and counting
 * 'count' parameters.. the which parameter tells whether we want to access the parameters as
 * the $@ or the $* special parameter, which affects the number of fields we get (see the
 * POSIX excerpt comment above).. the quoted parameter indicates whether we should expand
 * the values as if we are doing it inside double quotes.
 */
char *get_pos_params_str(char which, int quoted, int offset, int count)
{
    char   separator  = ' ';
    char *IFS = get_shell_varp("IFS", NULL);

    /* if $IFS is unset, separate using ' ' */
    if(IFS)
    {
        separator = *IFS;
    }

    size_t len    = 0;
    int    i      = offset;
    int    last   = i+count;
    int    j      = pos_param_count()+1;
    char  *params = NULL, *p1 = NULL;

    if(last > j)
    {
        last = j;
    }

    if(which == '*' || (which == '@' && !quoted))
    {
        /*
         * $* expands each param to a separate word, while "$*" expands to the params
         * separated by the first $IFS char, i.e. "$1c$2c$3c...".
         */
        if(!quoted)
        {
            separator = ' ';
        }
        quoted = 0;
    }
    else
    {
        /*
         * "$@" expands to "$1" "$2" "$3"...
         * we'll expand "$@" to $1" "$2" "$3, i.e. omitting the very first and
         * very last quotes, as we will use the quote chars from the original word.
         */
        separator = '\0';
        quoted = 1;
    }

    /* get the total length */
    i = offset;
    while(i < last)
    {
        sprintf(buf, "%d", i);
        size_t len2 = strlen(get_shell_varp(buf, ""));
        if(quoted)
        {
            len2 += 2;          /* 2 for the quotes */
        }
        len += len2+1; /* 1 for the separator */
        i++;
    }

    /* allocate memory */
    params = malloc(len+1);
    if(!params)
    {
        return NULL;
    }

    p1 = params;
    i = offset;
    while(i < last)
    {
        sprintf(buf, "%d", i);
        char *p2 = get_shell_varp(buf, "");
        len = strlen(p2);
        while((*p1++ = *p2++))
        {
            ;
        }
        p1--;

        /* for every param except the last ... */
        if(i < last-1)
        {
            /* ... add " " after the param value if being quoted */
            if(quoted)
            {
                *p1++ = '"';
                *p1++ = ' ';
                *p1++ = '"';
            }
            /* ... or if not quoted, concatenate params if first IFS char is null */
            else if(separator)
            {
                *p1++ = separator;
            }
        }
        i++;
    }
    *p1 = '\0';
    return params;
}


/*
 * set the values of positional parameters $1 to $count.. if the new paramer
 * count is less than the old parameter count, positional parameters $count+1
 * to $oldcount are set to NULL.. we set all of these parameters in the local
 * symbol table, so that when a dot script or shell function returns, we pop
 * the local symbol table off the stack, and those parameters resume the values
 * they had before we entered the script/function.
 */
void set_local_pos_params(int count, char **params)
{
    int  i;
    char buf[32];
    struct symtab_entry_s *entry;
    struct symtab_s *st = get_local_symtab();

    /* sanity check */
    if(!count || !params || !*params)
    {
        return;
    }

    /* set arguments $1...$count */
    for(i = 1; i <= count; i++)
    {
        sprintf(buf, "%d", i);
        entry = add_to_any_symtab(buf, st);
        if(entry)
        {
            symtab_entry_setval(entry, params[i-1]);
            entry->flags = FLAG_LOCAL | FLAG_READONLY;
        }
    }
    
    /*
     * overshadow the rest of the parameters, if any, so that the user cannot
     * access them behind our back.
     */
    entry = get_symtab_entry("#");
    if(entry && entry->val)
    {
        int old_count = atoi(entry->val);
        if(old_count > 0)
        {
            for( ; i <= old_count; i++)
            {
                sprintf(buf, "%d", i);
                entry = add_to_any_symtab(buf, st);
                if(entry)
                {
                    entry->flags = FLAG_LOCAL | FLAG_READONLY;
                }
            }
        }
    }
    
    /* set our new param $# */
    entry = add_to_any_symtab("#", st);
    if(entry)
    {
        sprintf(buf, "%d", count);
        symtab_entry_setval(entry, buf);
        entry->flags = FLAG_LOCAL | FLAG_READONLY;
    }
}


/*
 * initialize some of the shell variables to preset default values.
 * called on shell initialization. the first two params are the current
 * user's name and gid, the 3rd param is the shell's fullpath or argv[0].
 */
void init_shell_vars(char *pw_name, gid_t pw_gid, char *fullpath)
{
    struct symtab_entry_s *entry;
    char buf[12];
    char *strend;
    /* $? is the exit status of the most recent pipeline */
    entry = add_to_symtab("?");
    symtab_entry_setval(entry, "0");
    
    /* $$ is the decimal process ID of the shell */
    sprintf(buf, "%u", shell_pid);
    entry = add_to_symtab("$");
    symtab_entry_setval(entry, buf);
  
    /* $! is the decimal process ID of the most recent background command */
    entry = add_to_symtab("!");
    symtab_entry_setval(entry, "0");
  
    entry = add_to_symtab("OPTIND");
    symtab_entry_setval(entry, "1");
  
    /* set the maximum size of the history list */
    entry = get_symtab_entry("HISTSIZE");
    long l = (entry->val) ? strtol(entry->val, &strend, 10) : 0;
    if(!l || *strend)
    {
        l = default_HISTSIZE;
        sprintf(buf, "%ld", l);
        symtab_entry_setval(entry, buf);
    }

    /* set the maximum size of the history file */
    entry = add_to_symtab("HISTFILESIZE");
    sprintf(buf, "%ld", l);
    symtab_entry_setval(entry, buf);

    /*
     * increment the shell nesting level with each shell invocation (bash/tcsh).
     * tcsh also resets $SHLVL it to 1 in login shells.
     */
    entry = add_to_symtab("SHLVL");
    l = (entry->val) ? strtol(entry->val, &strend, 10) : 0;
    if(option_set('L') || l < 0 || *strend)    /* login shell or invalid value */
    {
        l = 0;
    }
    sprintf(buf, "%ld", ++l);
    symtab_entry_setval(entry, buf);
    entry->flags = FLAG_READONLY | FLAG_EXPORT;
    shell_level = l;

    /* incremented for each subshell invocation (similar to $BASH_SUBSHELL) */
    entry = add_to_symtab("SUBSHELL");
    l = (entry->val) ? strtol(entry->val, &strend, 10) : 0;
    if(l < 0 || *strend)    /* invalid value */
    {
        l = 0;
    }
    sprintf(buf, "%ld", l);
    symtab_entry_setval(entry, buf);
    entry->flags = FLAG_READONLY | FLAG_EXPORT;
    executing_subshell = l;

    entry = add_to_symtab("FUNCNAME");
    if(!entry->val)
    {
        /* first function name */
        symtab_entry_setval(entry, "main");
    }
    entry->flags = FLAG_READONLY | FLAG_EXPORT;
    
    /*
     * $_ (or underscore) is an obscure variable that shells love to assign 
     * different values to. it starts with being the shell's name as passed in 
     * the environment. then it becomes the last argument of the last command 
     * executed. sometimes it is assigned the absolute pathname of the command 
     * and passed to the command in its environment. it also gets assigned the 
     * value of the matching MAIL file when cheking the mail. csh assigns it 
     * the command line of the last command executed. had enough yet? :)
     */
    entry = add_to_symtab("_");
    symtab_entry_setval(entry, fullpath);
    
    /* group list */
    int   n;
    gid_t *supp_groups;
    if(get_supp_groups(pw_name, pw_gid, &supp_groups, &n))
    {
        char buf2[(n*(sizeof(gid_t)+1))+1];
        buf2[0] = '\0';
        while(n--)
        {
            sprintf(buf, "%u ", supp_groups[n]);
            strcat(buf2, buf);
        }
        /* in bash, $GROUPS is an array var. ours is simple */
        entry = add_to_symtab("GROUPS");
        symtab_entry_setval(entry, buf2);
        entry->flags = FLAG_READONLY;
        if(supp_groups)
        {
            free(supp_groups);
        }
    }
    
    /* make $SHELLOPTS readonly (bash) */
    entry = get_symtab_entry("SHELLOPTS");
    if(entry)
    {
        entry->flags |= FLAG_READONLY;
    }
    
    /* tcsh has a 'tty' variable naming the controlling terminal */
    l = cur_tty_fd();
    if(l >= 0)
    {
        entry = add_to_symtab("TTY");
        symtab_entry_setval(entry, ttyname(l));
    }
    
    /* init our 'special' vars (see vars.c) */
    for(n = 0; n < special_var_count; n++)
    {
        entry = add_to_symtab(special_var_names[n]);
        entry->flags = FLAG_SPECIAL_VAR | FLAG_EXPORT;
    }
}
