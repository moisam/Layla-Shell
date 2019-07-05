/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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

// char **pos_params       = NULL;
// int    pos_params_count = 0;
// char  *null_param       = "";
int    exit_status      = 0;
// struct cmd_token null_param_token;
int    subshell_level   = 0;

/* defined in builtins/newgrp.c */
int get_supp_groups(char *name, gid_t gid, gid_t **_supp_groups, int *_n);


struct symtab_entry_s *get_pos_param(int i)
{
    char buf[16];
    sprintf(buf, "%d", i);
    return get_symtab_entry(buf);
}

int is_pos_param(char *var_name)
{
    if(var_name[1] == '\0')
    {
        if(var_name[0] >= '0' && var_name[0] <= '9') return 1;
        return 0;
    }
    char *p = var_name;
    while(*p >= '0' && *p <= '9') p++;
    if(*p == '\0') return 1;
    return 0;
}

int is_special_param(char *var_name)
{
    if(var_name[1] == '\0')
    {
        if(var_name[0] == '#' || var_name[0] == '?' || var_name[0] == '-' ||
           var_name[0] == '$' || var_name[0] == '!' || var_name[0] == '@' ||
           var_name[0] == '*') return 1;
    }
    return 0;
}

int pos_param_count()
{
    struct symtab_entry_s *entry = get_symtab_entry("#");
    if(!entry || !entry->val) return -1;
    return atoi(entry->val);
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
struct cmd_token *get_all_pos_params(char which, int quoted)
{
    struct symtab_entry_s *entry = get_symtab_entry("#");
    if(!entry || !entry->val) return NULL;
    if(entry->val[0] == '0' ) return NULL;
    int pos_params_count = atoi(entry->val);
    return get_pos_params(which, quoted, 1, pos_params_count);
}

struct cmd_token *get_pos_params(char which, int quoted, int offset, int count)
{
    char   separator  = ' ';
    char *IFS = get_shell_varp("IFS", NULL);
    if(IFS) separator = IFS[0];
    size_t len        = 0;
    int    i          = offset;
    int    last       = i+count;
    int    j          = pos_param_count()+1;
    if(last > j) last = j;
    struct cmd_token *param       = NULL;
    struct cmd_token *first_param = NULL;

    /* with field separation */
    if((which == '@') || (which == '*' && !quoted))
    {
        if(!IFS || *IFS)
        {
            i = offset;
            while(i < last)
            {
                struct cmd_token *param2 = (struct cmd_token *)malloc(sizeof(struct cmd_token));
                /* TODO: we should free the so-far allocated parameters */
                if(!param2) goto memerror;

                sprintf(buf, "%d", i);
                char *p = get_shell_varp(buf, "");
                size_t len2 = strlen(p);
                param2->data = __get_malloced_str(p);
                param2->next = NULL;
                param2->len  = len2;
                if(param) param->next = param2;
                param = param2;
                if(!first_param) first_param = param;
                i++;
            }
            return first_param;
        }
    }

    /* without field separation */
    i = offset;
    while(i < last)
    {
        sprintf(buf, "%d", i);
        size_t len2 = strlen(get_shell_varp(buf, ""));
        len += len2+1; /* 1 for the separator */
        i++;
    }
    param = (struct cmd_token *)malloc(sizeof(struct cmd_token));
    if(!param) goto memerror;
    char *params = (char *)malloc(len+1);
    if(!params) { free(param); goto memerror; }
    param->data = params;
    param->next = NULL;
    
    char *p1 = params;
    i = offset;
    while(i < last)
    {
        sprintf(buf, "%d", i);
        char *p2 = get_shell_varp(buf, "");
        len = strlen(p2);
        while((*p1++ = *p2++)) ;
        /* as per POSIX, if IFS first char is NULL, concatenate params */
        if(!separator) p1--;
        else           p1[-1] = separator;
        i++;
    }
    *p1 = '\0';
    if(separator) p1[-1] = '\0';
    param->len  = strlen(param->data);
    return param;

memerror:
    fprintf(stderr, "%s: insufficient memory to load positional parameters\r\n", SHELL_NAME);
    return NULL;
}

/*
 * save positional parameters. return array is NULL-terminated,
 * just like **argv.
 */
char **get_pos_paramsp()
{
    struct symtab_entry_s *entry = get_symtab_entry("#");
    if(!entry || !entry->val) return NULL;
    int     count = atoi(entry->val)+1;
    size_t  len = (count)*sizeof(char *);
    int     i, j;
    char   **p = malloc(len);
    if(!p) return NULL;
    memset(p, 0, len);
    for(i = 1, j = 0; i <= count; i++, j++)
    {
        sprintf(buf, "%d", i);
        entry = get_symtab_entry(buf);
        if(entry && entry->val)
        {
            p[j] = malloc(strlen(entry->val)+1);
            if(p[j]) strcpy(p[j], entry->val);
        }
    }
    return p;
}

/*
 * restore positional parameters from a saved array that is 
 * NULL-terminated.
 */
void set_pos_paramsp(char **p)
{
    if(!p) return;
    int     count, oldcount = 0;
    int     i, j;
    /* get the count */
    for(i = 0; p[i]; i++) ;
    count = i;
    struct symtab_entry_s *entry = add_to_symtab("#");
    if(!entry) return;
    if(entry) oldcount = atoi(entry->val);
    sprintf(buf, "%d", count);
    symtab_entry_setval(entry, buf);
    for(i = 1, j = 0; i <= count; i++, j++)
    {
        sprintf(buf, "%d", i);
        entry = add_to_symtab(buf);
        if(!entry) continue;
        symtab_entry_setval(entry, p[j]);
    }
    /* unset the rest of old params */
    for( ; i <= oldcount; i++)
    {
        sprintf(buf, "%d", i);
        entry = get_symtab_entry(buf);
        if(!entry) continue;
        symtab_entry_setval(entry, NULL);
    }
}


/*
 * called on shell initialization. the first two params are the current
 * user's name and gid, the 3rd param is the shell's fullpath or argv[0].
 */
void init_shell_vars(char *pw_name, gid_t pw_gid, char *fullpath)
{
    struct symtab_entry_s *entry;
    char buf[12];
    /* $? is the exit status of the most recent pipeline */
    entry = add_to_symtab("?");
    symtab_entry_setval(entry, "0");
    
    /* $$ is the decimal process ID of the shell */
    _itoa(buf, tty_pid);
    entry = add_to_symtab("$");
    symtab_entry_setval(entry, buf);
  
    /* $! is the decimal process ID of the most recent background command */
    entry = add_to_symtab("!");
    symtab_entry_setval(entry, "0");
  
    entry = add_to_symtab("OPTIND");
    symtab_entry_setval(entry, "1");
  
    /* set the maximum size of the history list */
    entry = get_symtab_entry("HISTSIZE");
    long l = (entry && entry->val) ? atol(entry->val) : 0;
    if(!l)
    {
        l = default_HISTSIZE;
        sprintf(buf, "%ld", l);
        symtab_entry_setval(entry, buf);
    }
    /* set the maximum size of the history file */
    entry = add_to_symtab("HISTFILESIZE");
    sprintf(buf, "%ld", l);
    symtab_entry_setval(entry, buf);
  
    subshell_level = get_shell_vari("SUBSHELL", 0);
    if(subshell_level == 0)
    {
        /* first function name */
        entry = add_to_symtab("FUNCNAME");
        symtab_entry_setval(entry, "main");
        entry->flags = FLAG_READONLY;
  
        /* incremented for each subshell invocation -- similar to $BASH_SUBSHELL */
        entry = add_to_symtab("SUBSHELL");
        symtab_entry_setval(entry, "0");
        entry->flags = FLAG_READONLY;
    }
    
    /*
     * $_ (or underscore) is an obscure variable that shells love to assign 
     * different values to. it starts with being the shell's name as passed in 
     * the environment. then it becomes the last argument of the last command 
     * executed. sometimes it is assigned the absolute pathname of the command 
     * and passed to the command in its environment. it also gets assigned the 
     * value of the matching MAIL file when cheking the mail. csh assigns it 
     * the command line of the last command executed. got enough yet? :)
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
        if(supp_groups) free(supp_groups);
    }
    
    /* make $SHELLOPTS readonly (bash) */
    entry = get_symtab_entry("SHELLOPTS");
    if(entry) entry->flags |= FLAG_READONLY;
    
    /*
     * increment the shell nesting level (bash and tcsh).
     * tcsh resets it to 1 in login shells.
     */
    entry = add_to_symtab("SHLVL");
    sprintf(buf, "%d", subshell_level);
    symtab_entry_setval(entry, buf);
    
    /*
    entry = get_symtab_entry("SHLVL");
    n = 0;
    if(!option_set('L') && entry)
    {
        char *strend = NULL;
        n = entry->val ? strtol(entry->val, &strend, 10) : 0;
        if(strend == entry->val) n = 0;
    }
    else entry = add_to_symtab("SHLVL");
    sprintf(buf, "%d", n+1);
    symtab_entry_setval(entry, buf);
    */
    
    /* bash doesn't mark $SHLVL as readonly. but better safe than sorry */
    if(entry) entry->flags |= FLAG_READONLY;
    
    /* tcsh has a 'tty' variable naming the controlling terminal */
    if(isatty(0))
    {
        char *s = ttyname(0);
        entry = add_to_symtab("TTY");
        symtab_entry_setval(entry, s);
    }
}
