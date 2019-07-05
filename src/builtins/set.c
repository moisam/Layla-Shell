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

struct
{
    int allexport  :1;    /* -a */
    int notify     :1;    /* -b */
    int braceexpand:1;    /* -B */
    int noclobber  :1;    /* -C */
    int dumpast    :1;    /* -d -- our own extension to dump parser's 
                                   Abstract Source Tree or AST */
    int errexit    :1;    /* -e */
    int errtrace   :1;    /* -E */
    int noglob     :1;    /* -f */
    int nolog      :1;    /* -g (short option is our non-POSIX extension) */
    int keyword    :1;    /* -k -- non-POSIX, ignored in our shell */
    int hashall    :1;    /* -h */
    int histexpand :1;    /* -H */
    int interactive:1;    /* -i */
    int login      :1;    /* -L */
    int pipefail   :1;    /* -l (short option is our non-POSIX extension) */
    int monitor    :1;    /* -m */
    int noexec     :1;    /* -n */
    int ignoreeof  :1;    /* -o (short option is our non-POSIX extension) */
    int privileged :1;    /* -p */
    int quit       :1;    /* -q -- borrowed from tcsh */
    int restricted :1;    /* -r */
    int onecmd     :1;    /* -t */
    int functrace  :1;    /* -T */
    int nounset    :1;    /* -u */
    int verbose    :1;    /* -v */
    int history    :1;    /* -w (short option is our non-POSIX extension) */
    int xtrace     :1;    /* -x */
    int vi         :1;    /* -y (short option is our non-POSIX extension) */
} options;

char short_option(char *long_option)
{
    if(strcmp(long_option, "allexport"  ) == 0) return 'a';
    if(strcmp(long_option, "notify"     ) == 0) return 'b';
    if(strcmp(long_option, "braceexpand") == 0) return 'B';
    if(strcmp(long_option, "noclobber"  ) == 0) return 'C';
    if(strcmp(long_option, "dumpast"    ) == 0) return 'd';
    if(strcmp(long_option, "errexit"    ) == 0) return 'e';
    if(strcmp(long_option, "errtrace"   ) == 0) return 'E';
    if(strcmp(long_option, "noglob"     ) == 0) return 'f';
    if(strcmp(long_option, "nolog"      ) == 0) return 'g';
    if(strcmp(long_option, "hashall"    ) == 0) return 'h';
    if(strcmp(long_option, "histexpand" ) == 0) return 'H';
    if(strcmp(long_option, "interactive") == 0) return 'i';
    if(strcmp(long_option, "keyword"    ) == 0) return 'k';
    if(strcmp(long_option, "pipefail"   ) == 0) return 'l';
    if(strcmp(long_option, "login"      ) == 0) return 'L';
    if(strcmp(long_option, "monitor"    ) == 0) return 'm';
    if(strcmp(long_option, "noexec"     ) == 0) return 'n';
    if(strcmp(long_option, "ignoreeof"  ) == 0) return 'o';
    if(strcmp(long_option, "privileged" ) == 0) return 'p';
    if(strcmp(long_option, "quit"       ) == 0) return 'q';
    if(strcmp(long_option, "restricted" ) == 0) return 'r';
    if(strcmp(long_option, "onecmd"     ) == 0) return 't';
    if(strcmp(long_option, "functrace"  ) == 0) return 'T';
    if(strcmp(long_option, "nounset"    ) == 0) return 'u';
    if(strcmp(long_option, "verbose"    ) == 0) return 'v';
    if(strcmp(long_option, "history"    ) == 0) return 'w';
    if(strcmp(long_option, "xtrace"     ) == 0) return 'x';
    if(strcmp(long_option, "vi"         ) == 0) return 'y';
    return 0;
}

char *long_option(char short_opt)
{
    switch(short_opt)
    {
        case 'a': return "allexport"  ;
        case 'b': return "notify"     ;
        case 'B': return "braceexpand";
        case 'C': return "noclobber"  ;
        case 'd': return "dumpast"    ;
        case 'e': return "errexit"    ;
        case 'E': return "errtrace"   ;
        case 'f': return "noglob"     ;
        case 'g': return "nolog"      ;
        case 'h': return "hashall"    ;
        case 'H': return "histexpand" ;
        case 'i': return "interactive";
        case 'k': return "keyword"    ;
        case 'l': return "pipefail"   ;
        case 'L': return "login"      ;
        case 'm': return "monitor"    ;
        case 'n': return "noexec"     ;
        case 'o': return "ignoreeof"  ;
        case 'p': return "privileged" ;
        case 'q': return "quit"       ;
        case 'r': return "restricted" ;
        case 't': return "onecmd"     ;
        case 'T': return "functrace"  ;
        case 'u': return "nounset"    ;
        case 'v': return "verbose"    ;
        case 'w': return "history"    ;
        case 'x': return "xtrace"     ;
        case 'y': return "vi"         ;
    }
    return NULL;
}

int is_short_option(char which)
{
    switch(which)
    {
        case 'a':
        case 'b':
        case 'B':
        case 'C':
        case 'd':
        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'h':
        case 'H':
        case 'i':
        case 'k':
        case 'l':
        case 'L':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 't':
        case 'T':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
            return 1;
    }
    return 0;
}

int option_set(char which)
{
    switch(which)
    {
        case 'a': return options.allexport  ;
        case 'b': return options.notify     ;
        case 'B': return options.braceexpand;
        case 'C': return options.noclobber  ;
        case 'd': return options.dumpast    ;
        case 'e': return options.errexit    ;
        case 'E': return options.errtrace   ;
        case 'f': return options.noglob     ;
        case 'g': return options.nolog      ;
        case 'h': return options.hashall    ;
        case 'H': return options.histexpand ;
        case 'i': return options.interactive;
        case 'k': return options.keyword    ;
        case 'l': return options.pipefail   ;
        case 'L': return options.login      ;
        case 'm': return options.monitor    ;
        case 'n': return options.noexec     ;
        case 'o': return options.ignoreeof  ;
        case 'p': return options.privileged ;
        case 'q': return options.quit       ;
        case 'r': return options.restricted ;
        case 't': return options.onecmd     ;
        case 'T': return options.functrace  ;
        case 'u': return options.nounset    ;
        case 'v': return options.verbose    ;
        case 'w': return options.history    ;
        case 'x': return options.xtrace     ;
        case 'y': return options.vi         ;
        default : return 0                  ;
    }
}

void reset_options()
{
    memset((void *)&options, 0, sizeof(options));
}

void symtab_save_options()
{
    /* $- is the current option flags. set it appropriately */
    struct symtab_entry_s *entry = add_to_symtab("-");
    char buf[32];
    char *b = buf;
    if(options.allexport  ) *b++ = 'a';
    if(options.notify     ) *b++ = 'b';
    if(options.braceexpand) *b++ = 'B';
    if(options.noclobber  ) *b++ = 'C';
    if(options.dumpast    ) *b++ = 'd';
    if(options.errexit    ) *b++ = 'e';
    if(options.errtrace   ) *b++ = 'E';
    if(options.noglob     ) *b++ = 'f';
    if(options.nolog      ) *b++ = 'g';
    if(options.hashall    ) *b++ = 'h';
    if(options.histexpand ) *b++ = 'H';
    if(options.interactive) *b++ = 'i';
    if(options.keyword    ) *b++ = 'k';
    if(options.pipefail   ) *b++ = 'l';
    if(options.login      ) *b++ = 'L';
    if(options.monitor    ) *b++ = 'm';
    if(options.noexec     ) *b++ = 'n';
    if(options.ignoreeof  ) *b++ = 'o';
    if(options.privileged ) *b++ = 'p';
    if(options.quit       ) *b++ = 'q';
    if(options.restricted ) *b++ = 'r';
    if(options.onecmd     ) *b++ = 't';
    if(options.functrace  ) *b++ = 'T';
    if(options.nounset    ) *b++ = 'u';
    if(options.verbose    ) *b++ = 'v';
    if(options.history    ) *b++ = 'w';
    if(options.xtrace     ) *b++ = 'x';
    if(options.vi         ) *b++ = 'y';
    *b = '\0';
    symtab_entry_setval(entry, buf);
    /* save options in a colon-separated list, similar to what bash does */
    /* make a buffer as large as can be. 11 if the length of the longest option name */
    char buf2[((b-buf)*11)+1];
    char *b2 = buf2;
    b = buf;
    while(*b)
    {
        char *o = long_option(*b);
        sprintf(b2, "%s" , o);
        b2 += strlen(o);
        if(b[1])
        {
            *b2++ = ':';
            *b2   = '\0';
        }
        b++;
    }
    entry = add_to_symtab("SHELLOPTS");
    symtab_entry_setval(entry, buf2);
    entry->flags |= FLAG_READONLY;
}

void set_option(char option, int set)
{
    switch(option)
    {
        case 'a': options.allexport   = set; break;
        case 'b': options.notify      = set; break;
        case 'B': options.braceexpand = set; break;
        case 'C': options.noclobber   = set; break;
        case 'd': options.dumpast     = set; break;
        case 'e': options.errexit     = set; break;
        case 'E': options.errtrace    = set; break;
        case 'f': options.noglob      = set; break;
        case 'g': options.nolog       = set; break;
        case 'h': options.hashall     = set; break;
        case 'H': options.histexpand  = set; break;
        case 'i': options.interactive = set; break;
        case 'k': options.keyword     = set; break;
        case 'l': options.pipefail    = set; break;
        case 'L': options.login       = set; break;
        case 'm': options.monitor     = set; break;
        case 'n': options.noexec      = set; break;
        case 'o': options.ignoreeof   = set; break;
        case 'p': options.privileged  = set; break;
        case 'q': options.quit        = set; break;
        case 'r': options.restricted  = set; break;
        case 't': options.onecmd      = set; break;
        case 'T': options.functrace   = set; break;
        case 'u': options.nounset     = set; break;
        case 'v': options.verbose     = set; break;
        case 'w': options.history     = set; break;
        case 'x': options.xtrace      = set; break;
        case 'y': options.vi          = set; break;
    }
    symtab_save_options();
}

void purge_options(char onoff)
{
    if(onoff)   /* user specified '-o' */
    {
        printf("allexport  \t%s\r\n", options.allexport   ? "on" : "off");
        printf("braceexpand\t%s\r\n", options.braceexpand ? "on" : "off");
        printf("dumpast    \t%s\r\n", options.dumpast     ? "on" : "off");
        printf("errexit    \t%s\r\n", options.errexit     ? "on" : "off");
        printf("errtrace   \t%s\r\n", options.errtrace    ? "on" : "off");
        printf("hashall    \t%s\r\n", options.hashall     ? "on" : "off");
        printf("histexpand \t%s\r\n", options.histexpand  ? "on" : "off");
        printf("history    \t%s\r\n", options.history     ? "on" : "off");
        printf("functrace  \t%s\r\n", options.functrace   ? "on" : "off");
        printf("ignoreeof  \t%s\r\n", options.ignoreeof   ? "on" : "off");
        printf("interactive\t%s\r\n", options.interactive ? "on" : "off");
        printf("login      \t%s\r\n", options.login       ? "on" : "off");
        printf("keyword    \t%s\r\n", options.keyword     ? "on" : "off");
        printf("monitor    \t%s\r\n", options.monitor     ? "on" : "off");
        printf("onecmd     \t%s\r\n", options.onecmd      ? "on" : "off");
        printf("noclobber  \t%s\r\n", options.noclobber   ? "on" : "off");
        printf("noexec     \t%s\r\n", options.noexec      ? "on" : "off");
        printf("noglob     \t%s\r\n", options.noglob      ? "on" : "off");
        printf("nolog      \t%s\r\n", options.nolog       ? "on" : "off");
        printf("notify     \t%s\r\n", options.notify      ? "on" : "off");
        printf("nounset    \t%s\r\n", options.nounset     ? "on" : "off");
        printf("pipefail   \t%s\r\n", options.pipefail    ? "on" : "off");
        printf("privileged \t%s\r\n", options.privileged  ? "on" : "off");
        printf("quit       \t%s\r\n", options.quit        ? "on" : "off");
        printf("restricted \t%s\r\n", options.restricted  ? "on" : "off");
        printf("verbose    \t%s\r\n", options.verbose     ? "on" : "off");
        printf("vi         \t%s\r\n", options.vi          ? "on" : "off");
        printf("xtrace     \t%s\r\n", options.xtrace      ? "on" : "off");
    }
    else        /* user specified '+o' */
    {
        printf("set %co allexport\r\n"  , options.allexport   ? '-' : '+');
        printf("set %co braceexpand\r\n", options.braceexpand ? '-' : '+');
        printf("set %co dumpast\r\n"    , options.dumpast     ? '-' : '+');
        printf("set %co errexit\r\n"    , options.errexit     ? '-' : '+');
        printf("set %co errtrace\r\n"   , options.errtrace    ? '-' : '+');
        printf("set %co hashall\r\n"    , options.hashall     ? '-' : '+');
        printf("set %co histexpand\r\n" , options.histexpand  ? '-' : '+');
        printf("set %co history\r\n"    , options.history     ? '-' : '+');
        printf("set %co functrace\r\n"  , options.functrace   ? '-' : '+');
        printf("set %co ignoreeof\r\n"  , options.ignoreeof   ? '-' : '+');
        printf("set %co interactive\r\n", options.interactive ? '-' : '+');
        printf("set %co login\r\n"      , options.login       ? '-' : '+');
        printf("set %co keyword\r\n"    , options.keyword     ? '-' : '+');
        printf("set %co monitor\r\n"    , options.monitor     ? '-' : '+');
        printf("set %co onecmd\r\n"     , options.onecmd      ? '-' : '+');
        printf("set %co noclobber\r\n"  , options.noclobber   ? '-' : '+');
        printf("set %co noexec\r\n"     , options.noexec      ? '-' : '+');
        printf("set %co noglob\r\n"     , options.noglob      ? '-' : '+');
        printf("set %co nolog\r\n"      , options.nolog       ? '-' : '+');
        printf("set %co notify\r\n"     , options.notify      ? '-' : '+');
        printf("set %co nounset\r\n"    , options.nounset     ? '-' : '+');
        printf("set %co pipefail\r\n"   , options.pipefail    ? '-' : '+');
        printf("set %co privileged\r\n" , options.privileged  ? '-' : '+');
        printf("set %co quit\r\n"       , options.quit        ? '-' : '+');
        printf("set %co restricted\r\n" , options.restricted  ? '-' : '+');
        printf("set %co verbose\r\n"    , options.verbose     ? '-' : '+');
        printf("set %co vi\r\n"         , options.vi          ? '-' : '+');
        printf("set %co xtrace\r\n"     , options.xtrace      ? '-' : '+');
    }
}

static inline void __do_privileged(int onoff)
{
    /* turning off privileged mode resets ids */
    if(!onoff)
    {
        uid_t euid = geteuid(), ruid = getuid();
        gid_t egid = getegid(), rgid = getgid();
        if(euid != ruid) seteuid(ruid);
        if(egid != rgid) setegid(rgid);
    }
    options.privileged = onoff;
}

static inline int __do_restricted(int onoff)
{
    /* -r mode cannot be turned off */
    if(!onoff && options.restricted)
    {
        fprintf(stderr, 
                "%s: restricted flag cannot be unset after being set\r\n",
                UTILITY);
        return 0;
    }
    options.restricted = onoff;
    return 1;
}

int do_options(char *ops, char *extra)
{
    char onoff = (*ops++ == '-') ? 1 : 0;
    int  res   = 0;
    while(*ops)
    {
        switch(*ops)
        {
            case 'a': options.allexport  = onoff; break;
            case 'b': options.notify     = onoff; break;
            case 'B': options.braceexpand= onoff; break;
            case 'C': options.noclobber  = onoff; break;
            case 'd': options.dumpast    = onoff; break;
            case 'e': options.errexit    = onoff; break;
            case 'E': options.errtrace   = onoff; break;
            case 'f': options.noglob     = onoff; break;
            case 'h': options.hashall    = onoff; break;
            case 'H': options.histexpand = onoff; break;
            case 'L':
                fprintf(stderr, "%s: cannot change the login option when the shell is running\n", SHELL_NAME);
                return -1;
                
            case 'k': options.keyword    = onoff; break;
            case 'm': options.monitor    = onoff; break;
            case 'n': options.noexec     = onoff; break;
            case 'p': __do_privileged(onoff); break;
            case 'q': options.quit       = onoff; break;
            case 'r': if(!__do_restricted(onoff)) return -1; break;
            case 't': options.onecmd     = onoff; break;
            case 'T': options.functrace  = onoff; break;
            case 'u': options.nounset    = onoff; break;
            case 'v': options.verbose    = onoff; break;
            case 'w': options.history    = onoff; break;
            case 'x': options.xtrace     = onoff; break;
            case 'i':
                if(onoff && ((getuid() != geteuid()) || (getgid() != getegid())))
                {
                    fprintf(stderr, 
                            "%s: cannot set interactive flag: insufficient permissions\r\n",
                            UTILITY);
                    return -1;
                }
                options.interactive = onoff;
                break;
                
            case 'o':
                if(!extra || !*extra)
                {
                    purge_options(onoff);
                    break;
                }
                /* get the long option */
                if     (strcmp(extra, "allexport"  ) == 0) options.allexport   = onoff;
                else if(strcmp(extra, "dumpast"    ) == 0) options.dumpast     = onoff;
                else if(strcmp(extra, "braceexpand") == 0) options.braceexpand = onoff;
                else if(strcmp(extra, "errexit"    ) == 0) options.errexit     = onoff;
                else if(strcmp(extra, "errtrace"   ) == 0) options.errtrace    = onoff;
                else if(strcmp(extra, "hashall"    ) == 0) options.hashall     = onoff;
                else if(strcmp(extra, "histexpand" ) == 0) options.histexpand  = onoff;
                else if(strcmp(extra, "history"    ) == 0) options.history     = onoff;
                else if(strcmp(extra, "functrace"  ) == 0) options.functrace   = onoff;
                else if(strcmp(extra, "ignoreeof"  ) == 0) options.ignoreeof   = onoff;
                else if(strcmp(extra, "login"  ) == 0)
                {
                    fprintf(stderr, "%s: cannot change the login option when the shell is running\n", SHELL_NAME);
                    return -1;
                }
                else if(strcmp(extra, "keyword"    ) == 0) options.keyword     = onoff;
                else if(strcmp(extra, "monitor"    ) == 0) options.monitor     = onoff;
                else if(strcmp(extra, "onecmd"     ) == 0) options.onecmd      = onoff;
                else if(strcmp(extra, "noclobber"  ) == 0) options.noclobber   = onoff;
                else if(strcmp(extra, "noglob"     ) == 0) options.noglob      = onoff;
                else if(strcmp(extra, "noexec"     ) == 0) options.noexec      = onoff;
                else if(strcmp(extra, "nolog"      ) == 0) options.nolog       = onoff;
                else if(strcmp(extra, "notify"     ) == 0) options.notify      = onoff;
                else if(strcmp(extra, "nounset"    ) == 0) options.nounset     = onoff;
                else if(strcmp(extra, "pipefail"   ) == 0) options.pipefail    = onoff;
                else if(strcmp(extra, "privileged" ) == 0) __do_privileged(onoff);
                else if(strcmp(extra, "quit"       ) == 0) options.quit        = onoff;
                else if(strcmp(extra, "restricted" ) == 0)
                {
                    if(!__do_restricted(onoff)) return -1;
                    break;
                }
                else if(strcmp(extra, "verbose"    ) == 0) options.verbose     = onoff;
                else if(strcmp(extra, "vi"         ) == 0) options.vi          = onoff;
                else if(strcmp(extra, "xtrace"     ) == 0) options.xtrace      = onoff;
                else
                {
                    purge_options(onoff);
                    break;
                }
                res = 1;
                break;
                
            default:
                fprintf(stderr, "%s: unknown option: %c\r\n", UTILITY, *ops);
                return -1;
                break;
        }
        ops++;
    }
    return res;
}

void reset_pos_params()
{
    /* unset all positional params */
    struct symtab_entry_s *hash = get_symtab_entry("#");
    if(hash && hash->val)
    {
        int params = atoi(hash->val);
        if(params != 0)
        {
            int  j;
            char buf[32];
            for(j = 1; j <= params; j++)
            {
                sprintf(buf, "%d", j);
                struct symtab_entry_s *entry = get_symtab_entry(buf);
                if(entry && entry->val) symtab_entry_setval(entry, NULL);
            }
        }
        symtab_entry_setval(hash, "0");
    }
}


int set(int argc, char *argv[])
{
    int i;
    if(argc == 1)
    {
        struct symtab_stack_s *stack = get_symtab_stack();
        for(i = 0; i < stack->symtab_count; i++)
        {
            struct symtab_s       *symtab = stack->symtab_list[i];

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
                        if(!entry->val)
                            printf("%s=\r\n", entry->name);
                        else
                        {
                            char *v     = entry->val;
                            int   quote = 0;
                            while(*v)
                            {
                                if(*v == ' ' || *v == '\t' || *v == '\n')
                                {
                                    quote = 1;
                                    break;
                                }
                                v++;
                            }
                            if(quote) printf("%s='%s'\r\n", entry->name, entry->val);
                            else      printf("%s=%s\r\n"  , entry->name, entry->val);
                        }
                        entry = entry->next;
                    }
          
#ifdef USE_HASH_TABLES

                }
            }
#endif

        }
        return 0;
    }

    int params = 0; /* , unset_params = 1; */
    char buf[32];
    struct symtab_entry_s *entry;
    if(strcmp(argv[1], "--") == 0)
    {
        reset_pos_params();
        i = 2;
        //unset_params = 0;
        goto read_arguments;
    }

    if(strcmp(argv[1], "-") == 0)
    {
        reset_pos_params();
        i = 2;
        set_option('x', 0);
        set_option('v', 0);
        goto read_arguments;
    }
  
    for(i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-' || argv[i][0] == '+')
        {
            int skip = do_options(argv[i], argv[i+1]);
            if(skip < 0) return 1;
            i += skip;
        }
        else break;
    }
  
read_arguments:
    for( ; i < argc; i++)
    {
        sprintf(buf, "%d", ++params);
        entry = add_to_symtab(buf);
        symtab_entry_setval(entry, argv[i]);
    }
  
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


int __set(char* name_buf, char* val_buf, int set_global, int set_flags, int unset_flags)
{
    /* check the special variables first */
    if(set_special_var(name_buf, val_buf)) return 1;
    
    /* is this shell restricted? */
    if(option_set('r') && is_restrict_var(name_buf))
    {
        /* r-shells can't set/unset SHELL, ENV, FPATH, or PATH */
        fprintf(stderr, "%s: restricted shells can't set %s\r\n", SHELL_NAME, name_buf);
        return 0;
    }
    
    /* now to normal variables */
    struct symtab_s *globsymtab = get_global_symtab();
    struct symtab_entry_s *entry1 = __do_lookup(name_buf, globsymtab);
    struct symtab_entry_s *entry2 = get_local_symtab_entry(name_buf);
    
    if(option_set('a')) set_flags |= FLAG_EXPORT, set_global = 1;
    
    if(set_global)
    {
        /*
         * remove the variable from local symbol table (if any is set),
         * and add it to the global symbol table.
         */
        if(entry1 && entry1 == entry2) goto setval;
        if(!entry1) entry1 = __add_to_symtab(name_buf, globsymtab);
        if(entry2)
        {
            if(entry1->val) free(entry1->val);
            entry1->val = entry2->val;
            free(entry2->name);
            rem_from_symtab(entry2);
        }
    }
    else
    {
        entry1 = add_to_symtab(name_buf);
        if(!entry1) return 0;
    }
setval:
    if(flag_set(entry1->flags, FLAG_READONLY)) return -1;
    if(val_buf) symtab_entry_setval(entry1, val_buf);
    entry1->flags |= set_flags;
    if(unset_flags) entry1->flags &= ~unset_flags;
    return 1;
}
