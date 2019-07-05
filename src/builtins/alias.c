/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: alias.c
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../cmd.h"
#include "../scanner/scanner.h"     /* is_keyword() */
#include "../parser/parser.h"       /* __parse_alias() */
#include "../debug.h"

struct  alias_s __aliases[MAX_ALIASES];
char    *null_alias = "";

#define UTILITY     "alias"

int set_alias(char *name, char *val);


void init_aliases()
{
    /* colorize the ls output */
    set_alias("ls", "ls --color=auto");
    /* use long listing format */
    set_alias("ll", "ls -la");
    /* show hidden files */
    set_alias("l.", "ls -d .* --color=auto");
    /* some cd aliases */
    set_alias("cd..", "cd ..");
    set_alias("..", "cd ..");
    set_alias("...", "cd ../../../");
    /* some grep aliases */
    set_alias("grep", "grep --color=auto");
    set_alias("egrep", "egrep --color=auto");
    set_alias("fgrep", "fgrep --color=auto");
    /* start the calcultator with math support */
    set_alias("bc", "bc -l");
    /* vi */
    set_alias("vi", "vim");
    /* some ksh-like aliases */
    set_alias("command", "command ");
    set_alias("nohup", "nohup ");
    set_alias("stop", "kill -s STOP");
    set_alias("suspend", "kill -s STOP $$");
    /* some bash-like aliases */
    set_alias("which", "(alias; declare -f) | /usr/bin/which --tty-only --read-alias --read-functions --show-tilde --show-dot");
    /* alias ksh's hist to our fc */
    set_alias("hist", "fc");
    /* alias ksh's typeset to our bash-like declare */
    set_alias("typeset", "declare");
    /* alias bash's shopt to our setx */
    set_alias("shopt", "setx");
    /* alias tcsh's builtins to our builtin */
    set_alias("builtins", "builtin");
    /* alias tcsh's where to our whence */
    set_alias("where", "whence -a");
    /* in tcsh, some builtins are synonyms for other builtins */
    set_alias("bye", "logout");
    set_alias("chdir", "cd");
    /* in tcsh, rehash doesn't do exactly what we do here, but the function is similar */
    set_alias("rehash", "hash -a");
    /* mimic the work of tcsh's unhash builtin */
    set_alias("unhash", "set +h");
    /* uncomment the following line if you want to remove source.c */
    //set_alias("source", "command .");
    /* some other useful aliases */
    set_alias("reboot", "sudo /sbin/reboot");
    set_alias("poweroff", "sudo /sbin/poweroff");
    set_alias("halt", "sudo /sbin/halt");
    set_alias("shutdown", "sudo /sbin/shutdown");
    //set_alias("df", "df -H");
    //set_alias("du", "du -ch");
    set_alias("r", "fc -s");    /* to quickly re-execute history commands */
    set_alias("memuse", "memusage");
}


int print_alias(char *name)
{
    int i;
    size_t len = 0;
    if(name) len = strlen(name);
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(!__aliases[i].name) continue;
        if(name)
        {
            if(strlen(__aliases[i].name) != len) continue;
            if(strcmp(__aliases[i].name, name) != 0) continue;
        }
        printf("alias %s=", __aliases[i].name);
        /* NOTE: POSIX says to use appropriate quoting, suitable for reinput 
         *       to the shell.
         */
        if(__aliases[i].val) purge_quoted_val(__aliases[i].val);
        printf("\r\n");
        if(name) return 0;
    }
    if(name)
    {
        fprintf(stderr, "%s: alias '%s' is not defined\r\n", UTILITY, name);
        return 1;
    }
    return 0;
}

int set_alias(char *name, char *val)
{
    if(!name || !val) return 1;
    int i, index = -1, first_free = -1;
    size_t len = strlen(name);
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(!__aliases[i].name)
        {
            if(first_free == -1) first_free = i;
            continue;
        }
        if(strlen(__aliases[i].name) != len) continue;
        if(strcmp(__aliases[i].name, name) == 0)
        {
            index = i; break;
        }
    }
    if(index == -1)
    {
        if(first_free == -1)
        {
            fprintf(stderr, "%s: couldn't set alias '%s': full buffers\r\n", UTILITY, name);
            return 1;
        }
        index = first_free;
    }
    i = index;
    if(!__aliases[i].name)
    {
        __aliases[i].name = (char *)malloc(len+1);
        if(!__aliases[i].name) goto memerr;
        strcpy(__aliases[i].name, name);
    }
    if(__aliases[i].val) free(__aliases[i].val);
    __aliases[i].val = (char *)malloc(strlen(val)+1);
    if(!__aliases[i].val) goto memerr;
    strcpy(__aliases[i].val, val);
    return 0;
    
memerr:
    fprintf(stderr, "%s: couldn't set alias '%s': insufficient memory\r\n", UTILITY, name);
    return 1;
}


int valid_alias_name(char *name)
{
    char *p = name;
    if(!*p) return 0;
    while(*p)
    {
        if(isalnum(*p) || *p == '_' || *p == '!' || *p == '%' || *p == ',' || *p == '@') p++;
        else return 0;
    }
    return 1;
}


/*
 * run the given alias as a command. used to implement tcsh's "Special Aliases"
 * functionality, where special aliases get processed and executed as commands
 * under specific conditions. only cd does its own thing by processing its alias
 * command (cwdcmd), as it needs to handle some gray cases.
 */

void run_alias_cmd(char *alias)
{
    char *cmd = __parse_alias(alias);
    if(cmd != alias && cmd != null_alias)
    {
        cmd = word_expand_to_str(cmd);
        if(cmd)
        {
            eval(2, (char *[]){ "eval", cmd, NULL });
            free(cmd);
        }
    }
}


/*
 * the alias builtin utility.
 */

int alias(int argc, char *argv[])
{
    int  index = 0;
    char *str  = NULL;
    char *eq;
    int  res   = 0, res3;
    int  i;

    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvp", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_ALIAS, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                print_alias(NULL);
                return 0;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        print_alias(NULL);
        return 0;
    }

    for(index = v; index < argc; index++)
    {
        str = argv[index];
        eq = strchr(str, '=');
        if(!eq)
        {
            int res2 = print_alias(str);
            /* if error return error */
            if(!res && res2) res = res2;
            continue;
        }
        i = eq-str;
        char tmp[i+1];
        strncpy(tmp, str, i);
        tmp[i] = '\0';
        /*
         * don't allow aliasing for shell keywords. tcsh also doesn't allow aliasing
         * the words 'alias' and 'unalias'.
         */
        if(is_keyword(tmp) >= 0 || strcmp(tmp, "alias") == 0 || strcmp(tmp, "unalias") == 0)
        {
            fprintf(stderr, "%s: cannot alias shell keyword: %s\r\n", UTILITY, tmp);
            res = 2;
            continue;
        }
        res3 = set_alias(tmp, str+i+1);
        /* if error return error */
        if(!res && res3) res = res3;
    }
    return res;
}
