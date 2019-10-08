/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: vi.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include "cmd.h"
#include "vi.h"
#include "kbdevent.h"
#include "scanner/scanner.h"
#include "scanner/source.h"
#include "parser/parser.h"
#include "parser/node.h"
#include "backend/backend.h"
#include "debug.h"

/* defined in cmdline.c */
// extern char     *cmdbuf      ;
// extern uint16_t  cmdbuf_index;
// extern uint16_t  cmdbuf_end  ;
// extern uint16_t  cmdbuf_size ;
extern long      CMD_BUF_SIZE;
extern int       terminal_row;
extern int       terminal_col;
extern int       VGA_WIDTH   ;
extern int       VGA_HEIGHT  ;
extern int       start_row   ;
extern int       start_col   ;
extern int       insert      ;

/* saved copies of the current row, column, and command buffer index */
static int srow, scol, scmdindex;
/* flag to indicate if we are in the INSERT mode */
int   sinsert = 0;
/* last search string used */
char *lstring = NULL;
/* backup copy of the command buffer */
char *backup  = NULL;


/*
 * search history list for command containing string 'buf'.
 * if hook is > 0, search is limited to commands starting with
 * search string. if back is > 0, search is done backwards
 * (towards first history entry), otherwise is done forwards
 * (towards last history entry).
 * 
 * returns the index of the command history line containing the
 * search string, -1 if no matches were found.
 */
int search_history(char *buf, int hook, int back)
{
    int i;
    char *s;
    if(back)
    {
        /* search backwards */
        if(cmd_history_index == 0)
        {
            /* already at the first history entry */
            return -1;
        }
        /* perform the search */
        for(i = cmd_history_index-1; i >= 0; i--)
        {
            if((s = strstr(cmd_history[i].cmd, buf)) == NULL)
            {
                continue;
            }
            if(hook)
            {
                if(s == cmd_history[i].cmd)
                {
                    return i;
                }
                else
                {
                    continue;
                }
            }
            return i;
        }
    }
    else
    {
        /* search forwards */
        if(cmd_history_index >= cmd_history_end-1)
        {
            /* already at the last history entry */
            return -1;
        }
        /* perform the search */
        for(i = cmd_history_index+1; i < cmd_history_end; i++)
        {
            if((s = strstr(cmd_history[i].cmd, buf)) == NULL)
            {
                continue;
            }
            if(hook)
            {
                if(s == cmd_history[i].cmd)
                {
                    return i;
                }
                else
                {
                    continue;
                }
            }
            return i;
        }
    }
    /* no matches found */
    return -1;
}


/*
 * find the next occurence of char c in the command buffer.
 * parameter c should be 't' or 'f' to indicate forward search.
 * if char c is found, move the cursor to that position, otherwise
 * do nothing.
 */
void find_next(char c, char c2, int count)
{
    int count2;
    switch(c)
    {
        case 't':
        case 'f':                       /* find next char */
            count2 = cmdbuf_index+1;
            /* reached the end of buffer */
            if(cmdbuf_index >= cmdbuf_end)
            {
                break;
            }
            /* search for the char */
            while(cmdbuf[count2] != c2 && count2 < cmdbuf_end)
            {
                count2++;
            }
            if(cmdbuf[count2] == c2)
            {
                count2 -= cmdbuf_index;
                do_right_key(count2);
            }
            /* t is equal to f-h */
            if(c == 't')
            {
                if(!count)
                {
                    do_left_key(1);
                }
                else
                {
                    do_left_key(count);
                }
            }
            break;
    }
}


/*
 * find the previous occurence of char c in the command buffer.
 * parameter c should be 'T' or 'F' to indicate backward search.
 * if char c is found, move the cursor to that position, otherwise
 * do nothing.
 */
void find_prev(char c, char c2, int count)
{
    int count2;
    switch(c)
    {
        case 'T':
        case 'F':                       /* find prev char */
            count2 = cmdbuf_index-1;
            /* reached the start of buffer */
            if(cmdbuf_index <= 0)
            {
                break;
            }
            /* search for the char */
            while(cmdbuf[count2] != c2 && count2 > 0)
            {
                count2--;
            }
            if(cmdbuf[count2] == c2)
            {
                count2 = cmdbuf_index-count2;
                do_left_key(count2);
            }
            /* T is equal to F-l */
            if(c == 'T')
            {
                if(!count)
                {
                    do_left_key(1);
                }
                else
                {
                    do_left_key(count);
                }
            }
            break;
    }
}

/*
 * find the boundaries of the current word (under the cursor).
 * returns NULL if the cursor is on a blank char, otherwise the
 * same buf pointer is returned with the word copied in buf.
 * start and end point to the start and end of word in the
 * original cmdbuf.
 */
char *get_curword(char *buf, int *start, int *end)
{
    char c = cmdbuf[cmdbuf_index];
    if(isspace(c) || c == '\0')
    {
        return NULL;
    }
    if(!buf)
    {
        return NULL;
    }
    int i = cmdbuf_index;
    int j = i;
    /* skip chars to determine the beginning of the word */
    while(!isspace(cmdbuf[i]) && i > 0)
    {
        i--;
    }
    if(i && isspace(cmdbuf[i]))
    {
        i++;
    }
    /* skip chars to determine the end of the word */
    while(!isspace(cmdbuf[j]) && j < cmdbuf_end)
    {
        j++;
    }
    if(isspace(cmdbuf[j]) || cmdbuf[j] == 0)
    {
        j--;
    }
    /* copy the word to the buffer */
    strncpy(buf, cmdbuf+i, j-i+1);
    buf[j-i+1] = '\0';
    /* save the pointers */
    *start = i;
    *end   = j;
    /* return the word */
    return buf;
}


/*
 * identify brace characters.
 * 
 * returns 1 if char c is a brace character, 0 otherwise.
 */
int isbrace(char c)
{
    if(c == '(' || c == '{' || c == '[' || c == ')' || c == '}' || c == ']')
    {
        return 1;
    }
    return 0;
}


/*
 * find the next brace char.
 */
void find_brace()
{
    int count2;
    count2 = cmdbuf_index+1;
    /* reached the end of buffer */
    if(cmdbuf_index >= cmdbuf_end)
    {
        return;
    }
    /* find the next brace char */
    while(!isbrace(cmdbuf[count2]) && count2 < cmdbuf_end)
    {
        count2++;
    }
    /* if we found a brace, move the cursor to its position */
    if(isbrace(cmdbuf[count2]))
    {
        count2 -= cmdbuf_index;
        do_right_key(count2);
    }
}


/*
 * save the current cursor position.
 */
void save_curpos()
{
    srow = terminal_row;
    scol = terminal_col;
    scmdindex = cmdbuf_index;
}


/*
 * restore the cursor to its saved position.
 */
void restore_curpos()
{
    terminal_row = srow;
    terminal_col = scol;
    cmdbuf_index = scmdindex;
    move_cur(terminal_row, terminal_col);
}


/*
 * insert some string at the cur position.
 */
void insert_at(char *s)
{
    int slen = strlen(s);
    /* zero-length string */
    if(!slen)
    {
        return;
    }
    char *p1, *p2, *p3;
    p1 = cmdbuf+cmdbuf_end;
    p2 = p1+slen;
    p3 = cmdbuf+cmdbuf_index;
    cmdbuf[cmdbuf_end+slen] = '\0';
    /* make some room for the new string */
    while(p1 >= p3)
    {
        *p2-- = *p1--;
    }
    p1++;
    /* now insert the new string */
    p2 = s;
    p3 = s+slen;
    while(p2 < p3)
    {
        *p1++ = *p2++;
    }
    /* print the new command line */
    printf("%s", s);
    save_curpos();
    printf("%s", cmdbuf+cmdbuf_index+slen);
    /* adjust our buffer pointers */
    cmdbuf_end   += slen;
    cmdbuf_index += slen;
    /* move the cursor to where we inserted the new string */
    do_left_key(cmdbuf_end-cmdbuf_index);
}


/*
 * replace the whole buffer contents with another string.
 */
void replace_with(char *s)
{
    clear_cmd(0);
    move_cur(start_row, start_col);
    strcpy(cmdbuf, s);
    cmdbuf_end = strlen(cmdbuf);
    cmdbuf_index = cmdbuf_end;
    output_cmd();
}


/*
 * free our internal buffers.
 */
void free_bufs()
{
    if(backup)
    {
        free(backup);
    }
    if(lstring)
    {
        free_malloced_str(lstring);
    }
    insert = sinsert;
}


/*
 * this function performs the actions of the vi-editing control mode.
 * see the 'vi Line Editing Command Mode' section of this link:
 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/sh.html
 * 
 * returns '\n' or '\r' if the user entered a newline or carriage return,
 * respectively, so that cmdline() would execute the command line.. returns
 * zero otherwise.
 */
int vi_cmode(struct source_s *src)
{
    int  c, c2;
    int  tabs   = 0;
    int  count  = 0, count2;
    char *p, **pp;
    /* last command char and last count */
    char lc     = 0, lc2 = 0;
    int  lcount = 0;
    /* last search string used */
    lstring = NULL;
    
    /* our command buffer */
#define BUFCHARS    127
    char buf[BUFCHARS+1];
    
    /*
     * this is a backup copy of the command buffer, just in case we
     * received a 'U' or 'undo all' command.
     */
    backup = malloc(cmdbuf_end+1);
    if(backup)
    {
        strcpy(backup, cmdbuf);
    }
    /* save the current INSERT mode */
    sinsert = insert;
    
    /* loop to read vi commands */
    while(1)
    {
        /***********************
         * get next key stroke
         ***********************/
        c = get_next_key();
        if(c == 0)
        {
            continue;
        }

        /* count tabs */
        if(c != '\t')
        {
            tabs = 0;
        }
        else
        {
            tabs++;
        }
        
select:
        save_curpos();
        glob_t glob;
        switch(c)
        {
            default:
                beep();
                break;

            /************************
             * the count field
             ************************/
            case '0':
                if(count == 0)  /* move to start of line */
                {
                    count2 = cmdbuf_index;
                    do_home_key();
                    if(lc == 'c')
                    {
                        do_del_key(count2);
                        free_bufs();
                        return 0;
                    }
                    else if(lc == 'd')
                    {
                        do_del_key(count2);
                    }
                    else if(lc == 'y')
                    {
                        yank(0, count2);
                        restore_curpos();
                    }
                    break;
                }
                /* NOTE: fall through to the next case */
                __attribute__((fallthrough));
                
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                count = count*10 + (c-'0');
                break;
                
            /************************
             * Motion Edit Commands
             ************************/
            case HOME_KEY:
            case '^':                       /* move to start of line */
                count2 = cmdbuf_index;
                do_home_key();
                count = 0;
                if(lc == 'c')
                {
                    do_del_key(count2);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_del_key(count2);
                }
                else if(lc == 'y')
                {
                    yank(0, count2);
                    restore_curpos();
                }
                break;
                
            case 'b':                       /* move backward one word */
                c = cmdbuf_index;
                lcount = c;
                if(isspace(cmdbuf[cmdbuf_index]))
                {
                    while(isspace(cmdbuf[c]) && c > 0)
                    {
                        c--;
                    }
                }
                else
                {
                    while(!isspace(cmdbuf[c]) && c > 0)
                    {
                        c--;
                    }
                    if(isspace(cmdbuf[c]))
                    {
                        c++;
                    }
                }
                count = cmdbuf_index-c;
                if(count)
                {
                    do_left_key(count);
                }
                if(lc == 'c')
                {
                    do_del_key(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_del_key(count);
                }
                else if(lc == 'y')
                {
                    yank(lcount, cmdbuf_index);
                    restore_curpos();
                }
                count = 0;
                break;
                
            case 'B':                       /* move to end of prev word */
                c = cmdbuf_index;
                lcount = c;
                if(!isspace(cmdbuf[cmdbuf_index]))
                {
                    while(!isspace(cmdbuf[c]) && c > 0)
                    {
                        c--;
                    }
                }
                while(isspace(cmdbuf[c]) && c > 0)
                {
                    c--;
                }
                count = cmdbuf_index-c;
                if(count)
                {
                    do_left_key(count);
                }
                if(lc == 'c')
                {
                    do_del_key(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_del_key(count);
                }
                else if(lc == 'y')
                {
                    yank(cmdbuf_index, lcount);
                    restore_curpos();
                }
                count = 0;
                break;

            case 'E':
            case 'e':                       /* move to end of word */
                c2 = cmdbuf_index;
                lcount = c2;
                if(isspace(cmdbuf[cmdbuf_index]))
                {
                    while(isspace(cmdbuf[c2]) && c2 < cmdbuf_end)
                    {
                        c2++;
                    }
                }
                while(!isspace(cmdbuf[c2]) && c2 < cmdbuf_end)
                {
                    c2++;
                }
                if(c == 'e' && (isspace(cmdbuf[c2]) || cmdbuf[c2] == 0))
                {
                    c2--;
                }
                count = c2-cmdbuf_index;
                if(count)
                {
                    do_right_key(count);
                }
                if(lc == 'c')
                {
                    do_backspace(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_backspace(count);
                }
                else if(lc == 'y')
                {
                    yank(lcount, cmdbuf_index);
                    restore_curpos();
                }
                count = 0;
                break;
                
            case LEFT_KEY:
                count = 1;
                /* NOTE: fall through to the next case */
                __attribute__((fallthrough));
                
            case 'h':                       /* move backward one char */
                if(!count)
                {
                    count = 1;
                }
                do_left_key(count);
                if(lc == 'c')
                {
                    do_del_key(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_del_key(count);
                }
                else if(lc == 'y')
                {
                    yank(cmdbuf_index, cmdbuf_index+count);
                    restore_curpos();
                }
                count = 0;
                break;
                
            case RIGHT_KEY:
                count = 1;
                /* NOTE: fall through to the next case */
                __attribute__((fallthrough));
                
            case 'l':                       /* move forward one char */
                if(!count)
                {
                    count = 1;
                }
                do_right_key(count);
                if(lc == 'c')
                {
                    do_backspace(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_backspace(count);
                }
                else if(lc == 'y')
                {
                    yank(cmdbuf_index-count, cmdbuf_index);
                    restore_curpos();
                }
                count = 0;
                break;
            
            case 't':
            case 'f':                       /* find next char */
                c2 = get_next_key();
                if(c2 == '\e')              /* ESC key */
                {
                    count = 0;
                    break;
                }
                lcount = cmdbuf_index;
                find_next(c, c2, count);
                if(lc == 'y')
                {
                    yank(lcount, cmdbuf_index);
                    restore_curpos();
                }
                else if(lc == 'c')
                {
                    do_backspace(cmdbuf_index-lcount);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_backspace(cmdbuf_index-lcount);
                }
                lc = c;
                lc2 = c2;
                lcount = count;
                count = 0;
                break;
                
            case 'T':
            case 'F':                       /* find prev char */
                c2 = get_next_key();
                if(c2 == '\e')              /* ESC key */
                {
                    count = 0;
                    break;
                }
                lcount = cmdbuf_index;
                find_prev(c, c2, count);
                if(lc == 'y')
                {
                    yank(cmdbuf_index, lcount);
                    restore_curpos();
                }
                else if(lc == 'c')
                {
                    do_del_key(lcount-cmdbuf_index);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_del_key(lcount-cmdbuf_index);
                }
                lc = c;
                lc2 = c2;
                lcount = count;
                count = 0;
                break;
                
            case 'w':                       /* move forward one word */
                c = cmdbuf_index;
                lcount = c;
                if(isspace(cmdbuf[cmdbuf_index]))
                {
                    while(isspace(cmdbuf[c]) && c < cmdbuf_end)
                    {
                        c++;
                    }
                }
                else
                {
                    while(!isspace(cmdbuf[c]) && c < cmdbuf_end)
                    {
                        c++;
                    }
                    if(isspace(cmdbuf[c]) || cmdbuf[c] == 0)
                    {
                        c--;
                    }
                }
                count = c-cmdbuf_index;
                if(count)
                {
                    do_right_key(count);
                }
                if(lc == 'c')
                {
                    do_backspace(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_backspace(count);
                }
                else if(lc == 'y')
                {
                    yank(lcount, cmdbuf_index);
                    restore_curpos();
                }
                count = 0;
                break;
                
            case 'W':                       /* move to beginning of next word */
                c = cmdbuf_index;
                lcount = c;
                if(!isspace(cmdbuf[cmdbuf_index]))
                {
                    while(!isspace(cmdbuf[c]) && c < cmdbuf_end)
                    {
                        c++;
                    }
                }
                while(isspace(cmdbuf[c]) && c < cmdbuf_end)
                {
                    c++;
                }
                count = c-cmdbuf_index;
                if(count)
                {
                    do_right_key(count);
                }
                if(lc == 'c')
                {
                    do_backspace(count);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_backspace(count);
                }
                else if(lc == 'y')
                {
                    yank(lcount, cmdbuf_index);
                    restore_curpos();
                }
                count = 0;
                break;

            case ';':           /* repeat last search command */
                if(count == 0)
                {
                    count = 1;
                }
                while(count--)
                {
                    if(lc == 't' || lc == 'f')
                    {
                        find_next(lc, lc2, lcount);
                    }
                    else if(lc == 'T' || lc == 'F')
                    {
                        find_prev(lc, lc2, lcount);
                    }
                    else
                    {
                        break;
                    }
                }
                count = 0;
                break;

            case ',':           /* repeat last search command inversed */
                if(count == 0)
                {
                    count = 1;
                }
                /* inverse the last search command */
                if(lc == 't')
                {
                    c = 'T';
                }
                else if(lc == 'T')
                {
                    c = 't';
                }
                else if(lc == 'f')
                {
                    c = 'F';
                }
                else if(lc == 'F')
                {
                    c = 'f';
                }
                /* now perform the command */
                while(count--)
                {
                    if(c == 't' || c == 'f')
                    {
                        find_next(c, lc2, lcount);
                    }
                    else if(c == 'T' || c == 'F')
                    {
                        find_prev(c, lc2, lcount);
                    }
                    else
                    {
                        break;
                    }
                }
                count = 0;
                break;

            case '|':                       /* move to column 'count' */
                count--;
                if(count < 0)
                {
                    count = 0;
                }
                else if(count > cmdbuf_end)
                {
                    count = cmdbuf_end;
                }
                /* move left or right? */
                if(count < cmdbuf_index)
                {
                    do_left_key(cmdbuf_index-count);
                    if(lc == 'c')
                    {
                        do_del_key(cmdbuf_index-count);
                        free_bufs();
                        return 0;
                    }
                    else if(lc == 'd')
                    {
                        do_del_key(cmdbuf_index-count);
                    }
                    else if(lc == 'y')
                    {
                        yank(cmdbuf_index, cmdbuf_index+count);
                        restore_curpos();
                    }
                }
                else if(count > cmdbuf_index)
                {
                    do_right_key(count-cmdbuf_index);
                    if(lc == 'c')
                    {
                        do_backspace(count-cmdbuf_index);
                        free_bufs();
                        return 0;
                    }
                    else if(lc == 'd')
                    {
                        do_backspace(count-cmdbuf_index);
                    }
                    else if(lc == 'y')
                    {
                        yank(cmdbuf_index-count, cmdbuf_index);
                        restore_curpos();
                    }
                }
                count = 0;
                break;
                
            case END_KEY:
            case '$':                       /* move to end of line */
                count2 = cmdbuf_index;
                do_end_key();
                if(lc == 'c')
                {
                    do_backspace(cmdbuf_index-count2);
                    free_bufs();
                    return 0;
                }
                else if(lc == 'd')
                {
                    do_backspace(cmdbuf_index-count2);
                }
                else if(lc == 'y')
                {
                    yank(count2, cmdbuf_index);
                    restore_curpos();
                }
                count = 0;
                break;
                
            case '%':                       /* move to balancing/first brace */
                c = cmdbuf[cmdbuf_index];
                switch(c)
                {
                    case '(': find_next('f', ')', 0); break;
                    case '{': find_next('f', '}', 0); break;
                    case '[': find_next('f', ']', 0); break;
                    case ')': find_prev('F', '(', 0); break;
                    case '}': find_prev('F', '{', 0); break;
                    case ']': find_prev('F', '[', 0); break;
                    default : find_brace()          ; break;
                }
                count = 0;
                break;
                
            case '[':                   /* POSIX vi commands that begin with '[' */
                c = get_next_key();
                switch(c)
                {
                    case 'A':                       /* the fetch prev command */
                        if(!count)
                        {
                            count = 1;
                        }
                        do_up_key(count);
                        break;
                        
                    case 'B':                       /* the fetch next command */
                        if(!count)
                        {
                            count = 1;
                        }
                        do_down_key(count);
                        break;
                        
                    case 'C':               /* move forward one char */
                        if(!count)
                        {
                            count = 1;
                        }
                        do_right_key(count);
                        if(lc == 'c')
                        {
                            do_backspace(count);
                            free_bufs();
                            return 0;
                        }
                        else if(lc == 'd')
                        {
                            do_backspace(count);
                        }
                        else if(lc == 'y')
                        {
                            yank(cmdbuf_index-count, cmdbuf_index);
                            restore_curpos();
                        }
                        break;

                    case 'D':               /* move backward one char */
                        if(!count)
                        {
                            count = 1;
                        }
                        do_left_key (count);
                        if(lc == 'c')
                        {
                            do_del_key(count);
                            free_bufs();
                            return 0;
                        }
                        else if(lc == 'd')
                        {
                            do_del_key(count);
                        }
                        else if(lc == 'y')
                        {
                            yank(cmdbuf_index, cmdbuf_index+count);
                            restore_curpos();
                        }
                        break;
                        
                    case 'H':               /* move to first non-blank character */
                        c = 0;
                        if(isspace(cmdbuf[0]))
                        {
                            while(isspace(cmdbuf[c]) && c < cmdbuf_end)
                            {
                                c++;
                            }
                        }
                        count = cmdbuf_index;
                        cmdbuf_index = c;
                        terminal_col = start_col+c;
                        terminal_row = start_row;
                        move_cur(terminal_row, terminal_col);
                        if(lc == 'c')
                        {
                            do_del_key(count-cmdbuf_index);
                            free_bufs();
                            return 0;
                        }
                        else if(lc == 'd')
                        {
                            do_del_key(count-cmdbuf_index);
                        }
                        else if(lc == 'y')
                        {
                            yank(cmdbuf_index, count);
                            restore_curpos();
                        }
                        break;
                        
                    case 'Y':               /* move to end of line */
                        count2 = cmdbuf_index;
                        do_end_key();
                        if(lc == 'c')
                        {
                            do_backspace(cmdbuf_index-count2);
                            free_bufs();
                            return 0;
                        }
                        else if(lc == 'd')
                        {
                            do_backspace(cmdbuf_index-count2);
                        }
                        else if(lc == 'y')
                        {
                            yank(count2, cmdbuf_index);
                            restore_curpos();
                        }
                        break;
                }
                count = 0;
                break;
                
            case UP_KEY:
                do_up_key(1);
                break;
                
            case DOWN_KEY:
                do_down_key(1);
                break;
                
            /************************
             * Search Edit Commands
             ************************/
            case '+':                      /* the fetch next command */
            case 'j':
                if(!count)
                {
                    count = 1;
                }
                do_down_key(count);
                break;
                
            case '-':                       /* the fetch prev command */
            case 'k':
                if(!count)
                {
                    count = 1;
                }
                do_up_key(count);
                break;

            case 'G':                       /* fetch command # count */
                if(!count)
                {
                    count = -1;
                }
                else
                {
                    count -= cmd_history_index;
                }
                /* move up or down? */
                if(count < 0)
                {
                    do_up_key  (-count);
                }
                else if(count > 0)
                {
                    do_down_key( count);
                }
                count = 0;
                break;
                
            case 'n':               /* search history backwards using the last search string */
                if(!lstring)
                {
                    break;
                }
                count = search_history(lstring, 0, 1);
                if(count == -1)                         /* no match found */
                {
                    beep();
                    break;
                }
                /* get prev command */
                count -= cmd_history_index;
                do_up_key(-count);
                count = 0;
                break;
                
            case 'N':               /* search history forwards using the last search string */
                if(!lstring)
                {
                    break;
                }
                count = search_history(lstring, 0, 0);
                if(count == -1)                         /* no match found */
                {
                    beep();
                    break;
                }
                /* get next command */
                count -= cmd_history_index;
                do_down_key(count);
                count = 0;
                break;
                
            case '/':                       /* search history backwards */
                count = 0;
                c = get_next_key();
                if(c == '\e')               /* ESC key */
                {
                    beep();
                    break;
                }
                if(c == '^')
                {
                    count2 = 1;    /* hook search to start of line */
                }
                else
                {
                    buf[count++] = c, count2 = 0;
                }
                while(count < BUFCHARS)
                {
                    c = get_next_key();
                    if(c == '\n' || c == '\r')
                    {
                        break;
                    }
                    buf[count++] = c;
                }
                buf[count] = '\0';
                if(count == 0)
                {
                    strcpy(buf, lstring);    /* use prev string */
                }
                count = search_history(buf, count2, 1);
                if(count == -1)                         /* no match found */
                {
                    beep();
                    break;
                }
                /* get prev command */
                count -= cmd_history_index;
                do_up_key(-count);
                count = 0;
                if(lstring)
                {
                    free_malloced_str(lstring);
                }
                lstring = get_malloced_str(buf);
                break;
                
            case '?':                       /* search history forwards */
                count = 0;
                c = get_next_key();
                if(c == '\e')               /* ESC key */
                {
                    beep();
                    break;
                }
                if(c == '^')
                {
                    count2 = 1;    /* hook search to start of line */
                }
                else
                {
                    buf[count++] = c, count2 = 0;
                }
                while(count < BUFCHARS)
                {
                    c = get_next_key();
                    if(c == '\n' || c == '\r')
                    {
                        break;
                    }
                    buf[count++] = c;
                }
                buf[count] = '\0';
                if(count == 0)
                {
                    strcpy(buf, lstring);    /* use prev string */
                }
                count = search_history(buf, count2, 0);
                if(count == -1)                         /* no match found */
                {
                    beep();
                    break;
                }
                /* get next command */
                count -= cmd_history_index;
                do_down_key(count);
                count = 0;
                if(lstring)
                {
                    free_malloced_str(lstring);
                }
                lstring = get_malloced_str(buf);
                break;

            /**********************************
             * Text Modification Edit Commands
             **********************************/
            case 'a':                           /* append here and return to input mode */
                do_right_key(1);
                free_bufs();
                return 0;
                
            case 'A':                           /* append at EOL and return to input mode */
                do_end_key();
                free_bufs();
                return 0;
                
            case 'c':                           /* can be of the form: [count] c motion
                                                 *                 or: c [count] motion
                                                 * either deletes chars from current to where the
                                                 * cursor moves with 'motion'.
                                                 */
                if(lc == 'c')   /* we have 'cc' - delete entire line */
                {
                    do_kill_key();
                    free_bufs();
                    return 0;
                }
                lc = 'c';
                break;
                
            case 'C':                           /* delete from here to EOL and return to input mode */
                clear_cmd(cmdbuf_index);
                cmdbuf[cmdbuf_index] = '\0';
                cmdbuf_end = cmdbuf_index;
                restore_curpos();
                free_bufs();
                return 0;
                
            case 'S':                           /* equivalent to 'cc' */
                do_kill_key();
                free_bufs();
                return 0;
                
            case 's':                           /* replace count chars under the cursor in input mode */
                if(!count)
                {
                    count = 1;
                }
                if(cmdbuf_index+count > cmdbuf_end)
                {
                    count = cmdbuf_end-cmdbuf_index;
                }
                do_del_key(count);
                free_bufs();
                return 0;
                
            case 'd':                           /* similar to 'c' without entering input mode */
                if(lc == 'd')   /* we have 'dd' - delete entire line */
                {
                     do_kill_key();
                     lc = 0;
                }
                else
                {
                    lc = 'd';
                }
                break;
                
            case 'D':                           /* similar to 'C' without entering input mode */
                clear_cmd(cmdbuf_index);
                cmdbuf[cmdbuf_index] = '\0';
                cmdbuf_end = cmdbuf_index;
                count = 0;
                lc = 'D';
                break;
                
            case 'i':                           /* enter input mode */
                free_bufs();
                return 0;
                
            case 'I':                           /* enter input mode at start of line */
                do_home_key();
                free_bufs();
                return 0;
                
            case 'P':                           /* insert save buffer before cur position */
                if(!count)
                {
                    count = 1;
                }
                lcount = count;
                while(count--)
                {
                    insert_at(savebuf);
                }
                lc = 'P';
                break;
                
            case 'p':                           /* insert save buffer after cur position */
                if(!count)
                {
                    count = 1;
                }
                lcount = count;
                while(count--)
                {
                    do_right_key(1);
                    insert_at(savebuf);
                }
                lc = 'p';
                break;
                
            case 'R':                           /* enter input mode with replace/insert chars */
                insert = 1;
                free_bufs();
                return 0;
                
            case 'r':                           /* replace count chars with c */
                c = get_next_key();
                if(c == '\e')                   /* ESC key */
                {
                    beep();
                    break;
                }
                if(!count)
                {
                    count = 1;
                }
                lcount = count;
                while(count--)
                {
                    if(cmdbuf_index == cmdbuf_end)
                    {
                        beep();          /* ring a bell */
                        break;
                    }
                    cmdbuf[cmdbuf_index++] = c;
                    putchar(c);
                }
                update_row_col();
                lc = 'r';
                break;

            case 'u':
                /*
                 * TODO: 'u' should only undo the last text modifying command.
                 *       what we do here is undo ALL commands, which is what 'U' does.
                 *       fix this.
                 */
                
            case 'U':
                if(!backup)
                {
                    break;
                }
                replace_with(backup);
                break;
                
            case 'V':                           /* print special fc command in buffer */
#define VSTR    "fc -e ${VISUAL:-${EDITOR:-vi}}"
                clear_cmd(0);
                move_cur(start_row, start_col);
                if(!count)      /* use cur line */
                {
                    char b[cmdbuf_end+1];
                    strcpy(b, cmdbuf);
                    snprintf(cmdbuf, CMD_BUF_SIZE, "%s %s", VSTR, b);
                }
                else            /* use count */
                {
                    snprintf(cmdbuf, CMD_BUF_SIZE, "%s %d", VSTR, count);
                }
                cmdbuf_end = strlen(cmdbuf);
                cmdbuf_index = cmdbuf_end;
                output_cmd();
                break;
                
            case 'x':                           /* del cur char */
                do_del_key(1);
                lc = 'x';
                count = 0;
                break;

            case 'X':                           /* del prev char */
                do_backspace(1);
                lc = 'X';
                count = 0;
                break;
                
            case 'y':                           /* yank (copy) chars to the save buffer */
                if(lc == 'y')   /* we have 'yy' - yank entire line */
                {
                     yank(0, cmdbuf_end);
                     lc = 0;
                }
                else
                {
                    lc = 'y';
                }
                break;
                
            case 'Y':                           /* yank (copy) chars from cur cursor to EOL */
                yank(cmdbuf_index, cmdbuf_end);
                lc = 'Y';
                break;
                
            case '_':                           /* append 'count' word from prev and enter input mode */
                if(cmd_history_index <= 0)
                {
                    beep();
                    break;
                }
                p = cmd_history[cmd_history_index-1].cmd;
                count2 = strlen(p);
                if(count)
                {
                    /* find the count-th word */
                    c = 0;
                    c2 = 0;
                    while(count--)
                    {
                        /* find start of word */
                        while( isspace(p[c]) && c < count2)
                        {
                            c++;
                        }
                        c2 = c;
                        /* find end of word */
                        while(!isspace(p[c]) && c < count2)
                        {
                            c++;
                        }
                    }
                    strncpy(buf, p+c2, c-c2);
                    buf[c-c2] = '\0';
                }
                else
                {
                    /* use the last word */
                    c = count2-1;
                    /* find end of word */
                    while( isspace(p[c]) && c > 0)
                    {
                        c--;
                    }
                    c2 = c+1;
                    /* find start of word */
                    while(!isspace(p[c]) && c > 0)
                    {
                        c--;
                    }
                    if(c)
                    {
                        c++;
                    }
                    strncpy(buf, p+c, c2-c);
                    buf[c2-c] = '\0';
                }
                /* POSIX says to insert a space first */
                if(buf[0])
                {
                    do_end_key();
                    do_insert(' ');
                    /* then the word */
                    insert_at(buf);
                }
                /* then enter input mode */
                free_bufs();
                return 0;
                
            case '.':                           /* repeat prev text modification command */
                c = lc;
                count = lcount;
                goto select;
                
            case '~':                           /* invert chars case */
                if(!count)
                {
                    count = 1;
                }
                lcount = count;
                while(count--)
                {
                    if(cmdbuf_index == cmdbuf_end)
                    {
                        beep();          /* ring a bell */
                        break;
                    }
                    c = cmdbuf[cmdbuf_index];
                    if(c >= 'a' && c <= 'z')
                    {
                        c = c-'a'+'A';
                    }
                    else if(c >= 'A' && c <= 'Z')
                    {
                        c = c-'A'+'a';
                    }
                    cmdbuf[cmdbuf_index++] = c;
                    putchar(c);
                }
                update_row_col();
                lc = '~';
                break;

            case '=':                           /*
                                                 * POSIX says this operator performs word expansions, while
                                                 * ksh says it generates a list of matching commands/file names.
                                                 */
                do_tab(cmdbuf, &cmdbuf_index, &cmdbuf_end, src);
                count = 0;
                break;
                
            case '*':                           /* 
                                                 * command or file name completion (replace all
                                                 * matches as per POSIX).
                                                 */
                if(!(p = get_curword(buf, &c, &c2)))
                {
                    beep();
                    break;
                }
                count = strlen(buf);
                /* assume implicit '*' if no regex chars as per POSIX */
                if(!has_regex_chars(buf, count))
                {
                    buf[count  ] =  '*';
                    buf[count+1] = '\0';
                }
                pp = get_filename_matches(p, &glob);
                if(!pp || !pp[0])
                {
                    globfree(&glob);
                    break;
                }
                p = list_to_str(pp, 0);
                globfree(&glob);
                if(!p)
                {
                    break;
                }
                /* insert the list of matched file/dir names */
                char *p2 = substitute_str(cmdbuf, p, (size_t)c, (size_t)c2);
                if(p2)
                {
                    /* move to start of cur word */
                    do_left_key(cmdbuf_index-c);
                    /* remove the word */
                    do_del_key(count);
                    /* and replace it */
                    insert_at(p2);
                    /* position cursor at end of inserted word */
                    /* now insert the space if it was not a dir */
                    if(p[strlen(p)-1] != '/')
                    {
                        do_insert(' ');
                    }
                    free(p2);
                }
                /* POSIX says we should return to input mode */
                free(p);
                free_bufs();
                return 0;
                
            case '\\':                          /*
                                                 * command or file name completion (replace the longest
                                                 * match as per POSIX).
                                                 */
                if(!(p = get_curword(buf, &c, &c2)))
                {
                    beep();
                    break;
                }
                count = strlen(buf);
                /* assume implicit '*' if no regex chars as per POSIX */
                if(!has_regex_chars(buf, count))
                {
                    buf[count  ] =  '*';
                    buf[count+1] = '\0';
                }
                pp = get_filename_matches(p, &glob);
                if(!pp || !pp[0])
                {
                    globfree(&glob);
                    break;
                }
                /* find the longest match */
                count = 0;
                p = pp[0];
                char *psave = NULL;
                for(count2 = 0; count2 < (int)glob.gl_pathc; count2++)
                {
                    /* save the index of the longest match */
                    int len = strlen(pp[count2]);
                    if(len > count)
                    {
                        count = len;
                        p = get_malloced_str(pp[count2]);
                        psave = p;
                    }
                }
                /* we will need to insert a space after filenames */
                if(p[count-1] != '/')
                {
                    count2 = 1;
                }
                else
                {
                    count2 = 0;
                }
                /* for now, insert the matched file/dir name */
                p = substitute_str(cmdbuf, p, (size_t)c, (size_t)c2);
                if(p)
                {
                    save_curpos();
                    replace_with(p);
                    restore_curpos();
                    do_right_key(count);
                    /* now insert the space if it was not a dir */
                    if(count2)
                    {
                        do_insert(' ');
                    }
                    free(p);
                }
                if(psave)
                {
                    free_malloced_str(psave);
                }
                globfree(&glob);
                /* POSIX says we should return to input mode */
                free_bufs();
                return 0;
                
            case '#':                           /* toggle commented lines */
                count = 0;
                while(isspace(cmdbuf[count]) && count < cmdbuf_end)
                {
                    count++;
                }
                cmdbuf_index = count;
                do_right_key(count);
                if(cmdbuf[count] == '#')
                {
                    while(cmdbuf_index < cmdbuf_end)
                    {
                        if(cmdbuf[cmdbuf_index] == '#')
                        {
                            if(cmdbuf_index == 0 || cmdbuf[cmdbuf_index-1] == '\n')
                            {
                                do_del_key(1);
                                continue;
                            }
                        }
                        cmdbuf_index++;
                        do_right_key(1);
                    }
                    cmdbuf_index = 0;
                    move_cur(start_row, start_col);
                }
                else
                {
                    while(cmdbuf_index < cmdbuf_end)
                    {
                        if(cmdbuf_index == 0 || cmdbuf[cmdbuf_index] == '\n')
                        {
                            if(cmdbuf[cmdbuf_index+1] == '\0')
                            {
                                break;
                            }
                            do_insert('#');
                            continue;
                        }
                        cmdbuf_index++;
                        do_right_key(1);
                    }
                    do_end_key();
                    do_insert('\n');
                    save_to_history(cmdbuf);
                    free_bufs();
                    return '\n';
                }
                count = 0;
                lc = 0;
                break;
                
            case '@':                       /* search for alias name */
                count = 0;
                c = get_next_key();
                if(c == '\e')               /* ESC key */
                {
                    beep();
                    break;
                }
                buf[count++] = c, count2 = 0;
                while(count < BUFCHARS)
                {
                    c = get_next_key();
                    if(c == '\n' || c == '\r')
                    {
                        break;
                    }
                    buf[count++] = c;
                }
                buf[count] = '\0';
                if(count == 0)
                {
                    beep();
                    break;
                }
                char *a = parse_alias(buf);
                if(!a)
                {
                    beep();
                    break;
                }
                replace_with(a);
                break;
                
            /*****************
             * Other Commands
             *****************/
            case '\e':
                beep();
                break;

            case '\f':                          /* ^L - linefeed and print cur line */
                clear_screen();
                print_prompt(src);
                update_row_col();
                start_row = get_terminal_row();
                start_col = get_terminal_col();
                output_cmd();
                cmdbuf_index = cmdbuf_end;
                break;
                
            case '\n':
                free_bufs();
                return '\n';
                
            case '\r':
                free_bufs();
                return '\r';
                
            case CTRLV_KEY:
                printf("\n%s\n", shell_ver);
                print_prompt(src);
                update_row_col();
                start_row = terminal_row;
                start_col = terminal_col;
                output_cmd();
                break;
                
        } /* end switch */
    } /* end while */
}
