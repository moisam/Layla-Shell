/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: prompt.c
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

/* macro definition needed to use gethostname() */
#define _XOPEN_SOURCE   500

/* 
 * we use the GNU version of basename() as it doesn't modify the
 * path variable passed to it.
 */
#define _GNU_SOURCE         /* basename() */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>
#include "include/cmd.h"
#include "symtab/symtab.h"
#include "backend/backend.h"
#include "builtins/builtins.h"
#include "builtins/setx.h"
#include "include/debug.h"

/* buffer to use when processing a prompt string */
char prompt[DEFAULT_LINE_MAX];

#define PS1     "PS1"
#define PS2     "PS2"
#define PS3     "PS3"
#define PS4     "PS4"

char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *month[]   = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                    "Sep", "Oct", "Nov", "Dec" };

/*
 * Return 1 if char c is an octal digit, 0 otherwise.
 */
static inline int is_octal(char c)
{
    if(c >= '0' && c <= '7')
    {
        return 1;
    }
    return 0;
}


/*
 * Get the tty name. If using zsh's '%l' escape sequence, remove the /dev/tty or the /dev/
 * prefix if the tty name starts with one of them. otherwise, get the basename of the tty
 * device and add it to the buffer.
 * 
 * Return value is the number of characters added to the buffer (length of used tty name).
 */
int get_ttyname(int rem_prefix)
{
    int k = cur_tty_fd();
    if(k >= 0)
    {
        char *s = ttyname(k);
        if(!s)
        {
            return 0;
        }
       
        if(rem_prefix)
        {
            /* remove the /dev/tty prefix, if any */
            if(strstr(s, "/dev/tty") == s)
            {
                s += 8;
            }
            /* remove the /dev/ prefix, if any */
            else if(strstr(s, "/dev/") == s)
            {
                s += 5;
            }
        }
        else
        {
            s = basename(s);
        }
        strcat(prompt, s);
        return strlen(s);
    }
    return 0;
}


/*
 * Get the current working directory name, formatted properly as required by the
 * different escape sequences. This function parses POSIX \w and \W escape sequences,
 * in addition to zsh %d, %/ and %~ escape sequences.
 * 
 * Return value is the number of characters added to the buffer (length of cwd used).
 */
int get_pwd(char *pwd, char *escseq, int baseonly, int tildsub)
{
    if(!pwd)
    {
        strcat(prompt, escseq);
        return strlen(escseq);
    }
    
    char *home = get_shell_varp("HOME", NULL);
    if(home && *home)
    {
        int homelen = strlen(home);
        /* home is abbreviated as ~ if requested */
        if(strstr(pwd, home) == pwd)
        {
            /* pwd is home (we don't know if it ends in / or not) */
            if(pwd[homelen] == '\0' || (pwd[homelen] == '/' && pwd[homelen+1] == '\0'))
            {
                if(tildsub)
                {
                    strcat(prompt, "~");
                    return 1;
                }
                else
                {
                    strcat(prompt, home);
                    return strlen(home);
                }
            }

            /* full path with tilde substitution: '%~' (zsh) and '\w' (POSIX) */
            if(!baseonly && tildsub)
            {
                strcat(prompt, "~");
                pwd += homelen;
                strcat(prompt, pwd);
                return strlen(pwd)+1;
            }
        }
    }
    
    /* base name only: '\W' (POSIX) */
    if(baseonly)
    {
        char *pwd2 = basename(pwd);
        /* if pwd is the system root, basename will by empty */
        if(pwd2 && *pwd2)
        {
            pwd = pwd2;
        }
    }

    /* full path with no tilde substitution: '%d' and '%/' (zsh) */
    strcat(prompt, pwd);
    return strlen(pwd);
}


/*
 * Get the index of the current history command. This function parses POSIX \! escape
 * sequence, in addition to zsh %h escape sequence.
 * 
 * Return value is the number of characters added to the buffer.
 */
int get_histindex(void)
{
    char buf[32];
    sprintf(buf, "%d", cmd_history_end+1);
    strcat(prompt, buf);
    return strlen(buf);
}


/*
 * Format the date according to the passed format string. This function parses POSIX \d and \D
 * escape sequences, in addition to zsh %D, %w, %W escape sequences.
 * 
 * Return value is the number of characters added to the buffer.
 */
int get_date(char *fmt, struct tm *now)
{
    char buf[128];
    if(strftime(buf, sizeof(buf), fmt, now))
    {
        strcat(prompt, buf);
        return strlen(buf);
    }
    else
    {
        PRINT_ERROR(SHELL_NAME, "strftime failed");
        return 0;
    }
}


/*
 * Evaluate a prompt string, substituting both POSIX '\' and zsh '%' escape
 * sequences, then performing word expansion on the prompt.
 * 
 * Result is the malloc'd word-expanded prompt string.
 */
char *evaluate_prompt(char *PS)
{
    if(!PS)
    {
        return NULL;
    }
    prompt[0] = '\0';
    size_t i = 0, j = 0, PS_len = strlen(PS);
    int k;
    time_t tim = time(NULL);
    struct tm *now = gmtime(&tim);
    /* temporary storage buffers for the loop below */
    char host[MAXHOSTNAMELEN];
    char buf[32];
    char *user, *pwd = get_shell_varp("PWD", NULL);
    char *s, c;
    
    do
    {
        switch((c = PS[i]))
        {
            /*
             * zsh has an option, PROMPT_PERCENT, which determines whether we recognize escape
             * sequences that are introduced by the percent sign, instead of the backslash character.
             * All of these are zsh non-POSIX extensions. as some of them are really useful, we include
             * them here (well, most of them anyway). being POSIX-compliant and all, we don't enable
             * this option by default.
             * 
             * For the complete list of zsh escape sequences, see section "Prompt Expansion" of the
             * zsh manpage.
             */
            case '%':
                if(!optionx_set(OPTION_PROMPT_PERCENT))
                {
                    /* just add the % char and continue */
                    strcat_c(prompt, j++, PS[i]);
                    break;
                }
                /*
                 * NOTE: Fall through to the next case, so that we can handle both POSIX and zsh
                 * escape sequences using the same switch clause.
                 * 
                 * WARNING: DO NOT put any intervening cases (or a break statement) here!
                 */
                __attribute__((fallthrough));

            /*
             * Normal POSIX escape sequences.
             */
            case '\\':
                i++;
                switch(PS[i])
                {
                    /* '\\':     the good ol' backslash */
                    case '\\':
                        strcat_c(prompt, j++, c);
                        /* if(c != '\\') */ strcat_c(prompt, j++, '\\');
                        break;
                    
                    /* '\a':     a bell character       */
                    case 'a':
                        beep();
                        //strcat_c(prompt, j++, '\a');
                        break;
                        
                    /* '%c':     the basename of cwd (zsh) */
                    case 'c':
                    case 'C':
                    case '.':
                        if(c == '%')
                        {
                            sprintf(buf, "\\%c", PS[i]);
                            j += get_pwd(pwd, buf, 1, (PS[i] == 'c') ? 1 : 0);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    case 'd':
                        if(c == '%')    /* handle zsh '%d' (full pwd path with no tilde substitution) */
                        {
                            sprintf(buf, "%%%c", PS[i]);
                            j += get_pwd(pwd, buf, 0, 0);
                            break;
                        }
                        
                        /*
                         * '\d':     the date, in "Weekday Month Date" format
                         *           (e.g., "Tue May 26")
                         */
                        j += get_date("%a %b %d", now);
                        break;
                        
                    case 'D':                        
                        /*
                         * '\D{format}':
                         *  The FORMAT is passed to 'strftime', the result is added to prompt.
                         *  An empty FORMAT results in a locale-specific time representation.
                         *  The braces are required.
                         */

                        /* missing '{'. forget the date and move along */
                        if(PS[i+1] != '{')
                        {
                            if(c == '%')        /* zsh's '%D' (prints the date in yy-mm-dd format) */
                            {
                                j += get_date("%F", now);
                            }
                            else
                            {
                                strcat_c(prompt, j++, '\\');
                                strcat_c(prompt, j++, 'D' );
                            }
                            break;
                        }
                        char *p = &PS[i+2];
                        char *p2 = p;
                        while(*p2 && *p2 != '}')
                        {
                            p2++;
                        }
                        /* cannot find '}. again, forget the date and move along */
                        if(*p2 != '}')
                        {
                            strcat_c(prompt, j++, '\\');
                            strcat_c(prompt, j++, 'D' );
                            break;
                        }
                        /* format the date */
                        {
                            /* start an inner scope to avoid a 'switch jumps into scope of identifier with variably modified type' error */
                            int  fmtlen = p2-p;
                            char fmt[fmtlen+1];
                            strncpy(fmt, p, fmtlen);
                            fmt[fmtlen] = '\0';
                            if(c == '%')        /* replace zsh's extensions with strftime()'s notation */
                            {
                                for(k = 0; k < fmtlen; k++)
                                {
                                    if(fmt[k] == '%')
                                    {
                                        switch(fmt[++k])
                                        {
                                            case 'f': fmt[k] = 'e'; break;
                                            case 'K': fmt[k] = 'k'; break;
                                            case 'L': fmt[k] = 'l'; break;
                                            /* out of laziness, convert zsh's micro- and nano-secs format to secs since Unix epoch */
                                            case '.':
                                            case '1':
                                            case '2':
                                            case '3':
                                            case '4':
                                            case '5':
                                            case '6':
                                            case '7':
                                            case '8':
                                            case '9':
                                                fmt[k] = 's'; break;
                                        }
                                    }
                                }
                            }
                            j += get_date(fmt, now);
                            i += fmtlen+2;
                        }
                        break;
                        
                    /* '\e':     an escape character */
                    case 'e':
                        strcat_c(prompt, j++, '\e');
                        /*
                         * TODO: we must process escape chars correctly here.
                         */
                        break;
                        
                    /*
                     * '%m':     the hostname, up to the first dot (zsh)
                     *           but we won't implement the component count feature - see man zsh for more
                     */
                    case 'm':
                        
                    /* '\h':     the hostname, up to the first dot '.' */
                    case 'h': ;
                        if((c == '\\' && PS[i] == 'h') || (c == '%' && PS[i] == 'm'))
                        {
                            gethostname(host, MAXHOSTNAMELEN);
                            if(strchr(host, '.'))
                            {
                                char *p = host;
                                while(*p && *p != '.')
                                {
                                    strcat_c(prompt, j++, *p++);
                                }
                            }
                            else
                            {
                                strcat(prompt, host);
                                j += strlen(host);
                            }
                        }
                        else if(c == '%' && PS[i] == 'h')   /* handle zsh '%h' (history index number) */
                        {
                            j += get_histindex();
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '%M':     the hostname (zsh) */
                    case 'M':
                        
                    /* '\H':     the hostname */
                    case 'H':
                        if((c == '\\' && PS[i] == 'H') || (c == '%' && PS[i] == 'M'))
                        {
                            gethostname(host, MAXHOSTNAMELEN);
                            strcat(prompt, host);
                            j += strlen(host);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '%i':     line number of current command (zsh) */
                    case 'i':
                    case 'I':   /* zsh has differences between %i and %I. we treat them the same here */
                        if(c == '%')
                        {
                            s = get_shell_varp("LINENO", "0");
                            strcat(prompt, s);
                            j += strlen(s);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /*
                     * '\j':     the number of current jobs.
                     *           zsh has '%j', which is similar. we handle both cases here.
                     */
                    case 'j':
                        sprintf(buf, "%d", get_total_jobs());
                        strcat(prompt, buf);
                        j += strlen(buf);
                        break;
                        
                    /* '\l':     The basename of the shell's terminal device name
                     *           '%l' does the same in zsh, except it removes /dev/ and /dev/tty if the
                     *           terminal device name starts with either of the two.
                     */
                    case 'l':
                        if(c == '\\')
                        {
                            j += get_ttyname(0);
                        }
                        else if(c == '%')
                        {
                            j += get_ttyname(1);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '%L':     the value of $SHLVL (zsh) */
                    case 'L':
                        if(c == '%')
                        {
                            s = get_shell_varp("SHLVL", "0");
                            strcat(prompt, s);
                            j += strlen(s);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '\r':     (really you need an explanation?) */
                    case 'r':
                        strcat_c(prompt, j++, '\r');
                        break;
                        
                    /* '\s':     the name of the shell 'basename $0' */
                    case 's': ;
                        s = get_shell_varp("0", NULL);
                        if(!s)
                        {
                            break;
                        }
                        s = basename(s);
                        if(!s)
                        {
                            break;
                        }
                        strcat(prompt, s);
                        j += strlen(s);
                        break;
                        
                    case 't':
                        if(c == '%')        /* zsh's '%t' (prints the time in 12-hr AM/PM format) */
                        {
                            j += get_date("%r", now);
                        }
                        else                /* '\t':     the time, in 24-hour HH:MM:SS format */
                        {
                            j += get_date("%T", now);
                        }
                        break;
                        
                    case 'T':
                        if(c == '%')        /* zsh's '%T' (prints the time in 24-hr format, with no seconds) */
                        {
                            j += get_date("%R", now);
                        }
                        else                /* '\T':     the time, in 12-hour HH:MM:SS format */
                        {
                            j += get_date("%I:%M:%S", now);
                        }
                        break;
                        
                    /* '\@':     the time, in 12-hour AM/PM format */
                    case '@': ;
                        j += get_date("%r", now);
                        break;
                        
                    /* '\A':     the time, in 24-hour HH:MM format */
                    case 'A':
                        j += get_date("%R", now);
                        break;
                        
                    /*
                     * '%N':     the name of the currently executing source file or function (zsh)
                     * '%x':     similar to %N, except that function and eval command names are not printed.
                     *           instead, print the name of the file where they were defined.
                     */
                    case 'N':
                        if(c == '%')
                        {
                            struct callframe_s *cf = get_cur_callframe();
                            s = cf ? cf->funcname : NULL;
                            if(s)
                            {
                                strcat(prompt, s);
                                j += strlen(s);
                                break;
                            }
                        }
                        /* NOTE: fall through to handle '%N' if the current command is not a function or eval cmd */
                        __attribute__((fallthrough));
                        
                    case 'x':
                        if(c == '%')
                        {
                            struct callframe_s *cf = get_cur_callframe();
                            s = cf ? cf->srcfile : NULL;
                            if(s)
                            {
                                strcat(prompt, s);
                                j += strlen(s);
                            }
                            else
                            {
                                /* if no file or function name, use $0 (as in zsh) */
                                s = get_shell_varp("0", SHELL_NAME);
                                strcat(prompt, s);
                                j += strlen(s);
                            }
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '\n':     (really you need an explanation?) */
                    case 'n':
                        if(c == '\\')
                        {
                            strcat_c(prompt, j++, '\n');
                            break;
                        }
                        /* NOTE: fall through to handle '%n' */
                        __attribute__((fallthrough));
                        
                    /*
                     * '\u':     the username of the current user. zsh has '%n', which prints
                     *           the value of $USERNAME (we're using $USER here - same thing).
                     */
                    case 'u':
                        if((c == '\\' && PS[i] == 'u') || (c == '%' && PS[i] == 'n'))
                        {
                            user = get_shell_varp("USER", NULL);
                            if(user && *user)
                            {
                                strcat(prompt, user);
                                j += strlen(user);
                                break;
                            }
                        }
                        strcat_c(prompt, j++, c    );
                        strcat_c(prompt, j++, PS[i]);
                        break;
                        
                    /* '\v':     shell version */
                    case 'v':
                    case 'V':
                        strcat(prompt, shell_ver);
                        j += strlen(shell_ver);
                        break;
                        
                    /* '\w':     The current working directory, with '$HOME' 
                     *           abbreviated with a tilde (uses the 
                     *           '$PROMPT_DIRTRIM' variable)
                     */
                    case 'w':
                        if(c == '\\')
                        {
                            sprintf(buf, "\\%c", PS[i]);
                            j += get_pwd(pwd, buf, 0, 1);
                        }
                        else        /* zsh's '%w' (prints the date in day-dd format) */
                        {
                            j += get_date("%a-%d", now);
                        }
                        break;
                        
                    /* '\W':     The basename of '$PWD', with '$HOME' abbreviated
                     *           with a tilde.
                     * 
                     *  TODO:    use the $PROMPT_DIRTRIM variable as bash does.
                     */
                    case 'W':
                        if(c == '\\')
                        {
                            sprintf(buf, "\\%c", PS[i]);
                            j += get_pwd(pwd, buf, 1, 1);
                        }
                        else        /* zsh's '%W' (prints the date in mm/dd/yy format) */
                        {
                            j += get_date("%D", now);
                        }
                        break;
                        
                    /*
                     * '%y':     the name of the tty device without the /dev/ prefix (zsh).
                     *           we simply use the basename of the tty device.
                     */
                    case 'y':
                        if(c == '%')
                        {
                            j += get_ttyname(0);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    case '#':
                        /*
                         * handle zsh's '%#' escape sequence. if the shell is running with privileges (we simply
                         * check if it's running as root), print #, otherwise print %.
                         */
                        if(c == '%')
                        {
                            if(isroot())
                            {
                                strcat_c(prompt, j++, '#');
                            }
                            else
                            {
                                strcat_c(prompt, j++, '%');
                            }
                            break;
                        }
                        __attribute__((fallthrough));
                        
                        /* '\#':     the command number of this command */

                        /*
                         * we currently use the same number as that of the history list. we should
                         * maintain a separate list for the newly entered commands.
                         * 
                         * TODO: Add this.
                         */
                        
                    /*
                     * '\!':     the history number of this command.
                     *           zsh has '%!', which is similar. we handle both cases here.
                     */
                    case '!':
                        j += get_histindex();
                        break;
                        
                    case '*':
                        if(c == '%')        /* zsh's '%*' (prints the time in 24-hr format, with seconds) */
                        {
                            j += get_date("%T", now);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '%?':     exit status of last command executed (zsh) */
                    case '?':
                        if(c == '%')
                        {
                            s = get_shell_varp("?", "0");
                            strcat(prompt, s);
                            j += strlen(s);
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '\$':     if superuser, then '#', else '$' */
                    case '$':
                        /*
                         * our $PROMPTCHARS variable is similar to bash and tcsh's promptchars.
                         * if set to 2 chars, the first is used for normal users, the second for
                         * the root user.
                         */
                        s = get_shell_varp("PROMPTCHARS", "$#");
                        if(!s || strlen(s) != 2)
                        {
                            s = "$#";
                        }
                        
                        if(isroot())
                        {
                            strcat_c(prompt, j++, s[1]);
                        }
                        else
                        {
                            strcat_c(prompt, j++, s[0]);
                        }
                        break;
                        
                    /* '\[':     Begin a sequence of non-printing characters.
                     *           This could be used to embed a terminal control 
                     *           sequence into the prompt
                     */
                    case '[':
                        i++;
                        while(PS[i])
                        {
                            if(PS[i] == '\\' && PS[i+1] == ']')
                            {
                                i++;
                                break;
                            }
                            strcat_c(prompt, j++, PS[i++]);
                        }
                        break;
                        
                    /* '\]':     End a sequence of non-printing characters. */
                    case ']':
                        /*
                         * TODO: Add this.
                         */
                        break;
                        
                    case '~':
                    case '/':
                        if(c == '%')
                        {
                            sprintf(buf, "%%%c", PS[i]);
                            if(PS[i] == '/')    /* handle zsh '%/' (full pwd path with no tilde substitution) */
                            {
                                j += get_pwd(pwd, buf, 0, 0);
                            }
                            else                /* handle zsh '%~' (full pwd path with tilde substitution) */
                            {
                                j += get_pwd(pwd, buf, 0, 1);
                            }
                        }
                        else
                        {
                            strcat_c(prompt, j++, c    );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* recognize these two if the escape sequence begins with '%' */
                    case '%':                           /* %% */
                    case ')':                           /* %) */
                        if(c == '%')
                        {
                            strcat_c(prompt, j++, PS[i]);
                            break;
                        }
                        /*
                         * NOTE: fall through to the next case.
                         * WARNING: DO NOT put any intervening cases (or a break statement) here!
                         */
                        __attribute__((fallthrough));
                        
                    /********************************************/
                    /* none of the above                        */
                    /********************************************/
                    default:
                        /* '\NNN':   The character whose ASCII code is the octal value NNN */
                        if(is_octal(PS[i]))
                        {
                            int octal = PS[i]-'0';
                            if(is_octal(PS[i+1]))
                            {
                                octal = (octal*8) + (PS[++i]-'0');
                            }
                            if(is_octal(PS[i+1]))
                            {
                                octal = (octal*8) + (PS[++i]-'0');
                            }
                            strcat_c(prompt, j++, octal);
                        }
                        else
                        {
                            strcat_c(prompt, j++, '\\' );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                }
                break;
                
            /*
             * zsh has an option, PROMPT_BANG, which determines whether we substitute bangs '!'
             * during prompt expansion. as we're trying to comply with POSIX as much as possible,
             * we enable this option by default (POSIX doesn't actually specify this option).
             */
            case '!': /* valid only for PS1 */
                if(!optionx_set(OPTION_PROMPT_BANG))
                {
                    strcat_c(prompt, j++, PS[i]);
                    break;
                }
                i++;
                if(PS[i] == '!')
                {
                    strcat_c(prompt, j++, '!');
                }
                else
                {
                    /* 
                     * We should replace '!' with history index of the next command, 
                     * as per POSIX. They say:
                     *     "The shell shall replace each instance of the character '!' in 
                     *      PS1 with the history file number of the next command to be 
                     *      typed."
                     */
                    sprintf(buf, "%d", cmd_history_end);
                    strcat(prompt, buf);
                    j += strlen(buf);
                }
                break;
                
            default:
                strcat_c(prompt, j++, PS[i]);
                break;
        }
    } while(++i < PS_len);
    
    /*
     * now go POSIX-style on the prompt. that means parameter expansion,
     * command substitution, arithmetic expansion, and quote removal (but
     * don't remove whitespace chars).
     */
    struct word_s *w = word_expand_one_word(prompt, 0);
    if(w)
    {
        /* perform pathname expansion and quote removal */
        struct word_s *wordlist = pathnames_expand(w);
        char *res = wordlist_to_str(wordlist, WORDLIST_NO_SPACES);
        free_all_words(wordlist);
        return res ? : __get_malloced_str(prompt);
    }
    return __get_malloced_str(prompt);
    //return word_expand_to_str(prompt);
}

/*
 * For $PS4, repeat the first char to indicate levels of indirection (bash).
 */
void repeat_first_char(char *PS)
{
    if(!PS || !*PS)
    {
        return;
    }
    int count = get_callframe_count();
    if(count <= 0)
    {
        return;
    }
    while(count--)
    {
        putchar(*PS);
    }
}


/* 
 * Parse the PS1 variable to get the prompt.
 */
void do_print_prompt(char *which)
{
    struct symtab_entry_s *entry = get_symtab_entry(which);
    char *PS = entry->val;      /* getenv(which); */
    if(!PS || PS[0] == '\0')
    {
        if(which[2] == '3')
        {
            strcpy(prompt, "#? ");
        }
        else
        {
            prompt[1] = ' ';
            prompt[2] = '\0';
            if(which[2] == '1')
            {
                strcpy(prompt, "$ ");      /* default PS1 */
                if(isroot())
                {
                    prompt[0] = '#';       /* default PS1 for root */
                }
                else
                {
                    prompt[0] = '$';       /* default PS1 for non-root */
                }
            }
            else if(which[2] == '2')
            {
                prompt[0] = '>';           /* default PS2 */
            }
            else
            {
                prompt[0] = '+';           /* default PS4 */
            }
        }
        set_terminal_color(COL_WHITE, COL_DEFAULT);
        if(which[2] == '4')
        {
            repeat_first_char(prompt);
        }
        fprintf(stderr, "%s", prompt);
        return;
    }

    set_terminal_color(COL_WHITE, COL_DEFAULT);
    /* bash extension to decide whether or not to evaluate prompt strings */
    if(optionx_set(OPTION_PROMPT_VARS))
    {
        char *pr = evaluate_prompt(PS);
        if(!pr)
        {
            return;
        }
        if(which[2] == '4')
        {
            repeat_first_char(pr);
        }
        fprintf(stderr, "%s", pr);
        free(pr);
    }
    else
    {
        if(which[2] == '4')
        {
            repeat_first_char(PS);
        }
        fprintf(stderr, "%s", PS);
    }
}

/* 
 * Print the primary prompt $PS1.
 */
void print_prompt(void)
{
    /* command to execute before printing $PS1 (bash) */
    char *cmd = get_shell_varp("PROMPT_COMMAND", NULL);
    if(cmd)
    {
        command_builtin(2, (char *[]){ "command", cmd, NULL });
    }
    do_print_prompt(PS1);
}

/*
 * Print secondary prompt $PS2.
 */
void print_prompt2(void)
{
    do_print_prompt(PS2);
}

/*
 * Print the select loop prompt $PS3.
 */
void print_prompt3(void)
{
    do_print_prompt(PS3);
}

/*
 * Print 'execution trace' prompt $PS4.
 */
void print_prompt4(void)
{
    do_print_prompt(PS4);
}
