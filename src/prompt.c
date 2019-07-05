/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "cmd.h"
#include "symtab/symtab.h"
#include "backend/backend.h"
#include "builtins/setx.h"
#include "debug.h"

char prompt[DEFAULT_LINE_MAX];

#define PS1     "PS1"
#define PS2     "PS2"
#define PS3     "PS3"
#define PS4     "PS4"

char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *month[]   = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                    "Sep", "Oct", "Nov", "Dec" };

static inline int is_octal(char c)
{
    if(c >= '0' && c <= '7') return 1;
    return 0;
}

char *__evaluate_prompt(char *PS)
{
    if(!PS) return NULL;
    prompt[0]  = '\0';
    size_t i   = 0, j = 0, PS_len = strlen(PS);
    time_t tim = time(NULL);
    struct tm *now = gmtime(&tim);
    /* temporary storage buffers for the loop below */
    char host[MAXHOSTNAMELEN];
    char jobs[32];
    char buf[16];
    char *user, *pwd = get_symtab_entry("PWD")->val; /* = getenv("PWD");*/
    char *s;
    
    do
    {
        switch(PS[i])
        {
            case '\\':
                i++;
                switch(PS[i])
                {
                    /* '\\':     the good ol' backslash */
                    case '\\':
                        strcat_c(prompt, j++, '\\');
                        break;
                    
                    /* '\a':     a bell character       */
                    case 'a':
                        strcat_c(prompt, j++, '\a');
                        break;
                        
                    /* '\d':     the date, in "Weekday Month Date" format
                     *           (e.g., "Tue May 26")
                     */
                    case 'd':
                        strcat(prompt, weekday[now->tm_wday]);
                        j += strlen(weekday[now->tm_wday]);
                        strcat_c(prompt, j++, ' ');
                        strcat(prompt, weekday[now->tm_mon]);
                        j += strlen(weekday[now->tm_mon]);
                        strcat_c(prompt, j++, ' ');
                        char day[4];
                        sprintf(day, "%d", now->tm_mday);
                        strcat(prompt, day);
                        j += strlen(day);
                        break;
                        
                    /* '\D{format}':
                     *  The FORMAT is passed to 'strftime', the result is added to prompt.
                     *  An empty FORMAT results in a locale-specific time representation.
                     *  The braces are required.
                     */
                    case 'D':
                        /* missing '{'. forget the date and move along */
                        if(PS[i+1] != '{')
                        {
                            strcat_c(prompt, j++, '\\');
                            strcat_c(prompt, j++, 'D' );
                            continue;
                        }
                        char *p = &PS[i+2];
                        char *p2 = p;
                        while(*p2 && *p2 != '}') ;
                        /* cannot find '}. again, forget the date and move along */
                        if(*p2 != '}')
                        {
                            strcat_c(prompt, j++, '\\');
                            strcat_c(prompt, j++, 'D' );
                            continue;
                        }
                        /* format the date */
                        {
                            /* start an inner scope to avoid a 'switch jumps into scope of identifier with variably modified type' error */
                            char buf[70];
                            int  fmtlen = p2-p;
                            char fmt[fmtlen];
                            strncpy(fmt, p, fmtlen-1);
                            fmt[fmtlen-1] = '\0';
                            if(strftime(buf, sizeof(buf), fmt, now))
                            {
                                strcat(prompt, buf);
                                j += (fmtlen-1);
                            }
                            else fprintf(stderr, "%s: strftime failed\r\n", SHELL_NAME);
                            i = (p2-p)+1;
                        }
                        break;
                        
                    /* '\e':     an escape character */
                    case 'e':
                        strcat_c(prompt, j++, '\e');
                        /*
                         * TODO: we must process escape chars correctly here.
                         */
                        break;
                        
                    /* '\h':     the hostname, up to first dot '.' */
                    case 'h': ;
                        gethostname(host, MAXHOSTNAMELEN);
                        if(strchr(host, '.'))
                        {
                            char *p = host;
                            while(*p && *p != '.') strcat_c(prompt, j++, *p++);
                        }
                        else
                        {
                            strcat(prompt, host);
                            j += strlen(host);
                        }
                        break;
                        
                    /* '\H':     the hostname */
                    case 'H':
                        gethostname(host, MAXHOSTNAMELEN);
                        strcat(prompt, host);
                        j += strlen(host);
                        break;
                        
                    /* '\j':     the number of current jobs */
                    case 'j':
                        sprintf(jobs, "%d", get_total_jobs());
                        strcat(prompt, jobs);
                        j += strlen(jobs);
                        break;
                        
                    /* '\l':     The basename of the shell's terminal device name */
                    case 'l':
                        if(isatty(0))
                        {
                            s = ttyname(0);
                            if(s) s = basename(s);
                            strcat(prompt, s);
                            j += strlen(s);
                        }
                        break;
                        
                    /* '\n':     (really you need an explanation?) */
                    case 'n':
                        strcat_c(prompt, j++, '\n');
                        break;
                        
                    /* '\r':     (really you need an explanation?) */
                    case 'r':
                        strcat_c(prompt, j++, '\r');
                        break;
                        
                    /* '\s':     the name of the shell 'basename $0' */
                    case 's': ;
                        s = get_shell_varp("0", NULL);
                        if(!s) break;
                        strcat(prompt, s);
                        j += strlen(s);
                        break;
                        
                    /* '\t':     the time, in 24-hour HH:MM:SS format */
                    case 't':
                        sprintf(buf, "%d:%d:%d", now->tm_hour, now->tm_min, now->tm_sec);
                        strcat(prompt, buf);
                        j += strlen(buf);
                        break;
                        
                    /* '\T':     the time, in 12-hour HH:MM:SS format */
                    case 'T':
                        sprintf(buf, "%d:%d:%d", (now->tm_hour%12), now->tm_min, now->tm_sec);
                        strcat(prompt, buf);
                        j += strlen(buf);
                        break;
                        
                    /* '\@':     the time, in 12-hour AM/PM format */
                    case '@': ;
                        int hour = now->tm_hour%12;
                        sprintf(buf, "%d:%d:%d %s", hour, now->tm_min, now->tm_sec,
                                now->tm_hour < 12 ? "AM" : "PM");
                        strcat(prompt, buf);
                        j += strlen(buf);
                        break;
                        
                    /* '\A':     the time, in 24-hour HH:MM format */
                    case 'A':
                        sprintf(buf, "%d:%d", now->tm_hour, now->tm_min);
                        strcat(prompt, buf);
                        j += strlen(buf);
                        break;
                        
                    /* '\u':     the username of the current user */
                    case 'u':
                        user = get_shell_varp("USER", NULL);
                        if(user && *user)
                        {
                            strcat(prompt, user);
                            j += strlen(user);
                        }
                        else
                        {
                            strcat_c(prompt, j++, '\\' );
                            strcat_c(prompt, j++, PS[i]);
                        }
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
                        if(pwd)
                        {
                            struct symtab_entry_s *home = get_symtab_entry("HOME");
                            if(home && home->val)
                            {
                                if(strcmp(pwd, home->val) == 0)
                                {
                                    strcat_c(prompt, j++, '~');
                                    break;
                                }
                            }
                            strcat(prompt, pwd);
                            j += strlen(pwd);
                        }
                        else
                        {
                            strcat_c(prompt, j++, '\\' );
                            strcat_c(prompt, j++, PS[i]);
                        }
                        break;
                        
                    /* '\W':     The basename of '$PWD', with '$HOME' abbreviated
                     *           with a tilde.
                     * 
                     *  TODO:    use the $PROMPT_DIRTRIM variable as bash does.
                     */
                    case 'W':
                        if(pwd)
                        {
                            struct symtab_entry_s *home = get_symtab_entry("HOME");
                            if(home && home->val)
                            {
                                if(strcmp(pwd, home->val) == 0)
                                {
                                    strcat_c(prompt, j++, '~');
                                    break;
                                }
                            }
                            char *slash = strrchr(pwd, '/');
                            if(slash)
                            {
                                /* PWD is sys root? */
                                if((slash == pwd) && (pwd[1] == '\0')) strcat_c(prompt, j++, '/');
                                else
                                {
                                    strcat(prompt, slash+1);
                                    j += strlen(slash+1);
                                }
                            }
                            else
                            {
                                strcat(prompt, pwd);
                                j += strlen(pwd);
                            }
                        }
                        break;
                        
                    /* '\#':     the command number of this command */
                    case '#':
                        /*
                         * we currently use the same number as that of the history list. we should
                         * maintain a separate list for the newly entered commands.
                         * 
                         * TODO: Add this.
                         */
                        
                    /* '\!':     the history number of this command */
                    case '!':
                        sprintf(buf, "%d", cmd_history_end);
                        strcat(prompt, buf);
                        j += strlen(buf);
                        break;
                        
                    /* '\$':     if superuser, then '#', else '$' */
                    case '$':
                        /*
                         * our $PROMPTCHARS variable is similar to bash and tcsh's promptchars.
                         * if set to 2 chars, the first is used for normal users, the second for
                         * the root user.
                         */
                        s = get_shell_varp("PROMPTCHARS", "$#");
                        if(!s || strlen(s) != 2) s = "$#";
                        
                        if(isroot()) strcat_c(prompt, j++, s[1]);
                        else         strcat_c(prompt, j++, s[0]);
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
                        
                    /********************************************/
                    /* none of the above                        */
                    /********************************************/
                    default:
                        /* '\NNN':   The character whose ASCII code is the octal value NNN */
                        if(is_octal(PS[i]))
                        {
                            int octal = PS[i]-'0';
                            if(is_octal(PS[i+1])) octal = (octal*8) + (PS[++i]-'0');
                            if(is_octal(PS[i+1])) octal = (octal*8) + (PS[++i]-'0');
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
                
            case '!': /* valid only for PS1 */
                i++;
                if(PS[i] == '!') strcat_c(prompt, j++, '!');
                else
                {
                    /* 
                     * we should replace '!' with history index of the next command, 
                     *  as per POSIX. They say:
                     *     "The shell shall replace each instance of the character '!' in 
                     *      PS1 with the history file number of the next command to be 
                     *      typed."
                     */
                    int hist_next = cmd_history_end;
                    char str[32];
                    sprintf(str, "%d", hist_next);
                    strcat(prompt, str);
                    j += strlen(str);
                }
                break;

            default:
                strcat_c(prompt, j++, PS[i]);
                break;
        }
    } while(++i < PS_len);
    
    /************************************************
     * now do POSIX style on the prompt. that means
     * parameter expansion, command substitution,
     * arithmetic expansion, and quote removal.
     ************************************************/
    return word_expand_to_str(prompt);
}

/*
 * for $PS4, repeat the first char to indicate levels of indirection (bash).
 */
void repeat_first_char(char *PS)
{
    if(!PS || !*PS) return;
    int count = get_callframe_count();
    if(count <= 0) return;
    while(count--) putchar(*PS);
}


/* 
 * Parse the PS1 variable to get the prompt.
 */
void evaluate_prompt(char *which)
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
                    prompt[0] = '#';       /* default PS1 for root */
                else
                    prompt[0] = '$';       /* default PS1 for non-root */
            }
            else if(which[2] == '2')
                 prompt[0] = '>';           /* default PS2 */
            else prompt[0] = '+';           /* default PS4 */
        }
        set_terminal_color(COL_WHITE, COL_DEFAULT);
        if(which[2] == '3') repeat_first_char(prompt);
        fprintf(stderr, "%s", prompt);
        return;
    }

    set_terminal_color(COL_WHITE, COL_DEFAULT /* COL_BGBLACK */);
    /* bash extension to decide whether or not to evaluate prompt strings */
    if(optionx_set(OPTION_PROMPT_VARS))
    {
        char *pr = __evaluate_prompt(PS);
        if(!pr) return;
        if(which[2] == '3') repeat_first_char(pr);
        fprintf(stderr, "%s", pr);
        free(pr);
    }
    else
    {
        if(which[2] == '3') repeat_first_char(PS);
        fprintf(stderr, "%s", PS);
    }
}

/* print command line prompt */
void print_prompt()
{
    /* command to execute before printing $PS1 (bash) */
    char *cmd = get_shell_varp("PROMPT_COMMAND", NULL);
    if(cmd)
    {
        command(2, (char *[]){ "command", cmd, NULL });
    }
    evaluate_prompt(PS1);
}

/* print secondary prompt */
void print_prompt2()
{
    evaluate_prompt(PS2);
}

/* print select prompt */
void print_prompt3()
{
    evaluate_prompt(PS3);
}

/* print 'execution trace' prompt */
void print_prompt4()
{
    evaluate_prompt(PS4);
}
