/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 *
 *    file: terminal.c
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

#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include "cmd.h"
#include "backend/backend.h"
#include "debug.h"
#include "kbdevent.h"

/****************************************
 *
 * Terminal control/status functions
 *
 ****************************************/

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;
extern struct termios tty_attr;


/*
 * turn the terminal canonical mode on or off.
 */
void term_canon(int on)
{
    if(!isatty(0))
    {
        return;
    }
    if(on)
    {
        tcsetattr(0, TCSANOW, &tty_attr_old);
    }
    else
    {
        tcsetattr(0, TCSANOW, &tty_attr);
    }
}

/* special struct termios for turning echo on/off */
struct termios echo_tty_attr;


/*
 * turn echo on for the given file descriptor.
 *
 * returns 1 on success, 0 on failure.
 */
int echoon(int fd)
{
    /* check fd is a terminal */
    if(!isatty(fd))
    {
        return 0;
    }
    /* get the terminal attributes */
    if(tcgetattr(fd, &echo_tty_attr) == -1)
    {
        return 0;
    }
    /* turn echo on */
    echo_tty_attr.c_lflag |= ECHO;
    /* set the new terminal attributes */
    if((tcsetattr(fd, TCSAFLUSH, &echo_tty_attr) == -1))
    {
        return 0;
    }
    return 1;
}


/*
 * turn echo off for the given file descriptor.
 *
 * returns 1 on success, 0 on failure.
 */
int echooff(int fd)
{
    /* check fd is a terminal */
    if(!isatty(fd))
    {
        return 0;
    }
    /* get the terminal attributes */
    if(tcgetattr(fd, &echo_tty_attr) == -1)
    {
        return 0;
    }
    /* turn echo off */
    echo_tty_attr.c_lflag &= ~ECHO;
    /* set the new terminal attributes */
    if((tcsetattr(fd, TCSAFLUSH, &echo_tty_attr) == -1))
    {
        return 0;
    }
    return 1;
}


/*
 * get the screen size and save the width and height in the $COLUMNS and $LINES
 * shell variables, respectively.
 *
 * returns 1 if the screen size if obtained and saved, 0 otherwise.
 */
int get_screen_size()
{
    struct winsize w;
    /* find the size of the terminal window */
    int fd = isatty(0) ? 0 : isatty(2) ? 2 : -1;
    if(fd == -1)
    {
        return 0;
    }
    int res = ioctl(fd, TIOCGWINSZ, &w);
    if(res != 0)
    {
        return 0;
    }
    VGA_HEIGHT = w.ws_row;
    VGA_WIDTH  = w.ws_col;
    /* update the value of terminal columns in environ and in the symbol table */
    char buf[32];
    struct symtab_entry_s *e = get_symtab_entry("COLUMNS");
    if(!e)
    {
        e = add_to_symtab("COLUMNS");
    }
    if(e && sprintf(buf, "%d", VGA_WIDTH))
    {
        symtab_entry_setval(e, buf);
    }
    /* update the value of terminal rows in environ and in the symbol table */
    e = get_symtab_entry("LINES");
    if(!e)
    {
        e = add_to_symtab("LINES");
    }
    if(e && sprintf(buf, "%d", VGA_HEIGHT))
    {
        symtab_entry_setval(e, buf);
    }
    return 1;
}


/*
 * move the cursor to the given row (line) and column.. both values are 1-based,
 * counting from the top-left corner of the screen.
 */
void move_cur(int row, int col)
{
    fprintf(stdout, "\e[%d;%dH", row, col);
    fflush(stdout);
}


/*
 * clear the screen.
 */
void clear_screen()
{
    fprintf(stdout, "\e[2J");
    fprintf(stdout, "\e[0m");
    fprintf(stdout, "\e[3J\e[1;1H");
}


/*
 * set the text foreground and background color.
 */
void set_terminal_color(int FG, int BG)
{
    /*control sequence to set screen color */
    fprintf(stdout, "\x1b[%d;%dm", FG, BG);
}


/*
 * get the cursor position (current row and column), which are 1-based numbers,
 * counting from the top-left corner of the screen.
 */
void update_row_col()
{
    /*
     * clear the terminal device's EOF flag.. this would have been set, for example,
     * if we used the read builtin to read from the terminal and the user pressed
     * CTRL-D to indicate end of input.. we can't read the cursor position without
     * clearing that flag.
     */
    if(feof(stdin))
    {
        clearerr(stdin);
    }
    /*
     * we will temporarily block SIGCHLD so it won't disturb us between our cursor
     * position request and the terminal's response.
     */
    sigset_t intmask;
    sigemptyset(&intmask);
    sigaddset(&intmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &intmask, NULL);
    /* request terminal cursor position report (CPR) */
    fprintf(stdout, "\x1b[6n");
    /* read the result. it will be reported as:
     *     ESC [ y; x R
     */
    terminal_col = 0;
    terminal_row = 0;
    int c;
    int *in = &terminal_row;
    int delim = ';';
    term_canon(0);
    while((c = getchar()) != EOF)
    {
        if(c == 27 )
        {
            continue;
        }
        if(c == '[')
        {
            continue;
        }
        *in = c-'0';
        while((c = getchar()) != delim)
        {
            *in = ((*in) * 10) + (c-'0');
        }
        if(delim == ';')
        {
            in = &terminal_col;
            delim = 'R';
        }
        else
        {
            break;
        }
    }
    /* get the trailing 'R' if its still there */
    while(c != EOF && c != 'R')
    {
        c = getchar();
    }
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);
}


/*
 * return the current row at which the cursor is positioned.. rows are 1-based,
 * counting from the top of the screen.
 */
int get_terminal_row()
{
    return terminal_row;
}


/*
 * move the cursor to the given row.. rows are 1-based, counting from the
 * top of the screen.
 *
 * returns the new cursor row.
 */
int set_terminal_row(int row)
{
    int diff = row-terminal_row;
    if(diff < 0)
    {
        fprintf(stdout, "\x1b[%dA", -diff);
    }
    else
    {
        fprintf(stdout, "\x1b[%dB",  diff);
    }
    return row;
}


/*
 * return the current column at which the cursor is positioned.. columns are 1-based,
 * counting from the left side of the screen.
 */
int get_terminal_col()
{
    return terminal_col;
}


/*
 * move the cursor to the given column.. columns are 1-based, counting from the
 * left side of the screen.
 *
 * returns the new cursor column.
 */
int set_terminal_col(int col)
{
    fprintf(stdout, "\x1b[%dG", col);
    return col;
}
