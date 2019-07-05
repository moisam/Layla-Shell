/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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
int   errexp = 0;

/* defined in cmdline.c */
extern char *incomplete_cmd;

/* defined in history.c */
extern struct histent_s cmd_history[];


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
        while(*p && *p != '\'') p++;
        if(*p == '\'') p++;
        /* reached EOL? */
        if(!*p) return NULL;
    }

    /*
     * this is a backup copy of the command buffer. we are going to restore
     * the command buffer after we finish our history expansion. this is done
     * so that cmdline() can do its own thing, like erasing the previous command
     * from the screen before outputting our expanded version, for example.
     */
    char *backup = malloc(cmdbuf_end+1);
    if(!backup)
    {
        beep();
        fprintf(stderr, "%s: history expansion failed: %s\r\n", SHELL_NAME, strerror(errno));
        return INVALID_HIST_EXPAND;
    }
    strcpy(backup, cmdbuf);

    quotes = 0;
    while(*p)
    {
        p2 = NULL;
        p3 = NULL;
        switch(*p)
        {
            case '\\':
                p++;
                break;
                
            case '\'':
                p++;
                while(*p && *p != '\'') p++;
                /*
                 * reached EOL? go back one step, so that p++ later will return p to 
                 * EOL so that the loop will end.
                 */
                if(!*p) p--;
                break;
                
            case '`':
            case '"':
                quotes = *p;
                break;
                
            case '^':                           /* ^string1^string2^ */
                p2 = get_hist_cmd(-1);
                if(!p2)
                {
                    fprintf(stderr, "%s: history command not found: !!\r\n", SHELL_NAME);
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
                    fprintf(stderr, "%s: failed to expand history at '^': %s\r\n", SHELL_NAME, strerror(errno));
                    goto bailout;
                }
                strcpy(modif, ":s/");
                p3 = p+1;
                p4 = modif+3;
                while(j-- && *p3)
                {
                    if(*p3 == '^' && p3[-1] != '\\') *p4 = '/';
                    else *p4 = *p3;
                    p3++;
                    p4++;
                }
                *p4 = '\0';                
                j = do_hist_modif(p2, modif, &p4);
                free(modif);
                if(errexp)
                {
                    goto bailout;
                }
                i = insert_hist_cmd(p, p4, j);
                if(p4) free(p4);
                p2 = NULL;
                if(i) p += i-1;
                else
                {
                    fprintf(stderr, "%s: failed to expand history at '^'\r\n", SHELL_NAME);
                    goto bailout;
                }
                expanded = 1;
                break;
                        
            case '!':
                /* backslash-escaped '!' */
                if(p > cmdbuf && p[-1] == '\\') break;
                /* '!' just before closing '"' is not considered for expansion (bash) */
                if(p[1] == '"' && quotes == '"') break;
                /*
                 * also don't consider '!' followed by whitespace, '=' or '(' (bash with
                 * extglob shopt option, csh)
                 */
                if(isspace(p[1]) || p[1] == '\0' || p[1] == '=' || p[1] == '(') break;
                p2 = NULL, p3 = NULL, p4 = NULL;
                errmsg[0] = '\0';
                /* get prev command */
                switch(p[1])
                {
                    case '!':                           /* !! */
                        p2 = get_hist_cmd(-1);
                        if(!p2)
                        {
                            fprintf(stderr, "%s: history command not found: !!\r\n", SHELL_NAME);
                            goto bailout;
                        }
                        j = get_hist_words(p2, p+2, &p3);
                        if(errexp) goto bailout;
                        j += 2;
                        sprintf(errmsg, "%s: history command not found: !!", SHELL_NAME);
                        break;
                        
                    case '-':                           /* !-n */
                        p2 = p+2;
                        if(!*p2 || !isdigit(*p2))
                        {
                            fprintf(stderr, "%s: missing numeric argument to !-\r\n", SHELL_NAME);
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
                        if(errexp) goto bailout;
                        j += i;
                        sprintf(errmsg, "%s: history command not found: !%d", SHELL_NAME, k);
                        break;
                        
                    case '?':                           /* !?string[?] */
                        p2 = p+2;
                        if(!*p2 || *p2 == '?')
                        {
                            fprintf(stderr, "%s: missing string argument to !?\r\n", SHELL_NAME);
                            goto bailout;
                        }
                        j = 0;
                        while(*p2 && *p2 != '?' && !isspace(*p2))
                        {
                            p2++;
                            j++;
                        }
                        p3 = get_malloced_strl(p+2, 0, j);
                        if(!p3)
                        {
                            fprintf(stderr, "%s: history expansion failed: %s\r\n", SHELL_NAME, strerror(errno));
                            goto bailout;
                        }
                        k = 2;                  /* we will add 2 for the '!?' */
                        if(*p2 == '?') k++;     /* and another 1 for the terminating '?' */
                        j += k;
                        p2 = get_hist_cmdp(p3, 0);
                        if(query_str) free(query_str);
                        query_str = p3;
                        i = get_hist_words(p2, p+j, &p3);
                        if(errexp) goto bailout;
                        j += i;
                        sprintf(errmsg, "%s: history command not found: !?%s", SHELL_NAME, p3);
                        break;
                        
                    case '#':                           /* !# */
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
                        p2 = get_hist_cmd(-1);
                        if(!p2)
                        {
                            fprintf(stderr, "%s: history command not found: !!\r\n", SHELL_NAME);
                            goto bailout;
                        }
                        p4 = ":$";
                        j = get_hist_words(p2, p4, &p3);
                        if(errexp) goto bailout;
                        j = 2;
                        sprintf(errmsg, "%s: history command not found: !!", SHELL_NAME);
                        break;
                        
                    default:
                        if(isdigit(p[1]))               /* !n */
                        {
                            p2 = p+1;
                            i = 0, j = 0;
                            while(*p2 && isdigit(*p2))
                            {
                                i = (i*10) + (*p2)-'0';
                                p2++;
                                j++;
                            }
                            k = i;
                            p2 = get_hist_cmd(k);
                            if(!p2)
                            {
                                fprintf(stderr, "%s: history command not found: !%d\r\n", SHELL_NAME, k);
                                goto bailout;
                            }
                            j++;                        /* add 1 for the '!' */
                            i = get_hist_words(p2, p+j, &p3);
                            if(errexp) goto bailout;
                            j += i;
                            sprintf(errmsg, "%s: history command not found: !%d", SHELL_NAME, k);
                        }
                        else if(isalpha(p[1]))          /* !string */
                        {
                            p2 = p+1;
                            j = 0;
                            while(*p2 && !isspace(*p2) && *p2 != ':')
                            {
                                p2++;
                                j++;
                            }
                            p3 = get_malloced_strl(p+1, 0, j);
                            if(!p3)
                            {
                                fprintf(stderr, "%s: history expansion failed: %s\r\n", SHELL_NAME, strerror(errno));
                                goto bailout;
                            }
                            p2 = get_hist_cmdp(p3, 1);
                            j++;                        /* add 1 for the '!' */
                            sprintf(errmsg, "%s: history command not found: !%s", SHELL_NAME, p3);
                            free(p3);
                            i = get_hist_words(p2, p+j, &p3);
                            if(errexp) goto bailout;
                            j += i;
                        }
                        break;
                }

                if(!p2) break;
                if(!p3) p2 = get_malloced_str(p2);      /* so we don't modify the original cmd */
                j += do_hist_modif(p3 ? : p2, p+j, &p4);
                if(errexp)
                {
                    if(p3) free_malloced_str(p3);
                    else   free_malloced_str(p2);
                    goto bailout;
                }
                if(p4)
                {
                    if(p3) free_malloced_str(p3);
                    p3 = p4;
                }
                if(p3) p2 = p3;
                i = insert_hist_cmd(p, p2, j);
                if(p3) free_malloced_str(p3);
                else   free_malloced_str(p2);
                if(!errexp) p += i-1;
                else
                {
                    fprintf(stderr, "%s\r\n", errmsg);
                    goto bailout;
                }
                expanded = 1;
                break;
        }
        p++;
    }
    
    if(expanded)
    {
        p = get_malloced_str(cmdbuf);
    }
    else p = NULL;
    strcpy(cmdbuf, backup);
    free(backup);
    return p;
    
bailout:
    /* restore the original command to buffer */
    strcpy(cmdbuf, backup);
    free(backup);
    return INVALID_HIST_EXPAND;
}


/*
 * insert s at the start of p, replacing n chars.
 * returns the number of characters inserted.
 */
int insert_hist_cmd(char *p, char *s, int n)
{
    errexp = 0;
    if(!s) return errexp = 1, 0;
    int slen = strlen(s);
    //if(!slen) return 0;
    char *p1, *p2, *p3;
    int plen = strlen(p);
    int res = slen;
    if(slen > n)
    {
        p1 = p+n;
        p2 = p1+slen-n;
        p[plen+slen-n] = '\0';
        /* make some room */
        while((*p2++ = *p1++)) ;
    }
    else if(slen < n)
    {
        p1 = p+slen;
        p2 = p+n;
        p[plen-n+slen] = '\0';
        while((*p1++ = *p2++)) ;
    }
    p1 = p;
    /* now insert the new string */
    p2 = s;
    p3 = s+slen;
    while(p2 < p3) *p1++ = *p2++;
    return res;
}


/*
 * get the history command at index, which is 1-based (that is, the first history
 * command is number 1, the last is number cmd_history_end).
 */
char *get_hist_cmd(int index)
{
    if(index == 0) return NULL;
    else if(index < 0)
    {
        /* -1 refers to prev command */
        index += cmd_history_end;
        if(index < 0 || index >= cmd_history_end) return NULL;
        return cmd_history[index].cmd;
    }
    else
    {
        if(index > cmd_history_end) return NULL;
        return cmd_history[index-1].cmd;
    }
}


/*
 * get the history command that contains s. if anchor is non-zero, the history
 * command must start with s (the !string expansion). otherwise, s can occur
 * anywhere in the string (the !?string[?] expansion).
 */
char *get_hist_cmdp(char *s, int anchor)
{
    if(!s) return NULL;
    if(cmd_history_end == 0) return NULL;
    int i = cmd_history_index;
    char *p, *cmd;
    for( ; i >= 0; i--)
    {
        cmd = cmd_history[i].cmd;
        if(!cmd) continue;
        if((p = strstr(cmd, s)))
        {
            if(anchor)
            {
                if(p == cmd) return cmd;
            }
            else return cmd;
        }
    }
    return NULL;
}

/*
 * get the words specified by the wdesig word designator. these typically
 * start with a colon, which can be omitted if the disgnator starts with
 * a ‘^’, ‘$’, ‘*’, ‘-’, or ‘%’. words are counted from zero.
 * cmd contains the full command line from which we will select words.
 * the resultant malloc'd string will be placed in *res.
 * returns the number of characters in the word designator, with *res being 
 * set to NULL if expansion failed.
 */
int get_hist_words(char *cmd, char *wdesig, char **res)
{
    *res = NULL;
    errexp = 0;
    if(!cmd || !wdesig || !*wdesig) return 0;
    int i = 0, n, j;
    if(*wdesig == ':') i++, wdesig++;
    if(*wdesig != '^' && *wdesig != '$' && *wdesig != '*' && *wdesig != '-' && *wdesig != '%')
         return 0;
    
    char *p, *p2;
    switch(*wdesig)
    {
        case '^':           /* the first arg (word #1) */
            p = get_word_start(cmd, 1);
            if(!p) return *res = get_malloced_str(""), i+1;
            p = get_malloced_word(p);
            if(p) return *res = p, i+1;
            break;
            
        case '$':           /* the last arg */
            p = get_word_start(cmd, -1);
            if(!p) return *res = get_malloced_str(""), i+1;
            p = get_malloced_word(p);
            if(p) return *res = p, i+1;
            break;

        case '*':           /* all args (starting w/ word #1) */
            p = get_word_start(cmd, 1);
            if(!p) return *res = get_malloced_str(""), i+1;
            p = get_malloced_str(p);
            if(p) return *res = p, i+1;
            break;
            
        case '%':           /* word matching last !?string[?] search */
            if(!query_str || !(p = strstr(cmd, query_str)))
            {
                fprintf(stderr, "%s: no match found for word: %s\r\n", SHELL_NAME, query_str ? : "(null)");
                errexp = 1;
                return i+1;
            }
            /* find end of word */
            p2 = p;
            while(*p && !isspace(*p)) p++;
            j = p2-cmd;
            p = get_malloced_strl(cmd, j, p-cmd-j);
            if(p) return *res = p, i+1;
            break;
            
        case '-':           /* -y is a shorthand for 0-y */
            if(!isdigit(*++wdesig))
            {
                fprintf(stderr, "%s: invalid word index: %c\r\n", SHELL_NAME, *wdesig);
                errexp = 1;
                return i;
            }
            n = 0, j = 0;
            while(*wdesig && isdigit(*wdesig))
            {
                n = (n*10) + (*wdesig)-'0';
                wdesig++;
                j++;
            }
            i += j+1;         /* add length of y and 1 for '-' */
            p2 = get_word_start(cmd, 0);
            if(!p2) return *res = get_malloced_str(""), i;
            p = get_word_start(cmd, n);     /* get start of the second word */
            if(!p)
            {
                fprintf(stderr, "%s: invalid word index: %d\r\n", SHELL_NAME, n);
                errexp = 1;
                return i;
            }
            /* get to end of word */
            while(*p && !isspace(*p)) p++;
            j = p2-cmd;
            p = get_malloced_strl(cmd, j, p-cmd-j);
            if(p) return *res = p, i;     /* length of "'-' + y" */
            break;
            
        default:
            if(isdigit(*wdesig))               /* n-th word */
            {
                n = 0, j = 0;
                while(*wdesig && isdigit(*wdesig))
                {
                    n = (n*10) + (*wdesig)-'0';
                    wdesig++;
                    j++;
                }
                p = get_word_start(cmd, n);
                if(!p)
                {
                    fprintf(stderr, "%s: invalid word index: %d\r\n", SHELL_NAME, n);
                    errexp = 1;
                    return j+i;
                }
                /* check for x-y word ranges */
                if(*wdesig == '-')
                {
                    wdesig++;
                    if(isdigit(*wdesig))
                    {
                        i += j+1;       /* add length of x and 1 for '-' */
                        n = 0, j = 0;
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
                            fprintf(stderr, "%s: invalid word index: %d\r\n", SHELL_NAME, n);
                            errexp = 1;
                            return i;
                        }
                        /* get to end of word */
                        while(*p && !isspace(*p)) p++;
                        j = p2-cmd;
                        p = get_malloced_strl(cmd, j, p-cmd-j);
                        if(p) return *res = p, i;     /* length of "':' + x + '-' + y" */
                    }
                    else                    /* 'x-' abbrev. 'x-$' without the last word */
                    {
                        i += j+1;       /* add length of x and 1 for '-' */
                        p2 = p;         /* save index of first word */
                        p = get_word_start(cmd, -1);
                        if(!p) return i;
                        /* get to start of word */
                        if(!isspace(*p)) p--;
                        while(p >= cmd && isspace(*p)) p--;
                        /* only one word in the line? */
                        if(p < cmd) return i;
                        j = p2-cmd;
                        p = get_malloced_strl(cmd, j, p-cmd-j+1);
                        if(p) return *res = p, i;     /* length of "':' + x + '-'" */
                    }
                }
                else if(*wdesig == '*')     /* 'x*' abbrev. 'x-$' */
                {
                    wdesig++;
                    p = get_malloced_str(p);
                    if(p) return *res = p, j+i+1;   /* length of ': + word + *' */
                }
                else
                {
                    /* return just the word */
                    p = get_malloced_word(p);
                    if(p) return *res = p, j+i;     /* length of ': + word' */
                }
            }
    }
    return i;
}

/*
 * perform the history expansion modifiers indicated in hmod.
 * modifiers start with a colon and include one letter out of the
 * following: h, t, r, e, p, q, x, s, &, g, a, G.
 * cmd contains the command line (or words selected by a previous call to
 * get_hist_words(), on which we will apply the requested modifiers.
 * the resultant malloc'd string will be placed in *res.
 * returns the number of characters in the modifier, with *res being 
 * set to NULL if expansion failed.
 */
int do_hist_modif(char *cmd, char *hmod, char **res)
{
    *res = NULL;
    errexp = 0;
    if(!cmd || !hmod || !*hmod) return 0;
    if(*hmod != ':') return 0;
    static char chars[] = "htrepqxs&gaGuUlL";
    char *p, *p2, *p3, *p4;
    char *origcmd = cmd;
    char *orighmod = hmod;
    int i = 0, n, j;
    int quote_all;          /* the 'q' and 'x' modifiers */
    int repeat = 0;         /* the '&' modifier */
    int global = 0;         /* the 'g' modifier */
    int rptword = 0;        /* the 'a' modifier */
    
loop:
    hmod++;
    if(!repeat) i++;
    p = chars;
    while(*p)
    {
        if(*p == *hmod) break;
        p++;
    }
    if(!*p)
    {
        errexp = 1;
        fprintf(stderr, "%s: unknown modifier letter: %c\r\n", SHELL_NAME, *hmod);
        if(cmd && cmd != origcmd) free(cmd);
        return 0;       /* unknown modifier letter */
    }

    p2 = cmd;
    quote_all = 0;
    switch(*p)
    {
        case 'h':           /* keep path head only */
            p4 = p2;
            do
            {
                while(*p4 && isspace(*p4)) p4++;     /* skip to start of next word */
                if(*p4 == '/') p4++;
                while(*p4 && *p4 != '/')
                {
                    if(isspace(*p4)) break;     /* word finished */
                    p4++;
                }
                if(*p4 == '/')
                {
                    p2 = p4;
                    p3 = p2;
                    while(*p3 && !isspace(*p3)) p3++;
                    while((*p2++ = *p3++)) ;
                    if(!rptword)
                    {
                        while(*p4 && !isspace(*p4)) p4++;     /* skip to end of word */
                    }
                }
            } while(global && *p4);
            repeat = 0;
            break;
            
        case 't':           /* keep path tail only */
            p4 = p2+strlen(p2)-1;
            do
            {
                while(p4 > cmd && isspace(*p4)) p4--;     /* skip to end of last word */
                if(*p4 == '/') p4--;
                while(p4 > cmd && *p4 != '/')
                {
                    if(isspace(*p4)) break;     /* word finished */
                    p4--;
                }
                if(*p4 == '/')
                {
                    p2 = p4+1;
                    p3 = p4;
                    while(p3 > cmd && !isspace(*p3)) p3--;
                    if(isspace(*p3)) p3++;
                    while((*p3++ = *p2++)) ;
                    if(!rptword)
                    {
                        while(p4 > cmd && !isspace(*p4)) p4--;     /* skip to start of word */
                    }
                }
            } while(global && p4 > cmd);
            repeat = 0;
            break;
            
        case 'r':           /* remove file extension */
            /*
             * TODO: this works, but starts with the last word. should start processing
             *       with the first word, not the last.
             */
            p4 = p2+strlen(p2)-1;
            do
            {
                while(p4 > cmd && isspace(*p4)) p4--;     /* skip to end of last word */
                if(*p4 == '.') p4--;
                while(p4 > cmd && *p4 != '.')
                {
                    if(isspace(*p4)) break;     /* word finished */
                    p4--;
                }
                if(*p4 == '.')
                {
                    p2 = p4;
                    p3 = p4;
                    while(*p3 && !isspace(*p3)) p3++;
                    while((*p2++ = *p3++)) ;
                    p4--;
                    if(!rptword)
                    {
                        while(p4 > cmd && !isspace(*p4)) p4--;     /* skip to start of word */
                    }
                }
            } while(global && p4 > cmd);
            /*
            p2 = strrchr(cmd, '.');
            if(!p2) break;
            p3 = p2;
            while(*p3 && !isspace(*p3)) p3++;
            while((*p2++ = *p3++)) ;
            */
            repeat = 0;
            break;
            
        case 'e':           /* remove file name, keep extension */
            /*
             * TODO: this works, but starts with the last word. should start processing
             *       with the first word, not the last.
             */
            p4 = p2+strlen(p2)-1;
            do
            {
                while(p4 > cmd && isspace(*p4)) p4--;     /* skip to end of last word */
                if(*p4 == '.') p4--;
                while(p4 > cmd && *p4 != '.')
                {
                    if(isspace(*p4)) break;     /* word finished */
                    p4--;
                }
                if(*p4 == '.')
                {
                    p2 = p4+1;
                    p3 = p4;
                    while(p3 > cmd && !isspace(*p3)) p3--;
                    if(isspace(*p3)) p3++;
                    while((*p3++ = *p2++)) ;
                    if(!rptword)
                    {
                        while(p4 > cmd && !isspace(*p4)) p4--;     /* skip to start of word */
                    }
                }
            } while(global && p4 > cmd);
            /*
            p2 = strrchr(cmd, '.');
            if(!p2) break;
            p3 = cmd;
            while((*p3++ = *p2++)) ;
            */
            repeat = 0;
            break;
            
        case 'p':           /* print but don't execute */
            printf("%s\r\n", cmd);
            errexp = 1;
            return 0;
            
        case 'l':           /* lowercase the 1st uppercase letter -- csh */
            p4 = p2;
            do
            {
                while(*p4 && isspace(*p4)) p4++;     /* skip to start of next word */
                while(*p4)
                {
                    if(*p4 >= 'A' && *p4 <= 'Z')
                    {
                        *p4 = (*p4)+32;
                        if(!rptword)
                        {
                            while(*p4 && !isspace(*p4)) p4++;     /* skip to end of word */
                            break;
                        }
                    }
                    else if(isspace(*p4)) break;
                    p4++;
                }
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
                while(*p4 && isspace(*p4)) p4++;     /* skip to start of next word */
                while(*p4)
                {
                    if(*p4 >= 'a' && *p4 <= 'z')
                    {
                        *p4 = (*p4)-32;
                        if(!rptword)
                        {
                            while(*p4 && !isspace(*p4)) p4++;     /* skip to end of word */
                            break;
                        }
                    }
                    else if(isspace(*p4)) break;
                    p4++;
                }
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
            
        case 'q':           /* quote expansion letters to prevent further history substitution */
            n = 0;
            /* count the chars we'll need to escape */
            while(*p2)
            {
                if(isspace(*p2) && quote_all)
                {
                    while(*p2 && isspace(*p2)) p2++;
                    n += 2;
                    continue;
                }
                else if(*p2 == '\'')
                {
                    while(*p2 && *p2 != '\'') p2++;
                    if(!*p2) break;
                }
                else if(*p2 == '\\') p2++;
                else if(*p2 == '!' || *p2 == ':') n++;
                p2++;
            }
            if(quote_all) n += 2;   /* account for the final word in line */
            /* get a new buffer */
            p = malloc(strlen(cmd)+n+1);
            if(!p)
            {
                errexp = 1;
                fprintf(stderr, "%s: failed to apply modifier: %s\r\n", SHELL_NAME, strerror(errno));
                if(cmd && cmd != origcmd) free(cmd);
                return 0;
            }
            /* now copy the word, escaping it as appropriate */
            p2 = cmd;
            p3 = p;
            while(*p2)
            {
                if(isspace(*p2) && quote_all)
                {
                    if(p2 != cmd) *p++ = '"';
                    while(*p2 && isspace(*p2)) *p++ = *p2++;
                    *p++ = '"';
                    continue;
                }
                else if(*p2 == '\'')
                {
                    while(*p2 && *p2 != '\'') *p++ = *p2++;
                    if(!*p2) break;
                }
                else if(*p2 == '\\') *p++ = *p2++;
                else if(*p2 == '!' || *p2 == ':')
                {
                    *p++ = '\\';
                    *p++ = *p2;
                }
                else *p++ = *p2;
                p2++;
            }
            if(quote_all) *p++ = '"';
            *p = '\0';
            if(cmd != origcmd) free(cmd);
            cmd = p3;
            repeat = 0;
            break;
            
        case 'g':           /* apply modifier once to each word */
            global = 1;
            goto loop;
            
        case 'a':           /* apply modifier multiple times to one word */
            rptword = 1;
            goto loop;
            
        case 'G':           /*  */
            global = 1;
            hmod++;
            if(*hmod != 's')
            {
                errexp = 1;
                fprintf(stderr, "%s: missing 's' after modifier: %c\r\n", SHELL_NAME, *p);
                if(cmd && cmd != origcmd) free(cmd);
                return 0;
            }
            if(!repeat) i++;

        case 's':           /* search & replace. it takes the form: 's/old/new/' */
            hmod++;
            if(*hmod != '/') goto invalid_s;
            hmod++;
            /* find the second slash, knowing that slashes might be escaped in the string */
            p = strchr(hmod, '/');
            while(p && p[-1] == '\\')
            {
                p = strchr(p+1, '/');
            }
            if(!p) goto invalid_s;
            /* get the 'old' string */
            j = p-hmod;
            char *oldstr = get_malloced_strl(hmod, 0, j);
            if(!oldstr) goto invalid_s;
            /* find the third slash, knowing that slashes might be escaped in the string */
            p = strchr(p+1, '/');
            while(p && p[-1] == '\\')
            {
                p = strchr(p+1, '/');
            }
            /* the last slash might be omitted if its the last char in line */
            if(!p) p = hmod+strlen(hmod);
            /* get the 'new' string */
            j++;
            char *newstr = get_malloced_strl(hmod, j, p-hmod-j);
            if(!newstr)
            {
                free(oldstr);
                goto invalid_s;
            }
            if(!repeat) i += (p-hmod)+1;
            j = strlen(oldstr);
            do
            {
                p = strstr(cmd, oldstr);
                if(!p) break;
                n = p-cmd;
                p2 = __substitute(cmd, newstr, n, n+j-1);
                if(!p2) break;
                if(cmd != origcmd) free(cmd);
                cmd = p2;
            } while(global);
            free(oldstr);
            free(newstr);
            repeat = 0;
            break;
            
        case '&':           /* repeat last operation */
            repeat = 1;
            hmod -= 2;
            while(hmod >= orighmod && *hmod != ':') hmod--;
            if(*hmod == ':') hmod--;
            if(hmod < orighmod)
            {
                errexp = 1;
                fprintf(stderr, "%s: invalid application of the '&' modifier\r\n", SHELL_NAME);
                if(cmd && cmd != origcmd) free(cmd);
                return 0;
            }
            i++;
            break;            
    }    
    global = 0;
    rptword = 0;
    if(!repeat) i++;
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
        else goto loop;
    }
    
    if(cmd == origcmd) cmd = get_malloced_str(cmd);
    *res = cmd;
    return i;
    
invalid_s:
    errexp = 1;
    fprintf(stderr, "%s: invalid usage of the 's' modifier\r\n", SHELL_NAME);
    if(cmd && cmd != origcmd) free(cmd);
    return 0;
}

char *get_word_start(char *cmd, int n)
{
    /* skip any leading spaces */
    while(*cmd && isspace(*cmd)) cmd++;
    /* word #0 */
    if(n == 0) return cmd;
    /* last word */
    if(n == -1)
    {
        char *cmd2 = cmd;
        cmd += strlen(cmd)-1;
        /* skip trailing spaces */
        while(cmd > cmd2 &&  isspace(*cmd)) cmd--;
        /* skip to start of word */
        while(cmd > cmd2 && !isspace(*cmd)) cmd--;
        if(cmd == cmd2) return NULL;
        if(isspace(*cmd)) cmd++;
        return cmd;
    }
    /* word #n */
    while(n--)
    {
        if(!*cmd) return NULL;
        /* skip word n-1 */
        while(*cmd && !isspace(*cmd)) cmd++;
        /* skip spaces */
        while(*cmd &&  isspace(*cmd)) cmd++;
    }
    return cmd;
}

char *get_malloced_word(char *str)
{
    if(!str) return NULL;
    char *p = str;
    while(*p && !isspace(*p)) p++;
    return get_malloced_strl(str, 0, p-str);
}
