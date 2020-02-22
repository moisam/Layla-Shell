/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: hist_expand.c
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
#include <ctype.h>
#include <errno.h>
#include "../cmd.h"
#include "../debug.h"

char *get_hist_cmd(int index);
char *get_hist_cmdp(char *s, int anchor);
int   insert_hist_cmd(char *p, char *s, int n);
int   get_hist_words(char *cmd, char *wdesig, char **res);
char *get_malloced_word(char *str);
char *get_word_start(char *cmd, int n);
int   do_hist_modif(char *cmd, char *hmod, char **res);

/* last !?string[?] expansion string */
char *query_str = NULL;

/* flag we set when we fail to expand a history command line */
int   errexp = 0;

/* defined in cmdline.c */
extern char *incomplete_cmd;

/* defined in history.c */
extern struct histent_s cmd_history[];


/*
 * perform history expansion on the command line, before it is passed to the
 * parser and thence the executor.. if the quotes parameter is non-zero, we are
 * expanding a command line that is part of a multiline command, and there have
 * been an open quote somewhere in the previous lines.. we use this flag to skip
 * the quoted part (if quote is ') while expanding the command line.
 *
 * returns the malloc'd history-expanded command line, or NULL in case of error.
 */
char *hist_expand(int quotes)
{
    char *p = cmdbuf;
    char *p2, *p3, *p4;
    int i, j, k;
    char errmsg[128];
    int expanded = 0;

    /*
     * if this line is part of a multiline single-quoted string, skip the 
     * quoted part.
     */
    if(quotes == '\'')
    {
        while(*p && *p != '\'')
        {
            p++;
        }
        if(*p == '\'')
        {
            p++;
        }
        /* reached EOL? */
        if(!*p)
        {
            return NULL;
        }
    }

    /*
     * this is a backup copy of the command buffer.. we are going to restore
     * the command buffer after we finish our history expansion.. this is done
     * so that cmdline() can do its own thing, like erasing the previous command
     * from the screen before outputting our expanded version, for example.
     */
    char *backup = malloc(cmdbuf_end+1);
    if(!backup)
    {
        beep();
        PRINT_ERROR("%s: history expansion failed: %s\n", SHELL_NAME, strerror(errno));
        return INVALID_HIST_EXPAND;
    }
    strcpy(backup, cmdbuf);

    /*
     *  perform history expansion, which is done as follows:
     *  - determine which command line from the history list we are going to use
     *    (bash calls this line the 'event').
     *  - determine which parts of the event line we will use (bash calls these parts
     *    the 'words').
     *  - apply the 'modifiers', which are are characters that instruct us to perform
     *    certain operations on the expansion words.
     *  - replace the expansion string (e.g. !! or !n) with the expanded words after
     *    applying the modifiers.
     */
    quotes = 0;
    while(*p)
    {
        p2 = NULL;
        p3 = NULL;
        switch(*p)
        {
            case '\\':
                /* skip backslash-quoted char */
                p++;
                break;
                
            case '\'':
                /* skip single-quoted string */
                p++;
                while(*p && *p != '\'')
                {
                    p++;
                }
                /*
                 * reached EOL? go back one step, so that p++ later will return p to 
                 * EOL so that the loop will end.
                 */
                if(!*p)
                {
                    p--;
                }
                break;
                
            case '`':
            case '"':
                /* save the quote char */
                quotes = *p;
                break;
                
            case '^':                           /* ^string1^string2^ */
                /* get the previous history command */
                p2 = get_hist_cmd(-1);
                if(!p2)
                {
                    PRINT_ERROR("%s: history command not found: !!\n", SHELL_NAME);
                    goto bailout;
                }
                /* convert "^string^string^" to "s/string/string/" */
                p3 = p+1;
                k = 2;
                j = 0;
                while(k && *p3)
                {
                    if(*p3 == '^' && p3[-1] != '\\')
                    {
                        k--;
                    }
                    p3++;
                    j++;
                }
                char *modif = malloc(j+4);
                if(!modif)
                {
                    PRINT_ERROR("%s: failed to expand history at '^': %s\n", SHELL_NAME, strerror(errno));
                    goto bailout;
                }
                strcpy(modif, ":s/");
                p3 = p+1;
                p4 = modif+3;
                while(j-- && *p3)
                {
                    if(*p3 == '^' && p3[-1] != '\\')
                    {
                        *p4 = '/';
                    }
                    else
                    {
                        *p4 = *p3;
                    }
                    p3++;
                    p4++;
                }
                *p4 = '\0';                
                /* perform the "s/string/string/" expansion */
                j = do_hist_modif(p2, modif, &p4);
                free(modif);
                if(errexp)
                {
                    goto bailout;
                }
                /* insert the expanded string */
                i = insert_hist_cmd(p, p4, j);
                if(p4)
                {
                    free(p4);
                }
                p2 = NULL;
                if(i)
                {
                    p += i-1;
                }
                else
                {
                    PRINT_ERROR("%s: failed to expand history at '^'\n", SHELL_NAME);
                    goto bailout;
                }
                expanded = 1;
                break;

            /*
             * ! introduces history expansions.
             */
            case '!':
                /* backslash-escaped '!' */
                if(p > cmdbuf && p[-1] == '\\')
                {
                    break;
                }
                /* '!' just before closing '"' is not considered for expansion (bash) */
                if(p[1] == '"' && quotes == '"')
                {
                    break;
                }
                /*
                 * also don't consider '!' followed by whitespace, '=' or '(' (bash with
                 * extglob shopt option, csh)
                 */
                if(isspace(p[1]) || p[1] == '\0' || p[1] == '=' || p[1] == '(')
                {
                    break;
                }
                p2 = NULL, p3 = NULL, p4 = NULL;
                errmsg[0] = '\0';
                /* get the prev command we will substitute */
                switch(p[1])
                {
                    case '!':                           /* !! */
                        /* get the last command in the history list */
                        p2 = get_hist_cmd(-1);
                        if(!p2)
                        {
                            PRINT_ERROR("%s: history command not found: !!\n", SHELL_NAME);
                            goto bailout;
                        }
                        j = get_hist_words(p2, p+2, &p3);
                        if(errexp)
                        {
                            goto bailout;
                        }
                        j += 2;
                        sprintf(errmsg, "%s: history command not found: !!", SHELL_NAME);
                        break;
                        
                    case '-':                           /* !-n */
                        /* get the n-th last command */
                        p2 = p+2;
                        if(!*p2 || !isdigit(*p2))
                        {
                            PRINT_ERROR("%s: missing numeric argument to !-\n", SHELL_NAME);
                            goto bailout;
                        }
                        i = 0, j = 0;
                        while(*p2 && isdigit(*p2))
                        {
                            i = (i*10) + (*p2)-'0';
                            p2++;
                            j++;
                        }
                        k = -i;
                        p2 = get_hist_cmd(k);
                        j += 2;                     /* add 2 for the '!-' */
                        i = get_hist_words(p2, p+j, &p3);
                        if(errexp)
                        {
                            goto bailout;
                        }
                        j += i;
                        sprintf(errmsg, "%s: history command not found: !%d", SHELL_NAME, k);
                        break;
                        
                    case '?':                           /* !?string[?] */
                        /* get the history command containing the given string */
                        p2 = p+2;
                        if(!*p2 || *p2 == '?')
                        {
                            PRINT_ERROR("%s: missing string argument to !?\n", SHELL_NAME);
                            goto bailout;
                        }
                        j = 0;
                        /* find the end of the string, which is the first space char or ? */
                        while(*p2 && *p2 != '?' && !isspace(*p2))
                        {
                            p2++;
                            j++;
                        }
                        p3 = get_malloced_strl(p+2, 0, j);
                        if(!p3)
                        {
                            PRINT_ERROR("%s: history expansion failed: %s\n", SHELL_NAME, strerror(errno));
                            goto bailout;
                        }
                        k = 2;                  /* we will add 2 for the '!?' */
                        if(*p2 == '?')
                        {
                            k++;     /* and another 1 for the terminating '?' */
                        }
                        j += k;
                        /* get the history command */
                        p2 = get_hist_cmdp(p3, 0);
                        /* save the last query string */
                        if(query_str)
                        {
                            free(query_str);
                        }
                        query_str = p3;
                        i = get_hist_words(p2, p+j, &p3);
                        if(errexp)
                        {
                            goto bailout;
                        }
                        j += i;
                        sprintf(errmsg, "%s: history command not found: !?%s", SHELL_NAME, p3);
                        break;
                        
                    case '#':                           /* !# */
                        /* get the complete command line in the buffer */
                        if(incomplete_cmd)
                        {
                            strcpy(cmdbuf, incomplete_cmd);
                            strcat(cmdbuf, backup);
                            free(incomplete_cmd);
                            incomplete_cmd = NULL;
                        }
                        p = cmdbuf+strlen(cmdbuf)-1;
                        break;
                        
                    case '$':                           /* '!$', a shorthand for '!!:$' */
                        /* get the last command in the history list */
                        p2 = get_hist_cmd(-1);
                        if(!p2)
                        {
                            PRINT_ERROR("%s: history command not found: !!\n", SHELL_NAME);
                            goto bailout;
                        }
                        p4 = ":$";
                        j = get_hist_words(p2, p4, &p3);
                        if(errexp)
                        {
                            goto bailout;
                        }
                        j = 2;
                        sprintf(errmsg, "%s: history command not found: !!", SHELL_NAME);
                        break;
                        
                    default:
                        if(isdigit(p[1]))               /* !n */
                        {
                            p2 = p+1;
                            i = 0, j = 0;
                            /* get n */
                            while(*p2 && isdigit(*p2))
                            {
                                i = (i*10) + (*p2)-'0';
                                p2++;
                                j++;
                            }
                            k = i;
                            /* get the history command #n */
                            p2 = get_hist_cmd(k);
                            if(!p2)
                            {
                                PRINT_ERROR("%s: history command not found: !%d\n", SHELL_NAME, k);
                                goto bailout;
                            }
                            j++;                        /* add 1 for the '!' */
                            i = get_hist_words(p2, p+j, &p3);
                            if(errexp)
                            {
                                goto bailout;
                            }
                            j += i;
                            sprintf(errmsg, "%s: history command not found: !%d", SHELL_NAME, k);
                        }
                        else if(isalpha(p[1]))          /* !string */
                        {
                            p2 = p+1;
                            j = 0;
                            /* get the string */
                            while(*p2 && !isspace(*p2) && *p2 != ':')
                            {
                                p2++;
                                j++;
                            }
                            p3 = get_malloced_strl(p+1, 0, j);
                            if(!p3)
                            {
                                PRINT_ERROR("%s: history expansion failed: %s\n", SHELL_NAME, strerror(errno));
                                goto bailout;
                            }
                            /* get the history command containing the string */
                            p2 = get_hist_cmdp(p3, 1);
                            j++;                        /* add 1 for the '!' */
                            sprintf(errmsg, "%s: history command not found: !%s", SHELL_NAME, p3);
                            free_malloced_str(p3);
                            i = get_hist_words(p2, p+j, &p3);
                            if(errexp)
                            {
                                goto bailout;
                            }
                            j += i;
                        }
                        break;
                }

                /* we've got no words */
                if(!p2)
                {
                    break;
                }
                if(!p3)
                {
                    /* get a copy so we don't modify the original cmd */
                    p2 = get_malloced_str(p2);
                }
                /* apply the modifiers (if any) */
                j += do_hist_modif(p3 ? : p2, p+j, &p4);
                /* error applying the modifiers */
                if(errexp)
                {
                    if(p3)
                    {
                        free_malloced_str(p3);
                    }
                    else
                    {
                        free_malloced_str(p2);
                    }
                    goto bailout;
                }
                if(p4)
                {
                    if(p3)
                    {
                        free_malloced_str(p3);
                    }
                    p3 = p4;
                }
                if(p3)
                {
                    p2 = p3;
                }
                /* insert the expanded history command */
                i = insert_hist_cmd(p, p2, j);
                /* free temp memory */
                if(p3)
                {
                    free_malloced_str(p3);
                }
                else
                {
                    free_malloced_str(p2);
                }
                if(!errexp)
                {
                    /* no errors. skip the expanded part */
                    p += i-1;
                }
                else
                {
                    /* print error and bail out */
                    PRINT_ERROR("%s\n", errmsg);
                    goto bailout;
                }
                expanded = 1;
                break;
        }
        /* move on to the next char */
        p++;
    }
    
    if(expanded)
    {
        p = get_malloced_str(cmdbuf);
    }
    else
    {
        p = NULL;
    }
    /* restore the original command buffer */
    strcpy(cmdbuf, backup);
    /* free the backup copy */
    free(backup);
    /* return the expanded command line */
    return p;
    
bailout:
    /* restore the original command to buffer */
    strcpy(cmdbuf, backup);
    free(backup);
    return INVALID_HIST_EXPAND;
}


/*
 * insert string s at the start of string p, replacing n chars from p.
 *
 * returns the number of characters inserted, 0 means none.
 */
int insert_hist_cmd(char *p, char *s, int n)
{
    errexp = 0;
    if(!s)
    {
        errexp = 1;
        return 0;
    }
    int slen = strlen(s);
    //if(!slen) return 0;
    char *p1, *p2, *p3 = NULL;
    int plen = strlen(p);
    int res = slen;
    /*
     *  replacement string longer is longer than the replaced part.
     *  we need to make room for the extra chars.
     */
    if(slen > n)
    {
        p1 = p+n;
        p2 = p1+slen-n;
        p[plen+slen-n] = '\0';
        /* make some room */
        while((*p2++ = *p1++))
        {
            ;
        }
    }
    /*
     *  replacement string longer is shorter than the replaced part.
     *  we need to shorten the string.
     */
    else if(slen < n)
    {
        p1 = p+slen;
        p2 = p+n;
        p[plen-n+slen] = '\0';
        while((*p1++ = *p2++))
        {
            ;
        }
    }
    p1 = p;
    /* now insert the new string */
    p2 = s;
    p3 = s+slen;
    while(p2 < p3)
    {
        *p1++ = *p2++;
    }
    return res;
}


/*
 * get the history command at the given index, which is 1-based (that is, the
 * first history command is number 1, the last is number cmd_history_end).
 */
char *get_hist_cmd(int index)
{
    if(index == 0)
    {
        /* invalid index */
        return NULL;
    }
    else if(index < 0)
    {
        /* -1 refers to the prev command */
        index += cmd_history_end;
        if(index < 0 || index >= cmd_history_end)
        {
            /* invalid index */
            return NULL;
        }
        return cmd_history[index].cmd;
    }
    else
    {
        if(index > cmd_history_end)
        {
            /* invalid index */
            return NULL;
        }
        return cmd_history[index-1].cmd;
    }
}


/*
 * get the history command that contains string s.. if the anchor parameter is
 * non-zero, the history command must start with string s (which we get by using
 * the '!string' expansion).. otherwise, string s can occur anywhere in the command
 * string (which we get by using the '!?string[?]' expansion).
 */
char *get_hist_cmdp(char *s, int anchor)
{
    /* invalid search string */
    if(!s)
    {
        return NULL;
    }
    /* empty history list */
    if(cmd_history_end == 0)
    {
        return NULL;
    }
    /* search for the command */
    int i = cmd_history_index;
    char *p, *cmd;
    for( ; i >= 0; i--)
    {
        cmd = cmd_history[i].cmd;
        if(!cmd)
        {
            continue;
        }
        /* search the command line for the query string */
        if((p = strstr(cmd, s)))
        {
            /* should s be at the beginning of p? */
            if(anchor)
            {
                /* yes. check if that is the case */
                if(p == cmd)
                {
                    /* return the command */
                    return cmd;
                }
                /* p doesn't start with s. check the next command */
            }
            else
            {
                return cmd;
            }
        }
    }
    /* no command found that contains the given query string */
    return NULL;
}


/*
 * get the words specified by the 'wdesig' word designator.. these typically
 * start with a colon, which can be omitted if the disgnator starts with
 * a ‘^’, ‘$’, ‘*’, ‘-’, or ‘%’.. words are counted from zero.. argument
 * 'cmd' contains the full command line from which we will select the words..
 * the resultant malloc'd string will be placed in the *res parameter.
 *
 * returns the number of characters in the word designator, with *res being 
 * set to NULL if the expansion failed.
 */
int get_hist_words(char *cmd, char *wdesig, char **res)
{
    (*res) = NULL;
    errexp = 0;
    /* sanity checks */
    if(!cmd || !wdesig || !*wdesig)
    {
        return 0;
    }
    int i = 0, n, j;
    /* skip the optional leading : */
    if(*wdesig == ':')
    {
        i++;
        wdesig++;
    }
    /* check we have a valid word designator char */
    if(*wdesig != '^' && *wdesig != '$' && *wdesig != '*' && *wdesig != '-' && *wdesig != '%')
    {
         return 0;
    }
    /* parse the word designator to get the words */
    char *p, *p2;
    switch(*wdesig)
    {
        case '^':           /* get the first arg (word #1) */
            p = get_word_start(cmd, 1);
            if(!p)
            {
                (*res) = get_malloced_str("");
                return i+1;
            }
            p = get_malloced_word(p);
            if(p)
            {
                (*res) = p;
                return i+1;
            }
            break;
            
        case '$':           /* get the last arg */
            p = get_word_start(cmd, -1);
            if(!p)
            {
                (*res) = get_malloced_str("");
                return i+1;
            }
            p = get_malloced_word(p);
            if(p)
            {
                (*res) = p;
                return i+1;
            }
            break;

        case '*':           /* get all args (starting with word #1) */
            p = get_word_start(cmd, 1);
            if(!p)
            {
                (*res) = get_malloced_str("");
                return i+1;
            }
            p = get_malloced_str(p);
            if(p)
            {
                (*res) = p;
                return i+1;
            }
            break;
            
        case '%':           /* get the word matching the last !?string[?] search */
            if(!query_str || !(p = strstr(cmd, query_str)))
            {
                PRINT_ERROR("%s: no match found for word: %s\n", SHELL_NAME, query_str ? : "(null)");
                errexp = 1;
                return i+1;
            }
            /* find the end of the word */
            p2 = p;
            while(*p && !isspace(*p))
            {
                p++;
            }
            j = p2-cmd;
            p = get_malloced_strl(cmd, j, p-cmd-j);
            if(p)
            {
                (*res) = p;
                return i+1;
            }
            break;
            
        case '-':           /* -y is a shorthand for 0-y */
            if(!isdigit(*++wdesig))
            {
                PRINT_ERROR("%s: invalid word index: %c\n", SHELL_NAME, *wdesig);
                errexp = 1;
                return i;
            }
            n = 0, j = 0;
            /* get the number y */
            while(*wdesig && isdigit(*wdesig))
            {
                n = (n*10) + (*wdesig)-'0';
                wdesig++;
                j++;
            }
            i += j+1;         /* add the length of y, plus 1 for '-' */
            p2 = get_word_start(cmd, 0);
            if(!p2)
            {
                (*res) = get_malloced_str("");
                return i;
            }
            p = get_word_start(cmd, n);     /* get the start of the second word */
            if(!p)
            {
                PRINT_ERROR("%s: invalid word index: %d\n", SHELL_NAME, n);
                errexp = 1;
                return i;
            }
            /* go to the end of the word */
            while(*p && !isspace(*p))
            {
                p++;
            }
            j = p2-cmd;
            p = get_malloced_strl(cmd, j, p-cmd-j);
            if(p)
            {
                (*res) = p;
                return i;     /* length of "'-' + y" */
            }
            break;
            
        default:
            if(isdigit(*wdesig))               /* get the n-th word */
            {
                n = 0, j = 0;
                /* get the number n */
                while(*wdesig && isdigit(*wdesig))
                {
                    n = (n*10) + (*wdesig)-'0';
                    wdesig++;
                    j++;
                }
                p = get_word_start(cmd, n);
                if(!p)
                {
                    PRINT_ERROR("%s: invalid word index: %d\n", SHELL_NAME, n);
                    errexp = 1;
                    return j+i;
                }
                /* check for 'x-y' word ranges */
                if(*wdesig == '-')
                {
                    wdesig++;
                    if(isdigit(*wdesig))
                    {
                        i += j+1;       /* add length of x and 1 for '-' */
                        n = 0, j = 0;
                        /* get the number x */
                        while(*wdesig && isdigit(*wdesig))
                        {
                            n = (n*10) + (*wdesig)-'0';
                            wdesig++;
                            j++;
                        }
                        i += j;         /* add length of y */
                        p2 = p;         /* save index of first word */
                        p = get_word_start(cmd, n);     /* get start of 2nd word */
                        if(!p)
                        {
                            PRINT_ERROR("%s: invalid word index: %d\n", SHELL_NAME, n);
                            errexp = 1;
                            return i;
                        }
                        /* get to end of word */
                        while(*p && !isspace(*p))
                        {
                            p++;
                        }
                        j = p2-cmd;
                        p = get_malloced_strl(cmd, j, p-cmd-j);
                        if(p)
                        {
                            (*res) = p;
                            return i;     /* length of "':' + x + '-' + y" */
                        }
                    }
                    else                    /* 'x-' abbreviates 'x-$' without the last word */
                    {
                        i += j+1;       /* add length of x and 1 for '-' */
                        p2 = p;         /* save index of first word */
                        p = get_word_start(cmd, -1);
                        if(!p)
                        {
                            return i;
                        }
                        /* get to start of word */
                        if(!isspace(*p))
                        {
                            p--;
                        }
                        while(p >= cmd && isspace(*p))
                        {
                            p--;
                        }
                        /* only one word in the line? */
                        if(p < cmd)
                        {
                            return i;
                        }
                        j = p2-cmd;
                        p = get_malloced_strl(cmd, j, p-cmd-j+1);
                        if(p)
                        {
                            (*res) = p;
                            return i;     /* length of "':' + x + '-'" */
                        }
                    }
                }
                else if(*wdesig == '*')     /* 'x*' abbreviates 'x-$' */
                {
                    wdesig++;
                    p = get_malloced_str(p);
                    if(p)
                    {
                        (*res) = p;
                        return j+i+1;   /* length of ': + word + *' */
                    }
                }
                else
                {
                    /* return just the word */
                    p = get_malloced_word(p);
                    if(p)
                    {
                        (*res) = p;
                        return j+i;     /* length of ': + word' */
                    }
                }
            }
    }
    return i;
}


/*
 * perform the history expansion modifiers indicated in the 'hmod' parameter.
 * modifiers start with a colon and include one letter of the following:
 *
 *     h, t, r, e, p, q, x, s, &, g, a, G.
 *
 * 'cmd' contains the command line (or words selected by a previous call to
 * get_hist_words(), on which we will apply the requested modifiers.
 * the resultant malloc'd string will be placed in the *res parameter.
 *
 * returns the number of characters in the modifier, with *res being 
 * set to NULL if the expansion failed.
 */
int do_hist_modif(char *cmd, char *hmod, char **res)
{
    *res = NULL;
    errexp = 0;
    /* sanity checks */
    if(!cmd || !hmod || !*hmod)
    {
        return 0;
    }
    /* history modifier must begin with : */
    if(*hmod != ':')
    {
        return 0;
    }
    /* the acceptable (valid) char modifiers */
    static char chars[] = "htrepqxs&gaGuUlL";
    char *p, *p2, *p3, *p4;
    char *origcmd = cmd;
    char *orighmod = hmod;
    int i = 0, n, j;
    int quote_all;          /* the 'q' and 'x' modifiers */
    int repeat = 0;         /* the '&' modifier */
    int global = 0;         /* the 'g' modifier */
    int rptword = 0;        /* the 'a' modifier */
    
    /* process and execute the modifiers */
loop:
    hmod++;
    if(!repeat)
    {
        i++;
    }
    p = chars;
    /* scan the modifiers string to ensure it only contains valid chars */
    while(*p)
    {
        if(*p == *hmod)
        {
            /* char matched. break the loop */
            break;
        }
        /* no match. move on to check against the next char in the valid chars string */
        p++;
    }
    /* the string contains an invalid char */
    if(!*p)
    {
        errexp = 1;
        PRINT_ERROR("%s: unknown modifier letter: %c\n", SHELL_NAME, *hmod);
        if(cmd && cmd != origcmd)
        {
            free(cmd);
        }
        return 0;       /* unknown modifier letter */
    }

    /* execute the modifier char operation */
    p2 = cmd;
    quote_all = 0;
    switch(*p)
    {
        case 'h':           /* keep path head only */
            p4 = p2;
            do
            {
                /* skip to start of next word */
                while(*p4 && isspace(*p4))
                {
                    p4++;
                }
                /* skip leading slash */
                if(*p4 == '/')
                {
                    p4++;
                }
                /* goto next slash */
                while(*p4 && *p4 != '/')
                {
                    if(isspace(*p4))
                    {
                        break;     /* word finished */
                    }
                    p4++;
                }
                /* path head must end in slash */
                if(*p4 == '/')
                {
                    p2 = p4;
                    p3 = p2;
                    /* find the end of the head word */
                    while(*p3 && !isspace(*p3))
                    {
                        p3++;
                    }
                    /* and remove the rest of the path from the word (keep the head) */
                    while((*p2++ = *p3++))
                    {
                        ;
                    }
                    /* don't repeat operation for that same word */
                    if(!rptword)
                    {
                        while(*p4 && !isspace(*p4))
                        {
                            p4++;     /* skip to end of word */
                        }
                    }
                }
                /* repeat the operation for every word in the line */
            } while(global && *p4);
            repeat = 0;
            break;
            
        case 't':           /* keep path tail only */
            /* we will start from the end of the line */
            p4 = p2+strlen(p2)-1;
            do
            {
                /* skip to end of last word */
                while(p4 > cmd && isspace(*p4))
                {
                    p4--;
                }
                /* skip trailing slash */
                if(*p4 == '/')
                {
                    p4--;
                }
                /* goto prev slash */
                while(p4 > cmd && *p4 != '/')
                {
                    if(isspace(*p4))
                    {
                        break;     /* word finished */
                    }
                    p4--;
                }
                /* path head must end in slash */
                if(*p4 == '/')
                {
                    p2 = p4+1;
                    p3 = p4;
                    while(p3 > cmd && !isspace(*p3))
                    {
                        p3--;
                    }
                    if(isspace(*p3))
                    {
                        p3++;
                    }
                    /* and remove the head of the path from the word (keep the tail) */
                    while((*p3++ = *p2++))
                    {
                        ;
                    }
                    /* don't repeat operation for that same word */
                    if(!rptword)
                    {
                        while(p4 > cmd && !isspace(*p4))
                        {
                            p4--;     /* skip to start of word */
                        }
                    }
                }
                /* repeat the operation for every word in the line */
            } while(global && p4 > cmd);
            repeat = 0;
            break;
            
        case 'r':           /* remove file extension */
            /*
             * TODO: this works, but starts with the last word. we should start
             *       processing with the first word, not the last.
             */
            p4 = p2+strlen(p2)-1;
            do
            {
                /* skip to end of last word */
                while(p4 > cmd && isspace(*p4))
                {
                    p4--;
                }
                if(*p4 == '.')
                {
                    p4--;
                }
                while(p4 > cmd && *p4 != '.')
                {
                    if(isspace(*p4))
                    {
                        break;     /* word finished */
                    }
                    p4--;
                }
                if(*p4 == '.')
                {
                    p2 = p4;
                    p3 = p4;
                    while(*p3 && !isspace(*p3))
                    {
                        p3++;
                    }
                    /* remove the extension */
                    while((*p2++ = *p3++))
                    {
                        ;
                    }
                    p4--;
                    /* don't repeat operation for that same word */
                    if(!rptword)
                    {
                        while(p4 > cmd && !isspace(*p4))
                        {
                            p4--;     /* skip to start of word */
                        }
                    }
                }
                /* repeat the operation for every word in the line */
            } while(global && p4 > cmd);
            repeat = 0;
            break;
            
        case 'e':           /* remove file name, keep extension */
            /*
             * TODO: this works, but starts with the last word. we should start
             *       processing with the first word, not the last.
             */
            p4 = p2+strlen(p2)-1;
            do
            {
                while(p4 > cmd && isspace(*p4))
                {
                    p4--;     /* skip to end of last word */
                }
                if(*p4 == '.')
                {
                    p4--;
                }
                while(p4 > cmd && *p4 != '.')
                {
                    if(isspace(*p4))
                    {
                        break;     /* word finished */
                    }
                    p4--;
                }
                if(*p4 == '.')
                {
                    p2 = p4+1;
                    p3 = p4;
                    while(p3 > cmd && !isspace(*p3))
                    {
                        p3--;
                    }
                    if(isspace(*p3))
                    {
                        p3++;
                    }
                    /* remove the filename (keep the extension) */
                    while((*p3++ = *p2++))
                    {
                        ;
                    }
                    /* don't repeat operation for that same word */
                    if(!rptword)
                    {
                        while(p4 > cmd && !isspace(*p4))
                        {
                            p4--;     /* skip to start of word */
                        }
                    }
                }
                /* repeat the operation for every word in the line */
            } while(global && p4 > cmd);
            repeat = 0;
            break;
            
        case 'p':           /* print but don't execute */
            printf("%s\n", cmd);
            errexp = 1;
            return 0;
            
        case 'l':           /* lowercase the 1st uppercase letter -- csh */
            p4 = p2;
            do
            {
                while(*p4 && isspace(*p4))
                {
                    p4++;     /* skip to start of next word */
                }
                /* find and replace the first uppercase letter */
                while(*p4)
                {
                    if(*p4 >= 'A' && *p4 <= 'Z')
                    {
                        *p4 = (*p4)+32;
                        /* don't repeat operation for that same word */
                        if(!rptword)
                        {
                            while(*p4 && !isspace(*p4))
                            {
                                p4++;     /* skip to end of word */
                            }
                            break;
                        }
                    }
                    else if(isspace(*p4))
                    {
                        break;
                    }
                    p4++;
                }
                /* repeat the operation for every word in the line */
            } while(global && *p4);
            repeat = 0;
            break;
            
        case 'L':           /* lowercase all uppercase letters -- our extension, equivalent to ':agl' */
            while(*p2)
            {
                if(*p2 >= 'A' && *p2 <= 'Z')
                {
                    *p2 = (*p2)+32;
                }
                p2++;
            }
            repeat = 0;
            break;
            
        case 'u':           /* uppercase the 1st lowercase letter -- csh */
            p4 = p2;
            do
            {
                while(*p4 && isspace(*p4))
                {
                    p4++;     /* skip to start of next word */
                }
                /* find and replace the first lowercase letter */
                while(*p4)
                {
                    if(*p4 >= 'a' && *p4 <= 'z')
                    {
                        *p4 = (*p4)-32;
                        /* don't repeat operation for that same word */
                        if(!rptword)
                        {
                            while(*p4 && !isspace(*p4))
                            {
                                p4++;     /* skip to end of word */
                            }
                            break;
                        }
                    }
                    else if(isspace(*p4))
                    {
                        break;
                    }
                    p4++;
                }
                /* repeat the operation for every word in the line */
            } while(global && *p4);
            repeat = 0;
            break;
            
        case 'U':           /* uppercase all lowercase letters -- our extension, equivalent to ':agu' */
            while(*p2)
            {
                if(*p2 >= 'a' && *p2 <= 'z')
                {
                    *p2 = (*p2)-32;
                }
                p2++;
            }
            repeat = 0;
            break;
            
        case 'x':           /* quote expansion letters and the words themselves */
            quote_all = 1;
            /* fall through to the next case */
            __attribute__((fallthrough));
            
        case 'q':           /* quote expansion letters to prevent further history substitution */
            n = 0;
            /* count the chars we'll need to escape */
            while(*p2)
            {
                /* whitespace chars */
                if(isspace(*p2) && quote_all)
                {
                    while(*p2 && isspace(*p2))
                    {
                        p2++;
                    }
                    n += 2;
                    continue;
                }
                /* single-quoted substring */
                else if(*p2 == '\'')
                {
                    while(*p2 && *p2 != '\'')
                    {
                        p2++;
                    }
                    if(!*p2)
                    {
                        break;
                    }
                }
                /* backslash-quoted char */
                else if(*p2 == '\\')
                {
                    p2++;
                }
                /* special chars ! (hist expansion char) and : (expansion modifier char) */
                else if(*p2 == '!' || *p2 == ':')
                {
                    n++;
                }
                p2++;
            }
            if(quote_all)
            {
                n += 2;   /* account for the final word in the line */
            }
            /* get a new buffer */
            p = malloc(strlen(cmd)+n+1);
            if(!p)
            {
                errexp = 1;
                PRINT_ERROR("%s: failed to apply modifier: %s\n", SHELL_NAME, strerror(errno));
                if(cmd && cmd != origcmd)
                {
                    free(cmd);
                }
                return 0;
            }
            /* now copy the word, escaping it as appropriate */
            p2 = cmd;
            p3 = p;
            while(*p2)
            {
                if(isspace(*p2) && quote_all)
                {
                    if(p2 != cmd)
                    {
                        *p++ = '"';
                    }
                    while(*p2 && isspace(*p2))
                    {
                        *p++ = *p2++;
                    }
                    *p++ = '"';
                    continue;
                }
                else if(*p2 == '\'')
                {
                    while(*p2 && *p2 != '\'')
                    {
                        *p++ = *p2++;
                    }
                    if(!*p2)
                    {
                        break;
                    }
                }
                else if(*p2 == '\\')
                {
                    *p++ = *p2++;
                }
                else if(*p2 == '!' || *p2 == ':')
                {
                    *p++ = '\\';
                    *p++ = *p2;
                }
                else
                {
                    *p++ = *p2;
                }
                p2++;
            }
            if(quote_all)
            {
                *p++ = '"';
            }
            *p = '\0';
            if(cmd != origcmd)
            {
                free(cmd);
            }
            cmd = p3;
            repeat = 0;
            break;
            
        case 'g':           /* apply modifier once to each word */
            global = 1;
            goto loop;
            
        case 'a':           /* apply modifier multiple times to one word */
            rptword = 1;
            goto loop;
            
        case 'G':           /* apply 's' to each word (must be followed by 's') */
            global = 1;
            hmod++;
            if(*hmod != 's')
            {
                errexp = 1;
                PRINT_ERROR("%s: missing 's' after modifier: %c\n", SHELL_NAME, *p);
                if(cmd && cmd != origcmd)
                {
                    free(cmd);
                }
                return 0;
            }
            if(!repeat)
            {
                i++;
            }
            /* fall through to the next case */
            __attribute__((fallthrough));

        case 's':           /* search & replace. it takes the form: 's/old/new/' */
            hmod++;
            if(*hmod != '/')
            {
                goto invalid_s;
            }
            hmod++;
            /* find the second slash, knowing that slashes might be escaped in the string */
            p = strchr(hmod, '/');
            while(p && p[-1] == '\\')
            {
                p = strchr(p+1, '/');
            }
            if(!p)
            {
                goto invalid_s;
            }
            /* get the 'old' string */
            j = p-hmod;
            char *oldstr = get_malloced_strl(hmod, 0, j);
            if(!oldstr)
            {
                goto invalid_s;
            }
            /* find the third slash, knowing that slashes might be escaped in the string */
            p = strchr(p+1, '/');
            while(p && p[-1] == '\\')
            {
                p = strchr(p+1, '/');
            }
            /* the last slash might be omitted if its the last char in line */
            if(!p)
            {
                p = hmod+strlen(hmod);
            }
            /* get the 'new' string */
            j++;
            char *newstr = get_malloced_strl(hmod, j, p-hmod-j);
            if(!newstr)
            {
                free(oldstr);
                goto invalid_s;
            }
            if(!repeat)
            {
                i += (p-hmod)+1;
            }
            j = strlen(oldstr);
            do
            {
                p = strstr(cmd, oldstr);
                if(!p)
                {
                    break;
                }
                n = p-cmd;
                p2 = substitute_str(cmd, newstr, n, n+j-1);
                if(!p2)
                {
                    break;
                }
                if(cmd != origcmd)
                {
                    free(cmd);
                }
                cmd = p2;
            } while(global);
            free(oldstr);
            free(newstr);
            repeat = 0;
            break;
            
        case '&':           /* repeat last operation */
            repeat = 1;
            hmod -= 2;
            while(hmod >= orighmod && *hmod != ':')
            {
                hmod--;
            }
            if(*hmod == ':')
            {
                hmod--;
            }
            if(hmod < orighmod)
            {
                errexp = 1;
                PRINT_ERROR("%s: invalid application of the '&' modifier\n", SHELL_NAME);
                if(cmd && cmd != origcmd)
                {
                    free(cmd);
                }
                return 0;
            }
            i++;
            break;            
    }
    /* check the next modifier, if any */
    global = 0;
    rptword = 0;
    if(!repeat)
    {
        i++;
    }
    if(*++hmod == ':')
    {
        if(repeat && hmod[1] == '&')
        {
            repeat = 0;
            if(hmod[2] == ':')
            {
                hmod += 2;
                goto loop;
            }
        }
        else
        {
            goto loop;
        }
    }
    /* finished. return the expanded words */
    if(cmd == origcmd)
    {
        cmd = get_malloced_str(cmd);
    }
    *res = cmd;
    return i;
    
invalid_s:
    errexp = 1;
    PRINT_ERROR("%s: invalid usage of the 's' modifier\n", SHELL_NAME);
    if(cmd && cmd != origcmd)
    {
        free(cmd);
    }
    return 0;
}


/*
 * get a pointer to the first char of the n-th word of the command line 'cmd',
 * where words are 0-based.. n == 0 returns the zeroth word, n == -1 returns
 * the the last word in the line.
 *
 * returns a pointer to the word, or NULL in case of error.
 */
char *get_word_start(char *cmd, int n)
{
    /* skip any leading spaces */
    while(*cmd && isspace(*cmd))
    {
        cmd++;
    }
    /* word #0 */
    if(n == 0)
    {
        return cmd;
    }
    /* last word */
    if(n == -1)
    {
        char *cmd2 = cmd;
        cmd += strlen(cmd)-1;
        /* skip trailing spaces */
        while(cmd > cmd2 &&  isspace(*cmd))
        {
            cmd--;
        }
        /* skip to start of word */
        while(cmd > cmd2 && !isspace(*cmd))
        {
            cmd--;
        }
        if(cmd == cmd2)
        {
            return NULL;
        }
        if(isspace(*cmd))
        {
            cmd++;
        }
        return cmd;
    }
    /* word #n */
    while(n--)
    {
        if(!*cmd)
        {
            return NULL;
        }
        /* skip word n-1 */
        while(*cmd && !isspace(*cmd))
        {
            cmd++;
        }
        /* skip spaces */
        while(*cmd &&  isspace(*cmd))
        {
            cmd++;
        }
    }
    return cmd;
}


/*
 * return an malloc'd copy of the word that starts string str.
 * the word contains all characters upto the first whitespace char.
 */
char *get_malloced_word(char *str)
{
    if(!str)
    {
        return NULL;
    }
    char *p = str;
    while(*p && !isspace(*p))
    {
        p++;
    }
    return get_malloced_strl(str, 0, p-str);
}
