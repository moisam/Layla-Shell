/*
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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

/* Macro definitions needed to use sig*() */
#define _POSIX_C_SOURCE 200112L

#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include "include/cmd.h"
#include "include/sig.h"
#include "backend/backend.h"
#include "include/debug.h"
#include "include/kbdevent.h"

/****************************************
 *
 * Terminal control/status functions
 *
 ****************************************/

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;
extern struct termios tty_attr;


/*
 * Return a new file descriptor to the current terminal device.
 */
int cur_tty_fd(void)
{
    char *dev;
    static int i, fd = -1;
    
    if(fd >= 0 && isatty(fd))
    {
        return fd;
    }

    for(i = 0; i <= 2; i++)
    {
        if((dev = ttyname(i)))
        {
            break;
        }
    }
    
    if(!dev)
    {
        errno = ENOTTY;
        return -1;
    }

    do
    {
        fd = open(dev, O_RDWR | O_NOCTTY);
    } while (fd == -1 && errno == EINTR);

    if (fd == -1)
    {
        return -1;
    }

    return fd;
}


/*
 * Read one char from the terminal.
 */
int read_char(int tty)
{
    int nread;
    unsigned char c[4];
    
    while((nread = read(tty, c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN)
        {
            return 0;
        }
    }
    return c[0];
}


/*
 * Turn the terminal canonical mode on or off.
 */
void term_canon(int on)
{
    int tty = cur_tty_fd();
    if(!isatty(tty))
    {
        return;
    }

    if(on)
    {
        tcsetattr(tty, TCSANOW, &tty_attr_old);
    }
    else
    {
        tcsetattr(tty, TCSANOW, &tty_attr);
    }
}


/* current terminal attributes (we need this when resuming jobs and turning echo on/off) */
struct termios cur_tty_attr;


/*
 * Save the current termios structure from the given file descriptor.
 * 
 * Returns 1 the current tty atributes on success, NULL on failure.
 */
struct termios *save_tty_attr(void)
{
    int tty = cur_tty_fd();
    
    /* get the terminal attributes */
    if(tcgetattr(tty, &cur_tty_attr) == -1)
    {
        return NULL;
    }
    
    return &cur_tty_attr;
}


/*
 * Set the termios structure on the given file descriptor.
 * 
 * Returns 1 on success, 0 on failure.
 */
int set_tty_attr(int tty, struct termios *attr)
{
    int res = 0;
    
    /* set the new terminal attributes */
    do
    {
        res = tcsetattr(tty, TCSAFLUSH, attr);
    } while (res == -1 && errno == EINTR);
    
    if(res == -1)
    {
        PRINT_ERROR(SHELL_NAME, "failed to set terminal attributes: %s", 
                    strerror(errno));
    }
    
    return !(res == -1);
}


void set_term_pgid(int tty, pid_t pid)
{
    if(tty == -1)
    {
        return;
    }
    
    if(option_set('m'))
    {
        sigset_t sigset, old_sigset;
        
        sigemptyset(&sigset);
        sigemptyset(&old_sigset);
        sigaddset(&sigset, SIGCHLD);
        sigaddset(&sigset, SIGTTIN);
        sigaddset(&sigset, SIGTTOU);
        sigaddset(&sigset, SIGTSTP);
        sigprocmask(SIG_BLOCK, &sigset, &old_sigset);

        tcsetpgrp(tty, pid);

        sigprocmask(SIG_SETMASK, &old_sigset, (sigset_t *)NULL);
    }
}


/*
 * Get the screen size and save the width and height in the $COLUMNS and $LINES
 * shell variables, respectively.
 *
 * Returns 1 if the screen size if obtained and saved, 0 otherwise.
 */
int get_screen_size(void)
{
    struct winsize w;
    /* find the size of the terminal window */
    int tty = cur_tty_fd();
    if(tty == -1)
    {
        return 0;
    }

    int res = ioctl(tty, TIOCGWINSZ, &w);
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
    
    if(sprintf(buf, "%zu", VGA_WIDTH))
    {
        symtab_entry_setval(e, buf);
    }
    
    /* update the value of terminal rows in environ and in the symbol table */
    e = get_symtab_entry("LINES");

    if(!e)
    {
        e = add_to_symtab("LINES");
    }
    
    if(sprintf(buf, "%zu", VGA_HEIGHT))
    {
        symtab_entry_setval(e, buf);
    }
    
    return 1;
}


/*
 * Move the cursor to the given row (line) and column. Both values are 1-based,
 * counting from the top-left corner of the screen.
 */
void move_cur(int row, int col)
{
    if(isatty(stdout->_fileno))
    {
        fprintf(stdout, "\e[%d;%dH", row, col);
        fflush(stdout);
    }
}


/*
 * Clear the screen.
 */
void clear_screen(void)
{
    if(isatty(stdout->_fileno))
    {
        fprintf(stdout, "\e[2J");
        fprintf(stdout, "\e[0m");
        fprintf(stdout, "\e[3J\e[1;1H");
    }
}


/*
 * Set the text foreground and background color.
 */
void set_terminal_color(int FG, int BG)
{
    if(isatty(stdout->_fileno))
    {
        /*control sequence to set screen color */
        fprintf(stdout, "\x1b[%d;%dm", FG, BG);
    }
}


/*
 * Read the row or column number from the terminal.
 */
int term_get_num(int *delim, int tty)
{
    int c, in = 0;
    (*delim) = 0;
    while((c = read_char(tty)))
    {
        if(c < '0' || c > '9')
        {
            (*delim) = c;
            break;
        }
        in = ((in) * 10) + (c-'0');
    }
    return in;
}


/*
 * Get the cursor position (current row and column), which are 1-based numbers,
 * counting from the top-left corner of the screen.
 */
void update_row_col(void)
{
    /*
     * Clear the terminal device's EOF flag. This would have been set, for example,
     * if we used the read builtin to read from the terminal and the user pressed
     * CTRL-D to indicate end of input. We can't read the cursor position without
     * clearing that flag.
     */
    if(feof(stdin))
    {
        clearerr(stdin);
    }

    int tty = cur_tty_fd();
    if(tty < 0)
    {
        return;
    }
    
    /*
     * we will temporarily block SIGCHLD so it won't disturb us between our cursor
     * position request and the terminal's response.
     */
    sigset_t intmask;
    SIGNAL_BLOCK(SIGCHLD, intmask);
    
    /* request terminal cursor position report (CPR) */
    //fprintf(stdout, "\x1b[6n");
    if(write(tty, "\x1b[6n", 4) != 4)
    {
        SIGNAL_UNBLOCK(intmask);
        return;
    }
    
    /*
     * read the result. it will be reported as:
     *     ESC [ y; x R
     */
    
    /* skip the ^[[ sequence */
    int i, delim = 0;
    do
    {
        if(read_char(tty) != '\e')
        {
            break;
        }
    
        if(read_char(tty) != '[')
        {
            break;
        }
    
        /* get the row */
        i = term_get_num(&delim, tty);
        if(delim == ';')
        {
            terminal_row = i;
        }
        else
        {
            break;
        }
    
        /* get the row */
        i = term_get_num(&delim, tty);
        if(delim == 'R')
        {
            terminal_col = i;
        }
    } while(0);

    SIGNAL_UNBLOCK(intmask);
}


/*
 * Return the current row at which the cursor is positioned. Rows are 1-based,
 * counting from the top of the screen.
 */
size_t get_terminal_row(void)
{
    return terminal_row;
}


/*
 * Return the current column at which the cursor is positioned. Columns are 1-based,
 * counting from the left side of the screen.
 */
size_t get_terminal_col(void)
{
    return terminal_col;
}
