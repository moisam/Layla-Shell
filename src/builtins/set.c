/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: set.c
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

/* macro definition needed to use seteuid() and setegid() */
#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"
#include "setx.h"

#define UTILITY     "set"

/* all the short option option names */
char *short_options = "abBCdeEfghHklLmnopPqrtTuvwxy";

/* the long option names and whether each option is on or off */
struct option_s
{
    char *name;
    int   is_set;
}
shell_options[] =
{
    { "allexport"  , 0 },
    { "notify"     , 0 },
    { "braceexpand", 0 },
    { "noclobber"  , 0 },
    { "dumpast"    , 0 },
    { "errexit"    , 0 },
    { "errtrace"   , 0 },
    { "noglob"     , 0 },
    { "nolog"      , 0 },
    { "hashall"    , 0 },
    { "histexpand" , 0 },
    { "keyword"    , 0 },
    { "pipefail"   , 0 },
    { "login"      , 0 },
    { "monitor"    , 0 },
    { "noexec"     , 0 },
    { "ignoreeof"  , 0 },
    { "privileged" , 0 },
    { "posix"      , 0 },
    { "quit"       , 0 },
    { "restricted" , 0 },
    { "onecmd"     , 0 },
    { "functrace"  , 0 },
    { "nounset"    , 0 },
    { "verbose"    , 0 },
    { "history"    , 0 },
    { "xtrace"     , 0 },
    { "vi"         , 0 },
};

int options_count = sizeof(shell_options)/sizeof(struct option_s);

/* some important indices to the array above */
#define OPTION_PRIVILEGED       17
#define OPTION_POSIX            18
#define OPTION_RESTRICTED       20
#define OPTION_HISTORY          25


/*
 * If long_option is the long name of an option, return the short name (one char)
 * of that option. Otherwise return 0.
 *
 * See the manpage for an explanation of what each option does.
 */
char short_option(char *long_option)
{
    int i;
    for(i = 0; i < options_count; i++)
    {
        if(strcmp(long_option, shell_options[i].name) == 0)
        {
            return short_options[i];
        }
    }
    return 0;
}


/*
 * If short_opt is the short name (one char) of an option, return the long name
 * of that option. Otherwise return NULL.
 *
 * See the manpage for an explanation of what each option does.
 */
char *long_option(char short_opt)
{
    int i;
    for(i = 0; i < options_count; i++)
    {
        if(short_opt == short_options[i])
        {
            return shell_options[i].name;
        }
    }
    return NULL;
}


/*
 * Return 1 if 'which' is a short one-char option, 0 otherwise.
 */
int is_short_option(char which)
{
    char *p = short_options;
    while(*p)
    {
        if(*p == which)
        {
            return 1;
        }
        p++;
    }
    return 0;
}


/*
 * Return 1 if short-option 'which' is set, 0 if unset.
 */
int option_set(char which)
{
    int i;
    for(i = 0; i < options_count; i++)
    {
        if(which == short_options[i])
        {
            return shell_options[i].is_set;
        }
    }
    return 0;
}


/*
 * Save all the options that are set in the $- shell variable, which will consist
 * of a series of characters, each character representing a short option.
 * Additionally, save the long option version of the set options in the $SHELLOPTS
 * variable (bash), which will contain a colon-separated list of the set options.
 */
void symtab_save_options(void)
{
    /* $- is the current option flags. set it appropriately */
    char buf[32];
    char *b = buf;
    int i;
    for(i = 0; i < options_count; i++)
    {
        if(shell_options[i].is_set)
        {
            *b++ = short_options[i];
        }
    }
    
    /* add the interactive option if it's set */
    if(interactive_shell)
    {
        *b++ = 'i';
    }
    
    /* add the read stdin option if it's set */
    if(read_stdin)
    {
        *b++ = 's';
    }
    
    /* null-terminate the string */
    *b = '\0';

    struct symtab_entry_s *entry = add_to_symtab("-");
    entry->flags |= FLAG_READONLY;
    symtab_entry_setval(entry, buf);

    /* save options in a colon-separated list, similar to what bash does */
    /* make a buffer as large as can be. 11 if the length of the longest option name */
    char buf2[((b-buf)*11)+1];
    char *b2 = buf2;
    b = buf;
    while(*b)
    {
        char *o = long_option(*b);
        if(o)
        {
            sprintf(b2, "%s" , o);
            b2 += strlen(o);
            if(b[1])
            {
                *b2++ = ':';
                *b2   = '\0';
            }
        }
        b++;
    }
    set_shell_varp("SHELLOPTS", buf2);
}


/*
 * Set or unset 'option'. If 'set' is 0, the option is unset; if it is 1,
 * the option is set.
 */
void set_option(char short_opt, int set)
{
    int i;
    for(i = 0; i < options_count; i++)
    {
        if(short_opt == short_options[i])
        {
            shell_options[i].is_set = set;
            return;
        }
    }
}


/*
 * Print the on/off state of shell options. If 'onoff' is non-zero, each option
 * is printed as the option's long name, followed by on or off (according to the
 * option's state). If 'onoff' is zero, each option is printed as -o or +o,
 * followed by the option's long name.
 */
void print_options(char onoff)
{
    int i;
    for(i = 0; i < options_count; i++)
    {
        if(onoff)   /* user specified '-o' */
        {
            printf("%-11s\t%s\n", shell_options[i].name,
                                  shell_options[i].is_set ? "on" : "off");
        }
        else        /* user specified '+o' */
        {
            printf("set %co %s\n", shell_options[i].is_set ? '-' : '+',
                                   shell_options[i].name);
        }
    }
}


/*
 * Reset uid and gid if the previliged mode is off (the +p option).
 */
void do_privileged(int on)
{
    /* turning off privileged mode resets ids */
    if(!on)
    {
        uid_t euid = geteuid(), ruid = getuid();
        gid_t egid = getegid(), rgid = getgid();

        if(euid != ruid)
        {
            seteuid(ruid);
        }

        if(egid != rgid)
        {
            setegid(rgid);
        }
    }
    shell_options[OPTION_PRIVILEGED].is_set = on;
}


/*
 * Turn the restricted mode on (the -r option) or off (the +r option).
 */
int do_restricted(int on)
{
    /* -r mode cannot be turned off */
    if(!on && shell_options[OPTION_RESTRICTED].is_set)
    {
        PRINT_ERROR("%s: restricted flag cannot be unset after being set\n",
                    UTILITY);
        return 0;
    }
    shell_options[OPTION_RESTRICTED].is_set = on;
    return 1;
}


/*
 * Turn history on (the -w option) or off (the +w option).
 */
int do_history(int on)
{
    if(on)
    {
        if(hist_cmds_this_session == 0)
        {
            load_history_list();
        }
    }
    shell_options[OPTION_HISTORY].is_set = on;
    set_optionx(OPTION_SAVE_HIST, on);
    return 1;
}


/*
 * If the POSIX mode is on, switch off all the non-POSIX options.
 */
void reset_non_posix_options(void)
{
    int i;
    static char *op = "BdEHklqrtTw";
    char *p1 = op;
    while(*p1)
    {
        char *p2 = short_options;
        for(i = 0; i < options_count; i++, p2++)
        {
            if(*p1 == *p2)
            {
                shell_options[i].is_set = 0;
                break;
            }
        }
        p1++;
    }
    
    /* reset the privileged option */
    do_privileged(0);
}


/*
 * If 'on' is non-zero, enable the strict POSIX mode (done using the --posix 
 * or -p option). Otherwise disable the POSIX mode.
 */
void do_posix(int on)
{
    /* disable some builtin extensions in the posix '-P' mode */
    if(on)
    {
        reset_non_posix_options();
        disable_extended_options();
        disable_nonposix_builtins();
    }
    shell_options[OPTION_POSIX].is_set = on;
}


/*
 * Process the 'ops' string, which is an options string we got from the command
 * line (on shell startup) or from the set builtin utility. We process the string,
 * char by char, set or unset the option represented by each char. If the string is
 * -o or +o, we will process a long option, which is provided in the 'extra' parameter.
 * If the first char of 'ops' is '-', options are set, if it is '+', options are unset.
 *
 * Returns 0 if the ops string is parsed successfully, -1 on error. If the ops string
 * is -o or +o and the 'extra' parameter is processed, returns 1 so the caller can skip
 * the 'extra' parameter while processing its command line arguments.
 */
int do_options(char *ops, char *extra)
{
    char onoff = (*ops++ == '-') ? 1 : 0;
    int  i, res = 0;
    while(*ops)
    {
        switch(*ops)
        {
            case 'L':
                fprintf(stderr,
                        "%s: cannot change the --%s option when the shell is running\n",
                        UTILITY, long_option(*ops));
                return -1;
                
            case 'P':
                do_posix(onoff);
                break;

            case 'p':
                do_privileged(onoff);
                break;
                
            case 'r':
                if(!do_restricted(onoff))
                {
                    return -1;
                }
                break;
                
            case 'w':
                if(!do_history(onoff))
                {
                    return -1;
                }
                break;
                
            case 'o':
                if(!extra || !*extra)
                {
                    print_options(onoff);
                    break;
                }

                /* get the long option */
                if(strcmp(extra, "login") == 0)
                {
                    fprintf(stderr,
                            "%s: cannot change the --%s option when the shell is running\n",
                            UTILITY, extra);
                    return -1;
                }
                else if(strcmp(extra, "posix") == 0)
                {
                    do_posix(onoff);
                }
                else if(strcmp(extra, "privileged") == 0)
                {
                    do_privileged(onoff);
                }
                else if(strcmp(extra, "restricted") == 0)
                {
                    if(!do_restricted(onoff))
                    {
                        return -1;
                    }
                }
                else if(strcmp(extra, "history") == 0)
                {
                    if(!do_history(onoff))
                    {
                        return -1;
                    }
                }
                else
                {
                    for(i = 0; i < options_count; i++)
                    {
                        if(strcmp(extra, shell_options[i].name) == 0)
                        {
                            shell_options[i].is_set = onoff;
                            break;
                        }
                    }
                
                    /* unrecognized option */
                    if(i == options_count)
                    {
                        PRINT_ERROR("%s: unknown option: %s\n", UTILITY, extra);
                        return -1;
                    }
                }
                res = 1;
                break;

            default:
                for(i = 0; i < options_count; i++)
                {
                    if(*ops == short_options[i])
                    {
                        shell_options[i].is_set = onoff;
                        break;
                    }
                }
                
                /* unrecognized option */
                if(i == options_count)
                {
                    PRINT_ERROR("%s: unknown option: -%c\n", UTILITY, *ops);
                    return -1;
                }
                break;
        }
        ops++;
    }
    return res;
}


/*
 * The set builtin utility (POSIX). Used to set and unset shell options and
 * positional parameters.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help set` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int set_builtin(int argc, char **argv)
{
    int i = 1;

    /* no arguments. print the list of defined shell variables */
    if(argc == 1)
    {
        /* use an alpha list to sort variables alphabetically */
        struct alpha_list_s list;
        init_alpha_list(&list);
        struct symtab_stack_s *stack = get_symtab_stack();
        
        /*
         * print all variables, starting from the global symbol table and walking down
         * to the local symbol table.
         */
        for(i = 0; i < stack->symtab_count; i++)
        {
            /* fetch the next symbol table in the stack */
            struct symtab_s *symtab = stack->symtab_list[i];

            /*
             * For all but the local symbol table, we check the table lower down in the
             * stack to see if there is a local variable defined with the same name as
             * a global variable. If that is the case, the local variable takes precedence
             * over the global variable, and we skip printing the global variable as we
             * will print the local variable when we reach the local symbol table.
             */

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
                        /* check the lower symbol tables don't have the same variable defined */
                        struct symtab_entry_s *entry2 = get_symtab_entry(entry->name);

                        /* check we got an entry that is different from this one */
                        if(entry2 != entry)
                        {
                            /* we have a local variable with the same name. skip this one */
                            entry = entry->next;
                            continue;
                        }

                        /* get the formatted name=val string */
                        char *str = NULL;
                        if(!entry->val)
                        {
                            str = alpha_list_make_str("%s=", entry->name);
                        }
                        else
                        {
                            char *v = quote_val(entry->val, 1, 0);
                            if(!v)
                            {
                                str = alpha_list_make_str("%s=", entry->name);
                            }
                            else
                            {
                                str = alpha_list_make_str("%s=%s", entry->name, v);
                                free(v);
                            }
                        }
                        add_to_alpha_list(&list, str);
                        entry = entry->next;
                    }
          
#ifdef USE_HASH_TABLES

                }
            }
#endif

        }
        print_alpha_list(&list);
        free_alpha_list(&list);
        return 0;
    }

    int params = 0;
    int old_count = pos_param_count();
    char buf[32];
    struct symtab_entry_s *entry;
  
    /* parse options */
    for(i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-' || argv[i][0] == '+')
        {
            /*
             * special option '--': reset positional parameters and stop parsing options.
             */
            if(strcmp(argv[i], "--") == 0)
            {
                reset_pos_params();
                i++;
                break;
            }

            /*
             *  special option '-': reset positional parameters, turn the -x and -v options off,
             *  and stop parsing options.
             */
            if(strcmp(argv[i], "-") == 0)
            {
                reset_pos_params();
                set_option('x', 0);
                set_option('v', 0);
                /* update the options string */
                symtab_save_options();
                i++;
                break;
            }

            int skip = do_options(argv[i], argv[i+1]);

            /* error parsing option */
            if(skip < 0)
            {
                return 1;
            }

            i += skip;
        }
        else
        {
            break;
        }
    }
  
    /* set the positional parameters */
    for( ; i < argc; i++)
    {
        sprintf(buf, "%d", ++params);
        entry = add_to_symtab(buf);
        symtab_entry_setval(entry, argv[i]);
        /*
         * if a dot script calls set to change the values of one or more 
         * special parameters, the results should propagate to the rest of
         * the shell, which means we need to remove the local flag.
         */
        entry->flags &= ~FLAG_LOCAL;
    }
    
    /* set the rest of parameters to NULL */
    if(old_count >= 0)
    {
        for( ; i < old_count; i++)
        {
            sprintf(buf, "%d", ++params);
            entry = get_symtab_entry(buf);
            if(entry)
            {
                symtab_entry_setval(entry, NULL);
                /* ditto */
                entry->flags &= ~FLAG_LOCAL;
            }
        }
    }
  
    /* set the positional parameters count */
    if(params)
    {
        entry = get_symtab_entry("#");
        sprintf(buf, "%d", params);
        symtab_entry_setval(entry, buf);
        /* ditto */
        entry->flags &= ~FLAG_LOCAL;
    }
    //__asm__("xchg %%bx, %%bx"::);
    symtab_save_options();
    return 0;
}


/*
 * Set the value of shell variable 'name_buf' to the string 'val_buf'.
 * 'set_flags' contains the flags to be set in the variable's entry, while
 * 'unset_flags' contains the flags to be turned off. The 'flags' argument
 * contains flags to control do_set(), which can be a combination of:
 * 
 *     SET_FLAG_GLOBAL:    'name_buf' is set (or added if not already defined)
 *                         to the global symbol table.
 *     SET_FLAG_APPEND:    append 'val_buf' to the current variable's value.
 *     SET_FLAG_FORCE_NEW: don't search for an existing entry before adding 
 *                         the variable to the local symbol table.
 *
 * Returns the shell variable if its found and its flags are set, or NULL for errors.
 */
struct symtab_entry_s *do_set(char *name_buf, char *val_buf, int set_flags, 
                              int unset_flags, int flags)
{
    /* the -a option automatically sets the export flag for all variables */
    int export = option_set('a');
    int set_global = flag_set(flags, SET_FLAG_GLOBAL);
    int append = flag_set(flags, SET_FLAG_APPEND);
    int force_new = flag_set(flags, SET_FLAG_FORCE_NEW);
    char *strend;
    struct symtab_entry_s *entry1 = NULL, *entry2;
    
    if(export)
    {
        set_global = 1;
    }
    
    /* is this shell restricted? */
    if(startup_finished && option_set('r') && is_restrict_var(name_buf))
    {
        /* r-shells can't set/unset SHELL, ENV, FPATH, or PATH */
        PRINT_ERROR("%s: restricted shells can't set %s\n", 
                    SOURCE_NAME, name_buf);
        return NULL;
    }
    
    /* positional and special parameters can't be set like this */
    if(is_pos_param(name_buf))
    {
        PRINT_ERROR("%s: cannot set `%s`: positional parameter\n", 
                    SOURCE_NAME, name_buf);
        return NULL;
    }
    else if(is_special_param(name_buf))
    {
        /*
         * if we're declaring a local variable with the name '-', create a copy 
         * of the options string and make it local to the calling function (bash).
         */
        if(name_buf[0] == '-' && name_buf[1] == '\0' && !set_global && !val_buf)
        {
            entry1 = get_symtab_entry("-");
            entry2 = add_to_symtab("-");
            entry2->flags = FLAG_LOCAL | FLAG_READONLY;
            symtab_entry_setval(entry2, entry1 ? entry1->val : NULL);
            return entry2;
        }

        PRINT_ERROR("%s: cannot set `%s`: special parameter\n", 
                    SOURCE_NAME, name_buf);
        return NULL;
    }
    
    /* set the variable's value */
    if(set_global)
    {
        struct symtab_s *globsymtab = get_global_symtab();
        entry1 = do_lookup(name_buf, globsymtab);
        entry2 = get_local_symtab_entry(name_buf);
    
        /*
         * remove the variable from local symbol table (if any is set),
         * and add it to the global symbol table.
         */
        if(!entry1 || entry1 != entry2)
        {
            if(!entry1)
            {
                entry1 = add_to_any_symtab(name_buf, globsymtab);
            }

            if(entry2)
            {
                /* free old value */
                if(entry1->val)
                {
                    free(entry1->val);
                }
                /* store new value */
                entry1->val = get_malloced_str(entry2->val);
                /* remove the local variable, as we have added it to the global symbol table */
                rem_from_symtab(entry2, get_local_symtab());
            }
        }
    }
    else
    {
        if(!force_new)
        {
            entry1 = get_symtab_entry(name_buf);
        }
        
        if(!entry1)
        {
            entry1 = add_to_symtab(name_buf);
        }
    }

    /* can't set readonly variables (but return it so the caller can act on it) */
    if(flag_set(entry1->flags, FLAG_READONLY))
    {
        READONLY_ASSIGN_ERROR(SOURCE_NAME, name_buf, "variable");
        return NULL;
        //return entry1;
    }

    /* set the flags */
    entry1->flags |= set_flags;
    entry1->flags &= ~unset_flags;
    
    if(export)
    {
        entry1->flags |= FLAG_EXPORT;
    }
    
    /* set the value */
    if(val_buf)
    {
        if(append && entry1->val)
        {
            /* do we need to perform arithmetic addition? */
            if(flag_set(entry1->flags, FLAG_INTVAL))
            {
                char *old_val_str = arithm_expand(entry1->val);
                long old_val = 0, new_val;
                
                /* get the old value */
                if(old_val_str)
                {
                    old_val = strtol(old_val_str, &strend, 10);
                    if(*strend)
                    {
                        old_val = 0;
                    }
                }
                
                /* and the new value */
                new_val = strtol(val_buf, &strend, 10);
                if(*strend)
                {
                    new_val = 0;
                }
                
                /* and add them together */
                new_val += old_val;

                /* and set the new value */
                char buf[32];
                sprintf(buf, "%ld", new_val);
                symtab_entry_setval(entry1, buf);
            }
            else
            {
                char *s = alpha_list_make_str("%s%s", entry1->val, val_buf);
                if(s)
                {
                    symtab_entry_setval(entry1, s);
                    free(s);
                }
                else
                {
                    symtab_entry_setval(entry1, val_buf);
                }
            }
        }
        else
        {
            symtab_entry_setval(entry1, val_buf);
        }
    }

    if(strcmp(name_buf, "OPTIND") == 0)
    {
        /*
        strend = NULL;
        int val2 = entry1->val ? strtol(entry1->val, &strend, 10) : 0;
        if(strend && !*strend)
        {
            internal_argi = val2;
            internal_argsub = 0;
            do_set("OPTSUB", "0", 0, 0, flags);
        }
        */
        do_set("OPTSUB", "0", set_flags, unset_flags, flags);
    }
    
    return entry1;
}
