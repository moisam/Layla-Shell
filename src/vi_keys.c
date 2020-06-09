/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: vi_keys.c
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
#include "cmd.h"
#include "vi.h"
#include "debug.h"

/* defined in cmdline.c */
extern size_t    CMD_BUF_SIZE;
extern int       start_row   ;
extern int       start_col   ;
extern int       insert      ;

/*
 * Clear the command (in whole or part) that's in the command buffer from the
 * screen. We do this when we are processing some keys (backspace, delete, ^W),
 * and when we want to replace the command in the buffer (such as when the user
 * presses UP or DOWN keys to navigate the history list).
 */
void clear_cmd(int startat)
{
    /* if we'll clear the whole command, move the cursor to the beginning */
    if(!startat)
    {
        move_cur(start_row, start_col);
    }
    char *p = cmdbuf+startat;
    if(!*p)
    {
        return;
    }
    /* print out the characters in the buffer */
    for( ; *p; p++)
    {
        if(*p == '\t' || *p == '\r' || *p == '\n')
        {
            /* print tabs, newlines and carriage returns */
            putc(*p , stdout);
        }
        else
        {
            /* for all other chars, print a space (i.e. clear the char from screen) */
            putc(' ', stdout);
        }
    }
}


/* 
 * Print the command in the command buffer to the screen. We output the
 * command manually so we can calculate how many newlines are there and
 * update the start_row and terminal_row variables correctly.
 */
void output_cmd(void)
{
    update_row_col();
    char *p = cmdbuf;
    while(*p)
    {
        if(*p == '\n')
        {
            if(terminal_row == VGA_HEIGHT)
            {
                start_row--;
            }
            /*
            else
            {
                terminal_row++;
            }
            */
        }
        putchar(*p++);
    }
    update_row_col();
}


/*
 * Insert the given char at the current cursor position.
 */
void do_insert(char c)
{
    /* normal char, add to buffer and print */
    if(cmdbuf_end >= CMD_BUF_SIZE)
    {
        if(!ext_cmdbuf(&cmdbuf, &cmdbuf_size, CMD_BUF_SIZE+1))     /* TODO: we must handle this error */
        {
            return;
        }
    }
    printf("%c" , c);
    
    /* overwrite cur char if we are in the INSERT mode */
    if(insert)
    {
        cmdbuf[cmdbuf_index++] = c;
        /* inserting at the end of the string */
        if(cmdbuf_index > cmdbuf_end)
        {
            /* extend the string */
            cmdbuf_end++;
            cmdbuf[cmdbuf_index+1] = '\0';
        }
        /* update the cursor's positon */
        update_row_col();
        return;
    }
    
    /* normal addition (not in the INSERT mode) */
    if(cmdbuf_index < cmdbuf_end)
    {
        /* make room for the new char */
        size_t u;
        for(u = cmdbuf_end+1; u > cmdbuf_index; u--)
        {
            cmdbuf[u] = cmdbuf[u-1];
        }
        cmdbuf[cmdbuf_end+1] = '\0';
        /* update the cursor's positon */
        update_row_col();
        /* print the string from the current position till the end */
        printf("%s", cmdbuf+cmdbuf_index+1);
        /* add char to buffer */
        cmdbuf[cmdbuf_index] = c;
        /* move the cursor one place to the right */
        move_cur(terminal_row, terminal_col);
    }
    else
    {
        /* add char to buffer */
        cmdbuf[cmdbuf_index  ] = c;
        cmdbuf[cmdbuf_index+1] = '\0';
    }
    
    /* update the buffer pointers */
    cmdbuf_index++;
    cmdbuf_end++;
}


/*
 * Handle the kill key (default is ^U), which clears the current command from
 * the screen and the buffer.
 */
void do_kill_key(void)
{
    if(!cmdbuf_end)
    {
        return;
    }
    clear_cmd(0);
    move_cur(start_row, start_col);
    cmdbuf_end   = 0;
    cmdbuf_index = 0;
    cmdbuf[0]    = '\0';
}


/*
 * Handle the delete key (default is DEL), which removes the character after the
 * cursor. We generalize the case here so that we can indicate how many characters
 * we want deleted. Handling DEL equates to calling do_del_key(1).
 */
void do_del_key(int count)
{
    if(!count)
    {
        return;
    }
    if(cmdbuf_index < cmdbuf_end)
    {
        /*
         * shift characters from cursor till the end of the string 'count' positions
         * to the left.
         */
        size_t u;
        for(u = cmdbuf_index; u <= cmdbuf_end-count; u++)
        {
            cmdbuf[u] = cmdbuf[u+count];
        }
        /* remove excess characters from the string */
        cmdbuf_end -= count;
        cmdbuf[cmdbuf_end] = '\0';
        /* update the cursor's positon */
        update_row_col();
        /* print the new command string, and replace the deleted chars with spaces */
        size_t old_row = terminal_row, old_col = terminal_col;
        printf("%s%*s", cmdbuf+cmdbuf_index, count, " ");
        /* move the cursor back to where it was */
        move_cur(old_row, old_col);
        terminal_row = old_row;
        terminal_col = old_col;
    }
}


/*
 * Handle the backspace key (default is BKSP or ^H), which removes the character before
 * the cursor. We generalize the case here so that we can indicate how many characters
 * we want deleted. Handling BKSP equates to calling do_backspace(1).
 */
void do_backspace(size_t count)
{
    if(!count)
    {
        return;
    }
    update_row_col();
    /* first column of first row, no place to go back */
    if(terminal_col == 1 && terminal_row == 1)
    {
        return;
    }
    /* first char in the buffer, no char to delete */
    if(cmdbuf_index == 0)
    {
        return;
    }
    cmdbuf_index -= count;
    /*
     * shift characters from cursor till the end of the string 'count' positions
     * to the left.
     */
    size_t u;
    for(u = cmdbuf_index; u <= cmdbuf_end-count; u++)
    {
        cmdbuf[u] = cmdbuf[u+count];
    }
    cmdbuf_end -= count;
    cmdbuf[cmdbuf_end] = '\0';
    /* make sure the cursor doesn't point past the new end of the string */
    if(cmdbuf_index > cmdbuf_end)
    {
        cmdbuf_index = cmdbuf_end;
    }
    /* adjust the cursor position */
    for(u = 0; u < count; u++)
    {
        if(terminal_col == 1)
        {
            if(terminal_row == 1)
            {
                break;
            }
            terminal_col = VGA_WIDTH;
            terminal_row--;
        }
        else
        {
            terminal_col--;
        }
    }
    move_cur(terminal_row, terminal_col);
    /* print the new command string, and replace the deleted chars with spaces */
    printf("%s%*s", cmdbuf+cmdbuf_index, (int)count, " ");
    /* move the cursor back to where it was */
    move_cur(terminal_row, terminal_col);
}


/*
 * Handle the UP arrow key, which we usually use to navigate to the previous command in the
 * history list. We generalize the case here so that we can indicate how many positions
 * we want to go backwards in the list. Handling UP equates to calling do_up_key(1).
 */
void do_up_key(int count)
{
    /* we are already at the first command in the history list */
    if(cmd_history_index <= 0)
    {
        return;
    }
    /* remove the current command from the screen */
    clear_cmd(0);
    /* move the cursor to the start of the screen line */
    move_cur(start_row, start_col);
    cmd_history_index -= count;
    /* make sure we don't go past the first command in the history list */
    if(cmd_history_index < 0)
    {
        cmd_history_index = 0;
    }
    /* copy the command to the buffer */
    strcpy(cmdbuf, cmd_history[cmd_history_index].cmd);
    cmdbuf_end = strlen(cmdbuf);
    if(cmdbuf[cmdbuf_end-1] == '\n')
    {
        cmdbuf[cmdbuf_end-1] = '\0';
        cmdbuf_end--;
    }
    /* position the cursor at the end of the command */
    cmdbuf_index = cmdbuf_end;
    /* print the new command */
    output_cmd();
}


/*
 * Handle the DOWN arrow key, which we usually use to navigate to the next command in the
 * history list. We generalize the case here so that we can indicate how many positions
 * we want to go forward in the list. Handling DOWN equates to calling do_down_key(1).
 */
void do_down_key(int count)
{
    /* the command buffer is empty, means we don't have to go down the list */
    if(!cmdbuf_end)
    {
        return;
    }
    /* remove the current command from the screen */
    clear_cmd(0);
    /* move the cursor to the start of the screen line */
    move_cur(start_row, start_col);
    cmd_history_index += count;
    if(cmd_history_index >= cmd_history_end)
    {
        /* we're past the last command in the history list */
        cmd_history_index = cmd_history_end;
        /* clear the command buffer */
        cmdbuf_end   = 0;
        cmdbuf_index = 0;
        cmdbuf[0]    = '\0';
    }
    else
    {
        /* copy the command to the buffer */
        strcpy(cmdbuf, cmd_history[cmd_history_index].cmd);
        cmdbuf_end = strlen(cmdbuf);
        if(cmdbuf[cmdbuf_end-1] == '\n')
        {
            cmdbuf[cmdbuf_end-1] = '\0';
            cmdbuf_end--;
        }
        /* position the cursor at the end of the command */
        cmdbuf_index = cmdbuf_end;
        /* print the new command */
        output_cmd();
    }
}


/*
 * Handle the RIGHT arrow key, which we usually use to navigate to the next character
 * after the cursor. We generalize the case here so that we can indicate how many chars
 * we want to go forward in the line. Handling RIGHT equates to calling do_right_key(1).
 */
void do_right_key(size_t count)
{
    /* we are already at the last char */
    if(cmdbuf_index >= cmdbuf_end)
    {
        return;
    }
    
    /* invalid (zero) count */
    if(!count)
    {
        return;
    }

    /* calculate the new column index and adjust the row index accordingly */
    size_t newcol = terminal_col+count;
    if(newcol > VGA_WIDTH)
    {
        terminal_col = newcol-VGA_WIDTH;
        terminal_row++;
        cmdbuf_index += count;
    }
    else
    {
        terminal_col += count;
        cmdbuf_index += count;
    }
    
    /* move the cursor to the new location */
    move_cur(terminal_row, terminal_col);
}


/*
 * Handle the LEFT arrow key, which we usually use to navigate to the previous character
 * before the cursor. We generalize the case here so that we can indicate how many chars
 * we want to go backward in the line. Handling LEFT equates to calling do_left_key(1).
 */
void do_left_key(int count)
{
    /* we are already at the first char */
    if(cmdbuf_index <= 0)
    {
        return;
    }
    
    /* invalid (zero) count */
    if(!count)
    {
        return;
    }

    /* calculate the new column index and adjust the row index accordingly */
    int newcol = terminal_col-count;
    if(newcol < 1)
    {
        if(terminal_row > 1)
        {
            terminal_col = VGA_WIDTH+newcol;
            terminal_row--;
            cmdbuf_index -= count;
        }
    }
    else
    {
        terminal_col -= count;
        cmdbuf_index -= count;
    }
    /* move the cursor to the new location */
    move_cur(terminal_row, terminal_col);
}


/*
 * Handle the HOME key, which moves the cursor to the beginning of the command line.
 */
void do_home_key(void)
{
    /* we are already at the first char */
    if(cmdbuf_index <= 0)
    {
        return;
    }
    cmdbuf_index = 0;
    /* move the cursor to the new location */
    move_cur(start_row, start_col);
}


/*
 * Handle the END key, which moves the cursor to the end of the command line.
 */
void do_end_key(void)
{
    /* we are already at the last char */
    if(cmdbuf_index >= cmdbuf_end)
    {
        return;
    }
    terminal_row = start_row;
    /* calculate the new column index and adjust the row index accordingly */
    if(cmdbuf_end+start_col >= VGA_WIDTH)
    {
        terminal_row += ((cmdbuf_end+start_col)/VGA_WIDTH);
        terminal_col  = ((cmdbuf_end+start_col)%VGA_WIDTH);
    }
    else
    {
        terminal_col = cmdbuf_end+start_col;
    }
    cmdbuf_index = cmdbuf_end;
    /* move the cursor to the new location */
    move_cur(terminal_row, terminal_col);
}


/*
 * Output a control key, such as ^C or ^V.
 */
void print_ctrl_key(char c)
{
    if(c >= 0 && c < 32)
    {
        putchar('^');
        putchar(c+64);
    }
}

char *savebuf      = NULL;      /* buffer to save a copy of the command line */
int   savebuf_size = 0;         /* size of the save buffer */

/*
 * Copy (or yank) characters from the command buffer to the
 * special save buffer, starting at character number 'start',
 * ending at character number 'end' (indexes are zero-based).
 */
void yank(int start, int end)
{
    /* first time we are called. alloc the buffer */
    if(!savebuf)
    {
        /* get the system-defined maximum line length */
        savebuf_size = get_linemax();
        /* alloc the buffer */
        savebuf = malloc(savebuf_size);
        if(!savebuf)
        {
            PRINT_ERROR("ERROR: insufficient memory for the yank buffer");
            return;
        }
        savebuf[0] = '\0';
    }
    int len = end-start;
    /* zero length, nothing to copy */
    if(len == 0)
    {
        return;
    }
    /* don't exceed buffer length */
    if(len >= savebuf_size)
    {
        len = savebuf_size-1;
    }
    /* copy the command to the buffer */
    memcpy(savebuf, cmdbuf+start, len);
    savebuf[len] = '\0';
}
