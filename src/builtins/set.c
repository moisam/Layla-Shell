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

/*
 * options recognized by the shell.
 */
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
    int hashall    :1;    /* -h */
    int histexpand :1;    /* -H */
    int interactive:1;    /* -i */
    int keyword    :1;    /* -k -- borrowed from ksh */
    int login      :1;    /* -L */
    int pipefail   :1;    /* -l (short option is our non-POSIX extension) */
    int monitor    :1;    /* -m */
    int noexec     :1;    /* -n */
    int ignoreeof  :1;    /* -o (short option is our non-POSIX extension) */
    int privileged :1;    /* -p */
    int posix      :1;    /* -P (short option is our non-POSIX extension) */
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


/*
 * if long_option is the long name of an option, return the short name (one char)
 * of that option.. otherwise return 0.
 *
 * see the manpage for an explanation of what each option does.
 */
char short_option(char *long_option)
{
    if(strcmp(long_option, "allexport"  ) == 0)
    {
        return 'a';
    }
    if(strcmp(long_option, "notify"     ) == 0)
    {
        return 'b';
    }
    if(strcmp(long_option, "braceexpand") == 0)
    {
        return 'B';
    }
    if(strcmp(long_option, "noclobber"  ) == 0)
    {
        return 'C';
    }
    if(strcmp(long_option, "dumpast"    ) == 0)
    {
        return 'd';
    }
    if(strcmp(long_option, "errexit"    ) == 0)
    {
        return 'e';
    }
    if(strcmp(long_option, "errtrace"   ) == 0)
    {
        return 'E';
    }
    if(strcmp(long_option, "noglob"     ) == 0)
    {
        return 'f';
    }
    if(strcmp(long_option, "nolog"      ) == 0)
    {
        return 'g';
    }
    if(strcmp(long_option, "hashall"    ) == 0)
    {
        return 'h';
    }
    if(strcmp(long_option, "histexpand" ) == 0)
    {
        return 'H';
    }
    if(strcmp(long_option, "interactive") == 0)
    {
        return 'i';
    }
    if(strcmp(long_option, "keyword"    ) == 0)
    {
        return 'k';
    }
    if(strcmp(long_option, "pipefail"   ) == 0)
    {
        return 'l';
    }
    if(strcmp(long_option, "login"      ) == 0)
    {
        return 'L';
    }
    if(strcmp(long_option, "monitor"    ) == 0)
    {
        return 'm';
    }
    if(strcmp(long_option, "noexec"     ) == 0)
    {
        return 'n';
    }
    if(strcmp(long_option, "ignoreeof"  ) == 0)
    {
        return 'o';
    }
    if(strcmp(long_option, "privileged" ) == 0)
    {
        return 'p';
    }
    if(strcmp(long_option, "posix"      ) == 0)
    {
        return 'P';
    }
    if(strcmp(long_option, "quit"       ) == 0)
    {
        return 'q';
    }
    if(strcmp(long_option, "restricted" ) == 0)
    {
        return 'r';
    }
    if(strcmp(long_option, "onecmd"     ) == 0)
    {
        return 't';
    }
    if(strcmp(long_option, "functrace"  ) == 0)
    {
        return 'T';
    }
    if(strcmp(long_option, "nounset"    ) == 0)
    {
        return 'u';
    }
    if(strcmp(long_option, "verbose"    ) == 0)
    {
        return 'v';
    }
    if(strcmp(long_option, "history"    ) == 0)
    {
        return 'w';
    }
    if(strcmp(long_option, "xtrace"     ) == 0)
    {
        return 'x';
    }
    if(strcmp(long_option, "vi"         ) == 0)
    {
        return 'y';
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
        case 'P': return "posix"      ;
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


/*
 * return 1 if 'which' is a short one-char option, 0 otherwise.
 */
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
        case 'P':
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


/*
 * return 1 if short-option 'which' is set, 0 if unset.
 */
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
        case 'P': return options.posix      ;
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


/*
 * reset (unset) all options.
 */
void reset_options()
{
    memset((void *)&options, 0, sizeof(options));
}


/*
 * save all the options that are set in the $- shell variable, which will consist
 * of a series of characters, each character representing a short option.
 * additionally, save the long option version of the set options in the $SHELLOPTS
 * variable (bash), which will contain a colon-separated list of the set options.
 */
void symtab_save_options()
{
    /* $- is the current option flags. set it appropriately */
    //struct symtab_entry_s *entry = add_to_symtab("-");
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
    if(options.posix      ) *b++ = 'P';
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
    //symtab_entry_setval(entry, buf);
    set_shell_varp("-", buf);
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
    //entry = add_to_symtab("SHELLOPTS");
    //symtab_entry_setval(entry, buf2);
    //entry->flags |= FLAG_READONLY;
    set_shell_varp("SHELLOPTS", buf2);
}


/*
 * set or unset 'option'.. if 'set' is 0, the option is unset; if it is 1,
 * the option is set.
 */
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
        case 'P': options.posix       = set; break;
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


/*
 * print the on/off state of shell options.. if 'onoff' is non-zero, each option
 * is printed as the option's long name, followed by on or off (according to the
 * option's state).. if 'onoff' is zero, each option is printed as -o or +o,
 * followed by the option's long name.
 */
void purge_options(char onoff)
{
    if(onoff)   /* user specified '-o' */
    {
        printf("allexport  \t%s\n", options.allexport   ? "on" : "off");
        printf("braceexpand\t%s\n", options.braceexpand ? "on" : "off");
        printf("dumpast    \t%s\n", options.dumpast     ? "on" : "off");
        printf("errexit    \t%s\n", options.errexit     ? "on" : "off");
        printf("errtrace   \t%s\n", options.errtrace    ? "on" : "off");
        printf("hashall    \t%s\n", options.hashall     ? "on" : "off");
        printf("histexpand \t%s\n", options.histexpand  ? "on" : "off");
        printf("history    \t%s\n", options.history     ? "on" : "off");
        printf("functrace  \t%s\n", options.functrace   ? "on" : "off");
        printf("ignoreeof  \t%s\n", options.ignoreeof   ? "on" : "off");
        printf("interactive\t%s\n", options.interactive ? "on" : "off");
        printf("login      \t%s\n", options.login       ? "on" : "off");
        printf("keyword    \t%s\n", options.keyword     ? "on" : "off");
        printf("monitor    \t%s\n", options.monitor     ? "on" : "off");
        printf("onecmd     \t%s\n", options.onecmd      ? "on" : "off");
        printf("noclobber  \t%s\n", options.noclobber   ? "on" : "off");
        printf("noexec     \t%s\n", options.noexec      ? "on" : "off");
        printf("noglob     \t%s\n", options.noglob      ? "on" : "off");
        printf("nolog      \t%s\n", options.nolog       ? "on" : "off");
        printf("notify     \t%s\n", options.notify      ? "on" : "off");
        printf("nounset    \t%s\n", options.nounset     ? "on" : "off");
        printf("pipefail   \t%s\n", options.pipefail    ? "on" : "off");
        printf("privileged \t%s\n", options.privileged  ? "on" : "off");
        printf("posix      \t%s\n", options.posix       ? "on" : "off");
        printf("quit       \t%s\n", options.quit        ? "on" : "off");
        printf("restricted \t%s\n", options.restricted  ? "on" : "off");
        printf("verbose    \t%s\n", options.verbose     ? "on" : "off");
        printf("vi         \t%s\n", options.vi          ? "on" : "off");
        printf("xtrace     \t%s\n", options.xtrace      ? "on" : "off");
    }
    else        /* user specified '+o' */
    {
        printf("set %co allexport\n"  , options.allexport   ? '-' : '+');
        printf("set %co braceexpand\n", options.braceexpand ? '-' : '+');
        printf("set %co dumpast\n"    , options.dumpast     ? '-' : '+');
        printf("set %co errexit\n"    , options.errexit     ? '-' : '+');
        printf("set %co errtrace\n"   , options.errtrace    ? '-' : '+');
        printf("set %co hashall\n"    , options.hashall     ? '-' : '+');
        printf("set %co histexpand\n" , options.histexpand  ? '-' : '+');
        printf("set %co history\n"    , options.history     ? '-' : '+');
        printf("set %co functrace\n"  , options.functrace   ? '-' : '+');
        printf("set %co ignoreeof\n"  , options.ignoreeof   ? '-' : '+');
        printf("set %co interactive\n", options.interactive ? '-' : '+');
        printf("set %co login\n"      , options.login       ? '-' : '+');
        printf("set %co keyword\n"    , options.keyword     ? '-' : '+');
        printf("set %co monitor\n"    , options.monitor     ? '-' : '+');
        printf("set %co onecmd\n"     , options.onecmd      ? '-' : '+');
        printf("set %co noclobber\n"  , options.noclobber   ? '-' : '+');
        printf("set %co noexec\n"     , options.noexec      ? '-' : '+');
        printf("set %co noglob\n"     , options.noglob      ? '-' : '+');
        printf("set %co nolog\n"      , options.nolog       ? '-' : '+');
        printf("set %co notify\n"     , options.notify      ? '-' : '+');
        printf("set %co nounset\n"    , options.nounset     ? '-' : '+');
        printf("set %co pipefail\n"   , options.pipefail    ? '-' : '+');
        printf("set %co privileged\n" , options.privileged  ? '-' : '+');
        printf("set %co posix\n"      , options.posix       ? '-' : '+');
        printf("set %co quit\n"       , options.quit        ? '-' : '+');
        printf("set %co restricted\n" , options.restricted  ? '-' : '+');
        printf("set %co verbose\n"    , options.verbose     ? '-' : '+');
        printf("set %co vi\n"         , options.vi          ? '-' : '+');
        printf("set %co xtrace\n"     , options.xtrace      ? '-' : '+');
    }
}


/*
 * turn the previliged mode on (the -p option) or off (the +p option).
 */
static inline void __do_privileged(int onoff)
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
    options.privileged = onoff;
}


/*
 * turn the restricted mode on (the -r option) or off (the +r option).
 */
static inline int __do_restricted(int onoff)
{
    /* -r mode cannot be turned off */
    if(!onoff && options.restricted)
    {
        fprintf(stderr, 
                "%s: restricted flag cannot be unset after being set\n",
                UTILITY);
        return 0;
    }
    options.restricted = onoff;
    return 1;
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
    int  res   = 0;
    while(*ops)
    {
        switch(*ops)
        {
            case 'a':
                options.allexport  = onoff;
                break;

            case 'b':
                options.notify     = onoff;
                break;

            case 'B':
                options.braceexpand= onoff;
                break;

            case 'C':
                options.noclobber  = onoff;
                break;

            case 'd':
                options.dumpast    = onoff;
                break;

            case 'e':
                options.errexit    = onoff;
                break;

            case 'E':
                options.errtrace   = onoff;
                break;

            case 'f':
                options.noglob     = onoff;
                break;

            case 'h':
                options.hashall    = onoff;
                break;

            case 'H':
                options.histexpand = onoff;
                break;

            case 'L':
                fprintf(stderr,
                        "%s: cannot change the %s option when the shell is running\n",
                        SHELL_NAME, "--login");
                return -1;
                
            case 'k':
                options.keyword    = onoff;
                break;

            case 'm':
                options.monitor    = onoff;
                break;

            case 'n':
                options.noexec     = onoff;
                break;

            case 'p':
                __do_privileged(onoff);
                break;

            case 'P':
                fprintf(stderr,
                        "%s: cannot change the %s option when the shell is running\n",
                        SHELL_NAME, "--posix");
                return -1;
                
            case 'q':
                options.quit       = onoff;
                break;

            case 'r':
                if(!__do_restricted(onoff))
                {
                    return -1;
                }
                break;

            case 't':
                options.onecmd     = onoff;
                break;

            case 'T':
                options.functrace  = onoff;
                break;

            case 'u':
                options.nounset    = onoff;
                break;

            case 'v':
                options.verbose    = onoff;
                break;

            case 'w':
                options.history    = onoff;
                break;

            case 'x':
                options.xtrace     = onoff;
                break;

            case 'i':
                if(onoff && ((getuid() != geteuid()) || (getgid() != getegid())))
                {
                    fprintf(stderr, 
                            "%s: cannot set interactive flag: insufficient permissions\n",
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
                if     (strcmp(extra, "allexport"  ) == 0)
                {
                    options.allexport   = onoff;
                }
                else if(strcmp(extra, "dumpast"    ) == 0)
                {
                    options.dumpast     = onoff;
                }
                else if(strcmp(extra, "braceexpand") == 0)
                {
                    options.braceexpand = onoff;
                }
                else if(strcmp(extra, "errexit"    ) == 0)
                {
                    options.errexit     = onoff;
                }
                else if(strcmp(extra, "errtrace"   ) == 0)
                {
                    options.errtrace    = onoff;
                }
                else if(strcmp(extra, "hashall"    ) == 0)
                {
                    options.hashall     = onoff;
                }
                else if(strcmp(extra, "histexpand" ) == 0)
                {
                    options.histexpand  = onoff;
                }
                else if(strcmp(extra, "history"    ) == 0)
                {
                    options.history     = onoff;
                }
                else if(strcmp(extra, "functrace"  ) == 0)
                {
                    options.functrace   = onoff;
                }
                else if(strcmp(extra, "ignoreeof"  ) == 0)
                {
                    options.ignoreeof   = onoff;
                }
                else if(strcmp(extra, "login"      ) == 0)
                {
                    fprintf(stderr,
                            "%s: cannot change the %s option when the shell is running\n",
                            SHELL_NAME, "--login");
                    return -1;
                }
                else if(strcmp(extra, "keyword"    ) == 0)
                {
                    options.keyword     = onoff;
                }
                else if(strcmp(extra, "monitor"    ) == 0)
                {
                    options.monitor     = onoff;
                }
                else if(strcmp(extra, "onecmd"     ) == 0)
                {
                    options.onecmd      = onoff;
                }
                else if(strcmp(extra, "noclobber"  ) == 0)
                {
                    options.noclobber   = onoff;
                }
                else if(strcmp(extra, "noglob"     ) == 0)
                {
                    options.noglob      = onoff;
                }
                else if(strcmp(extra, "noexec"     ) == 0)
                {
                    options.noexec      = onoff;
                }
                else if(strcmp(extra, "nolog"      ) == 0)
                {
                    options.nolog       = onoff;
                }
                else if(strcmp(extra, "notify"     ) == 0)
                {
                    options.notify      = onoff;
                }
                else if(strcmp(extra, "nounset"    ) == 0)
                {
                    options.nounset     = onoff;
                }
                else if(strcmp(extra, "pipefail"   ) == 0)
                {
                    options.pipefail    = onoff;
                }
                else if(strcmp(extra, "privileged" ) == 0)
                {
                    __do_privileged(onoff);
                }
                else if(strcmp(extra, "posix"      ) == 0)
                {
                    fprintf(stderr,
                            "%s: cannot change the %s option when the shell is running\n",
                            SHELL_NAME, "--posix");
                    return -1;
                }
                else if(strcmp(extra, "quit"       ) == 0)
                {
                    options.quit        = onoff;
                }
                else if(strcmp(extra, "restricted" ) == 0)
                {
                    if(!__do_restricted(onoff))
                    {
                        return -1;
                    }
                    break;
                }
                else if(strcmp(extra, "verbose"    ) == 0)
                {
                    options.verbose     = onoff;
                }
                else if(strcmp(extra, "vi"         ) == 0)
                {
                    options.vi          = onoff;
                }
                else if(strcmp(extra, "xtrace"     ) == 0)
                {
                    options.xtrace      = onoff;
                }
                else
                {
                    purge_options(onoff);
                    break;
                }
                res = 1;
                break;

            default:
                fprintf(stderr, "%s: unknown option: %c\n", UTILITY, *ops);
                return -1;
                break;
        }
        ops++;
    }
    return res;
}


/*
 * reset the positional parameters by setting the value of each parameter to
 * NULL, followed by setting the value of $# to zero.
 */
void reset_pos_params()
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
 * the set builtin utility (POSIX).. used to set and unset shell options and
 * positional parameters.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help set` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int set(int argc, char *argv[])
{
    int i;

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
                            char *v = quote_val(entry->val, 1);
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
    char buf[32];
    struct symtab_entry_s *entry;
    /*
     * special option '--': reset positional parameters and stop parsing options.
     */
    if(strcmp(argv[1], "--") == 0)
    {
        reset_pos_params();
        i = 2;
        goto read_arguments;
    }

    /*
     *  special option '-': reset positional parameters, turn the -x and -v options off,
     *  and stop parsing options.
     */
    if(strcmp(argv[1], "-") == 0)
    {
        reset_pos_params();
        i = 2;
        set_option('x', 0);
        set_option('v', 0);
        goto read_arguments;
    }
  
    /* parse the rest of options */
    for(i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-' || argv[i][0] == '+')
        {
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
  
read_arguments:
    /* set the positional parameters */
    for( ; i < argc; i++)
    {
        sprintf(buf, "%d", ++params);
        entry = add_to_symtab(buf);
        symtab_entry_setval(entry, argv[i]);
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
int __set(char* name_buf, char* val_buf, int set_global, int set_flags, int unset_flags)
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
    struct symtab_entry_s *entry1 = __do_lookup(name_buf, globsymtab);
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
        if(entry1 && entry1 == entry2)
        {
            goto setval;
        }
        if(!entry1)
        {
            entry1 = __add_to_symtab(name_buf, globsymtab);
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
            rem_from_symtab(entry2);
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

setval:
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
