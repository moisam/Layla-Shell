/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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
extern char     *cmdbuf      ;
extern uint16_t  cmdbuf_index;
extern uint16_t  cmdbuf_end  ;
extern uint16_t  cmdbuf_size ;
extern long      CMD_BUF_SIZE;
extern int       terminal_row;
extern int       terminal_col;
extern int       VGA_WIDTH   ;
extern int       VGA_HEIGHT  ;
extern int       start_row   ;
extern int       start_col   ;
extern int       insert      ;

void clear_cmd(int startat)
{
    if(!startat) move_cur(start_row, start_col);
    char *p = cmdbuf+startat;
    if(!*p) return;
    for( ; *p; p++)
    {
        if(*p == '\t' || *p == '\r' || *p == '\n')
             putc(*p , stdout);
        else putc(' ', stdout);
    }
}

void output_cmd()
{
    /* 
     * output the command manually so we can calculate how many
     * newlines are there and update the start_row variable correctly.
     */
    update_row_col();
    char *p = cmdbuf;
    while(*p)
    {
        if(*p == '\n')
        {
            if(terminal_row == VGA_HEIGHT) start_row--;
            else terminal_row++;
        }
        putchar(*p++);
    }
}

/*
 * insert char at current position.
 */
void do_insert(char c)
{
    /* normal char, add to buffer and print */
    if(cmdbuf_end >= CMD_BUF_SIZE)
        if(!ext_cmdbuf(CMD_BUF_SIZE+1))     /* TODO: we must handle this error */
            return;
    printf("%c" , c);
    /* overwrite cur char in INSERT mode */
    if(insert)
    {
        cmdbuf[cmdbuf_index++] = c;
        if(cmdbuf_index > cmdbuf_end)
        {
            cmdbuf_end++;
            cmdbuf[cmdbuf_index+1] = '\0';
        }
        update_row_col();
        return;
    }
    /* normal addition, out of the INSERT mode */
    if(cmdbuf_index < cmdbuf_end)
    {
        int u;
        for(u = cmdbuf_end+1; u > cmdbuf_index; u--)
            cmdbuf[u] = cmdbuf[u-1];
        cmdbuf[cmdbuf_end+1] = '\0';
        update_row_col();
        printf("%s", cmdbuf+cmdbuf_index+1);
        /* add char to buffer */
        cmdbuf[cmdbuf_index] = c;
        move_cur(terminal_row, terminal_col);
    }
    else
    {
        /* add char to buffer */
        cmdbuf[cmdbuf_index  ] = c;
        cmdbuf[cmdbuf_index+1] = '\0';
    }
    cmdbuf_index++;
    cmdbuf_end++;
}

void do_kill_key()
{
    if(!cmdbuf_end) return;
    clear_cmd(0);
    move_cur(start_row, start_col);
    cmdbuf_end   = 0;
    cmdbuf_index = 0;
    cmdbuf[0]    = '\0';
}

void do_del_key(int count)
{
    if(!count) return;
    if(cmdbuf_index < cmdbuf_end)
    {
        int u;
        for(u = cmdbuf_index; u <= cmdbuf_end-count; u++)
            cmdbuf[u] = cmdbuf[u+count];
        cmdbuf_end -= count;
        cmdbuf[cmdbuf_end] = '\0';
        update_row_col();
        size_t old_row = terminal_row, old_col = terminal_col;
        printf("%s%*s", cmdbuf+cmdbuf_index, count, " ");
        move_cur(old_row, old_col);
        terminal_row = old_row;
        terminal_col = old_col;
    }
}

void do_backspace(int count)
{
    if(!count) return;
    update_row_col();
    if(terminal_col == 1 && terminal_row == 1) return;
    if(cmdbuf_index == 0) return;
    cmdbuf_index -= count;
    uint16_t u;
    for(u = cmdbuf_index; u <= cmdbuf_end-count; u++) cmdbuf[u] = cmdbuf[u+count];
    cmdbuf_end -= count;
    cmdbuf[cmdbuf_end] = '\0';
    if(cmdbuf_index > cmdbuf_end) cmdbuf_index = cmdbuf_end;
    for(u = 0; u < count; u++)
    {
        if(terminal_col == 1)
        {
            if(terminal_row == 1) break;
            terminal_col = VGA_WIDTH;
            terminal_row--;
        }
        else terminal_col--;
    }
    move_cur(terminal_row, terminal_col);
    printf("%s%*s", cmdbuf+cmdbuf_index, count, " ");
    move_cur(terminal_row, terminal_col);
}

void do_up_key(int count)
{
    if(cmd_history_index <= 0) return;
    clear_cmd(0);
    move_cur(start_row, start_col);
    cmd_history_index -= count;
    if(cmd_history_index < 0) cmd_history_index = 0;
    strcpy(cmdbuf, cmd_history[cmd_history_index].cmd);
    cmdbuf_end = strlen(cmdbuf);
    if(cmdbuf[cmdbuf_end-1] == '\n')
    {
        cmdbuf[cmdbuf_end-1] = '\0';
        cmdbuf_end--;
    }
    cmdbuf_index = cmdbuf_end;
    output_cmd();
}

void do_down_key(int count)
{
    if(!cmdbuf_end) return;
    clear_cmd(0);
    move_cur(start_row, start_col);
    cmd_history_index += count;
    if(cmd_history_index >= cmd_history_end)
    {
        cmd_history_index = cmd_history_end;
        cmdbuf_end = 0; cmdbuf_index = 0;
        cmdbuf[0] = '\0';
    }
    else
    {
        strcpy(cmdbuf, cmd_history[cmd_history_index].cmd);
        cmdbuf_end = strlen(cmdbuf);
        if(cmdbuf[cmdbuf_end-1] == '\n')
        {
            cmdbuf[cmdbuf_end-1] = '\0';
            cmdbuf_end--;
        }
        cmdbuf_index = cmdbuf_end;
        output_cmd();
    }
}

void do_right_key(int count)
{
    
    if(cmdbuf_index >= cmdbuf_end) return;
    if(!count) return;
    update_row_col();
    //if(count > VGA_WIDTH) count = VGA_WIDTH;
    int newcol = terminal_col+count;
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
    move_cur(terminal_row, terminal_col);
}

void do_left_key(int count)
{
    if(cmdbuf_index <= 0) return;
    if(!count) return;
    update_row_col();
    //if(count > VGA_WIDTH) count = VGA_WIDTH;
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
    move_cur(terminal_row, terminal_col);
}

void do_home_key()
{
    if(cmdbuf_index <= 0) return;
    cmdbuf_index = 0;
    move_cur(start_row, start_col);
}

void do_end_key()
{
    if(cmdbuf_index >= cmdbuf_end) return;
    terminal_row = start_row;
    if(cmdbuf_end+start_col >= VGA_WIDTH)
    {
        terminal_row += ((cmdbuf_end+start_col)/VGA_WIDTH);
        terminal_col  = ((cmdbuf_end+start_col)%VGA_WIDTH);
    }
    else terminal_col = cmdbuf_end+start_col;
    cmdbuf_index = cmdbuf_end;
    move_cur(terminal_row, terminal_col);
}

void print_ctrl_key(char c)
{
    if(c >= 0 && c < 32)
    {
        putchar('^');
        putchar(c+64);
    }
}

char *savebuf      = NULL;
int   savebuf_size = 0;

/*
 * copy (or yank) characters from the command buffer to the
 * special save buffer.
 */
void yank(int start, int end)
{
    if(!savebuf)
    {
        savebuf_size = sysconf(_SC_LINE_MAX);
        if(savebuf_size <= 0) savebuf_size = DEFAULT_LINE_MAX;
        savebuf = (char *)malloc(savebuf_size);
        if(!savebuf)
        {
            fprintf(stderr, "FATAL ERROR: Insufficient memory for save buffer");
        }
        savebuf[0] = '\0';
    }
    if(!savebuf) return;
    int len = end-start;
    if(len == 0) return;
    if(len >= savebuf_size) len = savebuf_size-1;
    memcpy(savebuf, cmdbuf+start, len);
    savebuf[len] = '\0';
}

/*
void free_yank_buf()
{
    if(!savebuf) return;
    free(savebuf);
    savebuf = NULL;
}
*/
