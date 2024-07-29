/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "builtins.h"
#include "../include/cmd.h"
#include "../scanner/scanner.h"     /* is_keyword() */
#include "../parser/parser.h"       /* get_alias_val() */
#include "../include/debug.h"

struct  alias_s aliases[MAX_ALIASES];

#define UTILITY     "alias"

/* Defined below */
int set_alias(char *name, char *val);
int alias_list_index(char *alias);


/*
 * Initialize our predefined aliases. called on shell startup by an interactive shell.
 */
void init_aliases(void)
{
    /* Colorize the ls output */
    set_alias("ls", "ls --color=auto");
    
    /* Use long listing format */
    set_alias("ll", "ls -la");
    
    /* Show hidden files */
    set_alias("l.", "ls -d .* --color=auto");
    
    /* Some cd aliases */
    set_alias("cd..", "cd ..");
    set_alias("..", "cd ..");
    set_alias("...", "cd ../../../");
    
    /* Some grep aliases */
    set_alias("grep", "grep --color=auto");
    set_alias("egrep", "egrep --color=auto");
    set_alias("fgrep", "fgrep --color=auto");
    
    /* Start the calcultator with math support */
    set_alias("bc", "bc -l");
    
    /* Vi editor */
    set_alias("vi", "vim");
    
    /* Some ksh-like aliases */
    set_alias("command", "command ");
    set_alias("nohup", "nohup ");
    set_alias("stop", "kill -s STOP");
    set_alias("suspend", "kill -s STOP $$");
    
    /* Some bash-like aliases */
    set_alias("which", "(alias; declare -f) | /usr/bin/which --tty-only "
                       "--read-alias --read-functions --show-tilde --show-dot");
    
    /* Alias ksh's hist to our fc */
    set_alias("hist", "fc");
    
    /* Alias bash's shopt to our setx (we're mostly compatible!) */
    //set_alias("shopt", "setx");
    
    /* Alias tcsh's builtins to our builtin */
    set_alias("builtins", "builtin");
    
    /* Alias tcsh's where to our whence */
    set_alias("where", "whence -a");
    
    /* In tcsh, some builtins are synonyms for other builtins */
    set_alias("bye", "logout");
    set_alias("chdir", "cd");
    
    /* In tcsh, rehash doesn't do exactly what we do here, but the function is similar */
    set_alias("rehash", "hash -a");
    
    /* Mimic the work of tcsh's unhash builtin */
    set_alias("unhash", "set +h");
    
    /* Uncomment the following line if you want to remove source.c */
    //set_alias("source", "command .");
    
    /* Some other useful aliases */
    set_alias("reboot", "sudo /sbin/reboot");
    set_alias("poweroff", "sudo /sbin/poweroff");
    set_alias("halt", "sudo /sbin/halt");
    set_alias("shutdown", "sudo /sbin/shutdown");
    //set_alias("df", "df -H");
    //set_alias("du", "du -ch");
    set_alias("r", "fc -s");    /* to quickly re-execute history commands */
    set_alias("memuse", "memusage");
}


/*
 * Unset (remove) all aliases. Called by init_subshell() when we fork a
 * new subshell.
 */
void unset_all_aliases(void)
{
    int i = 0;
    for( ; i < MAX_ALIASES; i++)
    {
        if(aliases[i].name)
        {
            free(aliases[i].name);
            aliases[i].name = NULL;
        }

        if(aliases[i].val)
        {
            free(aliases[i].val);
            aliases[i].val = NULL;
        }
    }
}


/*
 * Print an alias definition in a form that can be reinput again to the shell to redefine
 * the same alias. If name is NULL, all defined aliases are printed.
 * 
 * Returns 1 if the alias with the given name is not defined, otherwise 0.
 */
void print_alias(char *name, char *val)
{
    /*
     * NOTE: POSIX says to use appropriate quoting, suitable for reinput 
     *       to the shell.
     */
    printf("alias %s", name[0] == '-' ? "-- " : "");

    if(val)
    {
        char *val2 = quote_val(val, 1, 0);
        if(val2)
        {
            printf("%s=%s\n", name, val2);
            free(val2);
        }
        else
        {
            printf("%s=%s\n", name, val);
        }
    }
    else
    {
        printf("%s\n", name);
    }
}


/*
 * Print the list of aliases passed in args. If args[0] is NULL, print all aliases.
 */
int print_alias_list(char **args)
{
    int  i, res = 0;
    char **p;

    if(*args)
    {
        for(p = args; *p; p++)
        {
            i = alias_list_index(*p);
            if(i >= 0)
            {
                print_alias(aliases[i].name, aliases[i].val);
            }
            else
            {
                PRINT_ERROR(UTILITY, "alias `%s` is not defined", *p);
                res = 1;
            }
        }
    }
    else
    {
        /* Search the alias list for the given name, or print all aliases if name is NULL */
        for(i = 0; i < MAX_ALIASES; i++)
        {
            if(aliases[i].name)
            {
                print_alias(aliases[i].name, aliases[i].val);
            }
        }
    }

    return res;
}


/*
 * Define an alias with the given name and assign it the given value. If an alias
 * with the given name is already defined, the old value is removed before saving the
 * new value.
 * 
 * Returns 0 if the alias was successfully defined, 1 if there is an error.
 */
int set_alias(char *name, char *val)
{
    if(!name || !val)
    {
        return 1;
    }
    
    int i, index = -1, first_free = -1;
    size_t len = strlen(name);

    /*
     * Search the alias list for a free spot, while checking if an alias with the
     * given name already exists.
     */
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(!aliases[i].name)
        {
            if(first_free == -1)
            {
                first_free = i;
            }
            continue;
        }
        
        if(strcmp(aliases[i].name, name) == 0)
        {
            index = i;
            break;
        }
    }

    /* Full list, we can't define a new alias */
    if(index == -1)
    {
        if(first_free == -1)
        {
            PRINT_ERROR(UTILITY, "couldn't set alias `%s`: full buffers", name);
            return 1;
        }
        index = first_free;
    }
    i = index;
    
    /* Save the alias name */
    if(!aliases[i].name)
    {
        aliases[i].name = malloc(len+1);
        if(!aliases[i].name)
        {
            goto memerr;
        }
        
        strcpy(aliases[i].name, name);
    }
    
    /* Remove the old value */
    if(aliases[i].val)
    {
        free(aliases[i].val);
    }
    
    /* Save the new value */
    aliases[i].val = malloc(strlen(val)+1);
    if(!aliases[i].val)
    {
        goto memerr;
    }
    
    strcpy(aliases[i].val, val);
    
    return 0;
    
memerr:
    PRINT_ERROR(UTILITY, "couldn't set alias `%s`: insufficient memory", name);
    return 1;
}


/*
 * Check if the given name is a valid alias name. This function doesn't check if the
 * alias is already defined or not, it just checks if the name is valid as an alias
 * name, as defined by POSIX (aliases can contain alphanumerics, underscores, and any
 * of these characters: "!%,@").
 * 
 * Returns 1 if the name is a valid alias name, 0 otherwise.
 */
int valid_alias_name(char *name)
{
    char *p = name;
    
    if(!*p)
    {
        return 0;
    }
    
    while(*p)
    {
        if(isalnum(*p) || *p == '_' || *p == '!' || *p == '%' || *p == ',' || *p == '@')
        {
            p++;
        }
        else
        {
            return 0;
        }
    }
    
    return 1;
}


/*
 * Get the index of the given alias in the aliases array.
 * 
 * Returns the zero-based index, or -1 if the alias isn't found.
 */
int alias_list_index(char *alias)
{
    int i;
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(aliases[i].name && strcmp(aliases[i].name, alias) == 0)
        {
            return i;
        }
    }
    return -1;
}


/*
 * Parse an alias name by searching the alias definition and returning
 * the aliased value, NULL if the alias value is NULL, or the same alias
 * name if the alias is not defined.
 */
char *get_alias_val(char *cmd)
{
    if(!valid_alias_name(cmd))
    {
        return cmd;
    }

    /* Now get the alias value */
    int i = alias_list_index(cmd);
    return (i >= 0) ? aliases[i].val : cmd;
}


/*
 * Run the given alias as a command. Used to implement tcsh's "Special Aliases"
 * functionality, where special aliases get processed and executed as commands
 * under specific conditions. Only "cd" does its own thing by processing its alias
 * command "cwdcmd", as it needs to handle some gray cases which we won't bother
 * with here (see cd.c for more info).
 */
void run_alias_cmd(char *alias)
{
    char *cmd = get_alias_val(alias);
    if(cmd && cmd != alias)
    {
        cmd = word_expand_to_str(cmd, FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES);
        if(cmd)
        {
            do_builtin_internal(eval_builtin, 2, (char *[]){ "eval", cmd, NULL });
            free(cmd);
        }
    }
}


/*
 * The alias builtin utility (POSIX). Used to add and print alias definitions.
 * Returns 0 if all the the arguments were successfully defined or printed, 
 * non-zero otherwise.
 * 
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help alias` or `alias -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int alias_builtin(int argc, char **argv)
{
    char *str = NULL;
    char *eq;
    int  res = 0, res3;
    int  i, print = 0;
    int  v = 1, c;

    /*
     * Don't recognize any options in --posix mode (POSIX defines no options),
     * or recognize all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "" : "hvp";
    
    /****************************
     * Process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &ALIAS_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                print = 1;
                break;
        }
    }

    /* Unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* No arguments, print all aliases */
    if(print || v >= argc)
    {
        print_alias_list(&argv[v]);
        return 0;
    }

    /* Loop on arguments, printing or defining each one in turn */
    for( ; v < argc; v++)
    {
        str = argv[v];
        eq = strchr(str, '=');
        
        /* If the argument doesn't have =, print the alias definition */
        if(!eq)
        {
            int i = alias_list_index(str);
            if(i >= 0)
            {
                print_alias(aliases[i].name, aliases[i].val);
            }
            else
            {
                PRINT_ERROR(UTILITY, "alias `%s` is not defined", str);
                res = 1;
            }
        }
        else
        {
            /* If the argument has =, get the alias name, which is the part before the = */
            i = eq-str;
            char tmp[i+1];
            strncpy(tmp, str, i);
            tmp[i] = '\0';

            /*
             * Don't allow aliasing for shell keywords. tcsh also doesn't allow aliasing
             * for the words 'alias' and 'unalias' (bash doesn't seem to mind it).
             */
            if(is_keyword(tmp) >= 0 || strcmp(tmp, "alias") == 0 || strcmp(tmp, "unalias") == 0)
            {
                PRINT_ERROR(UTILITY, "cannot alias shell keyword: %s", tmp);
                res = 2;
            }
            else
            {
                /* Set the alias value, which is the part after the = */
                res3 = set_alias(tmp, eq+1);
        
                /* If error, return an error result */
                if(res3)
                {
                    res = res3;
                }
            }
        }
    }
    return res;
}
