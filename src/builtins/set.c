/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

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
#define OPTION_PRIVILEGED       18
#define OPTION_RESTRICTED       21


/*
 * if long_option is the long name of an option, return the short name (one char)
 * of that option.. otherwise return 0.
 *
 * see the manpage for an explanation of what each option does.
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
 * if short_opt is the short name (one char) of an option, return the long name
 * of that option.. otherwise return NULL.
 *
 * see the manpage for an explanation of what each option does.
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
 * return 1 if 'which' is a short one-char option, 0 otherwise.
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
 * return 1 if short-option 'which' is set, 0 if unset.
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
 * save all the options that are set in the $- shell variable, which will consist
 * of a series of characters, each character representing a short option.
 * additionally, save the long option version of the set options in the $SHELLOPTS
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
    if(entry)
    {
        entry->flags |= FLAG_READONLY;
        symtab_entry_setval(entry, buf);
    }

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
 * set or unset 'option'.. if 'set' is 0, the option is unset; if it is 1,
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
 * print the on/off state of shell options.. if 'onoff' is non-zero, each option
 * is printed as the option's long name, followed by on or off (according to the
 * option's state).. if 'onoff' is zero, each option is printed as -o or +o,
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
 * reset uid and gid if the previliged mode is off (the +p option).
 */
static inline void do_privileged(int onoff)
{
    /* turning off privileged mode resets ids */
    if(!onoff)
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
    shell_options[OPTION_PRIVILEGED].is_set = onoff;
}


/*
 * turn the restricted mode on (the -r option) or off (the +r option).
 */
static inline int do_restricted(int onoff)
{
    /* -r mode cannot be turned off */
    if(!onoff && shell_options[OPTION_RESTRICTED].is_set)
    {
        fprintf(stderr, 
                "%s: restricted flag cannot be unset after being set\n",
                UTILITY);
        return 0;
    }
    shell_options[OPTION_RESTRICTED].is_set = onoff;
    return 1;
}


/*
 * if the shell is started in the --posix mode, clear all non-POSIX options.
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
 * process the 'ops' string, which is an options string we got from the command
 * line (on shell startup) or from the set builtin utility.. we process the string,
 * char by char, set or unset the option represented by each char.. if the string is
 * -o or +o, we will process a long option, which is provided in the 'extra' parameter.
 * if the first char of 'ops' is '-', options are set, if it is '+', options are unset.
 *
 * returns 0 if the ops string is parsed successfully, -1 on error.. if the ops string
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
            case 'P':
                fprintf(stderr,
                        "%s: cannot change the --%s option when the shell is running\n",
                        UTILITY, long_option(*ops));
                return -1;
                
            case 'p':
                do_privileged(onoff);
                break;

            case 'r':
                if(!do_restricted(onoff))
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
                if(strcmp(extra, "login") == 0 || strcmp(extra, "posix") == 0)
                {
                    fprintf(stderr,
                            "%s: cannot change the --%s option when the shell is running\n",
                            UTILITY, extra);
                    return -1;
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
                        fprintf(stderr, "%s: unknown option: %s\n", UTILITY, extra);
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
                    fprintf(stderr, "%s: unknown option: %c\n", UTILITY, *ops);
                    return -1;
                }
                break;
        }
        ops++;
    }
    return res;
}


/*
 * the set builtin utility (POSIX).. used to set and unset shell options and
 * positional parameters.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help set` from lsh prompt to see a short
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
             * for all but the local symbol table, we check the table lower down in the
             * stack to see if there is a local variable defined with the same name as
             * a global variable.. if that is the case, the local variable takes precedence
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
            }
        }
    }
  
    /* set the positional parameters count */
    if(params)
    {
        entry = get_symtab_entry("#");
        sprintf(buf, "%d", params);
        symtab_entry_setval(entry, buf);
    }
    //__asm__("xchg %%bx, %%bx"::);
    symtab_save_options();
    return 0;
}


/*
 * set the value of shell variable 'name_buf' to the string 'val_buf.. if 'set_global' is
 * non-zero, 'name_buf' is set (or added if not already defined) to the global symbol table..
 * the 'set_flags' parameter contains the flags we need to set on 'name_buf' variable, the
 * 'unset_flags' contains the flags to unset.
 *
 * returns 1 if the shell variable and its flags are set, -1 if the variable is readonly,
 * and 0 for all other errors.
 */
int do_set(char* name_buf, char* val_buf, int set_global, int set_flags, int unset_flags)
{
    /* check the special variables first */
    if(set_special_var(name_buf, val_buf))
    {
        return 1;
    }
    
    /* is this shell restricted? */
    if(startup_finished && option_set('r') && is_restrict_var(name_buf))
    {
        /* r-shells can't set/unset SHELL, ENV, FPATH, or PATH */
        fprintf(stderr, "%s: restricted shells can't set %s\n", SHELL_NAME, name_buf);
        return 0;
    }
    
    /* now to normal variables */
    struct symtab_s *globsymtab = get_global_symtab();
    struct symtab_entry_s *entry1 = do_lookup(name_buf, globsymtab);
    struct symtab_entry_s *entry2 = get_local_symtab_entry(name_buf);
    
    /* the -a option automatically sets the export flag for all variables */
    if(option_set('a'))
    {
        set_flags |= FLAG_EXPORT;
        set_global = 1;
    }
    
    if(set_global)
    {
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
        entry1 = add_to_symtab(name_buf);
        if(!entry1)
        {
            return 0;
        }
    }

    /* can't set readonly variables */
    if(flag_set(entry1->flags, FLAG_READONLY))
    {
        return -1;
    }
    /* set the value */
    if(val_buf)
    {
        symtab_entry_setval(entry1, val_buf);
    }
    /* set the flags */
    entry1->flags |= set_flags;
    /* unset the given flags, if any */
    if(unset_flags)
    {
        entry1->flags &= ~unset_flags;
    }
    return 1;
}
