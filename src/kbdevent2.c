/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: kbdevent2.c
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>
#include "cmd.h"
#include "kbdevent.h"
#include "debug.h"

/* original terminal attributes (when the shell started) */
struct termios tty_attr_old;

/* current terminal attributes (as the shell runs) */
struct termios tty_attr    ;

/* masks to indicate the status of meta keys */
char ALT_MASK   ;
char CTRL_MASK  ;
char SHIFT_MASK ;
char CAPS_MASK  ;
char INSERT_MASK;

/* special control keys, as defined by the stty utility for the terminal */
char ERASE_KEY ;
char KILL_KEY  ;
char INTR_KEY  ;
char EOF_KEY   ;
char VLNEXT_KEY;


/*
 * turn the raw mode on.
 * 
 * returns 1 if the terminal attributes are set successfully, 0 otherwise.
 */
int rawon(void)
{
    /* get the special control keys */
    ERASE_KEY  = tty_attr_old.c_cc[VERASE];
    KILL_KEY   = tty_attr_old.c_cc[VKILL ];
    INTR_KEY   = tty_attr_old.c_cc[VINTR ];
    EOF_KEY    = tty_attr_old.c_cc[VEOF  ];
    VLNEXT_KEY = tty_attr_old.c_cc[VLNEXT];
    
    /* if any of the special control keys is not defined, use our default value */
    if(!ERASE_KEY)
    {
        ERASE_KEY = DEF_ERASE_KEY;
    }
    
    if(!KILL_KEY)
    {
        KILL_KEY = DEF_KILL_KEY;
    }
    
    if(!INTR_KEY)
    {
        INTR_KEY = DEF_INTR_KEY;
    }
    
    if(!EOF_KEY)
    {
        EOF_KEY = DEF_EOF_KEY;
    }
    
    if(!VLNEXT_KEY)
    {
        VLNEXT_KEY = CTRLV_KEY;
    }
    
    /* now modify the terminal's attributes */
    tty_attr = tty_attr_old;
    
    /* turn off buffering, echo and key processing */
    tty_attr.c_lflag    &= ~(ICANON | ECHO | /* ISIG | */ IEXTEN);
    tty_attr.c_iflag    &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF | INPCK | BRKINT);
    tty_attr.c_cflag    &= ~CREAD;
    //tty_attr.c_oflag    &= ~(OPOST);
    tty_attr.c_cc[VMIN ] = 0;         /* wait until at least one keystroke is available */
    tty_attr.c_cc[VTIME] = 1;         /* no timeout */
    
    /* set the new terminal attributes */
    if((tcsetattr(cur_tty_fd(), TCSAFLUSH, &tty_attr) == -1))
    {
        return 0;
    }
    return 1;
}


/*
 * return the next key press from the terminal.
 */
int get_next_key(int tty)
{
    CTRL_MASK = 0;
    int nread;
    char c;
    while((nread = read(tty, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN)
        {
            return 0;
        }
    }
    
    if(c == '\x1b')
    {
        char seq[3];        
        if(read(tty, &seq[0], 1) != 1)
        {
            return '\x1b';
        }
        if(read(tty, &seq[1], 1) != 1)
        {
            return '\x1b';
        }

        if(seq[0] == '[')
        {
            if(seq[1] >= '0' && seq[1] <= '9')
            {
                if(read(tty, &seq[2], 1) != 1)
                {
                    return '\x1b';
                }
                                
                if(seq[2] == '~')
                {
                    switch(seq[1])
                    {
                        case '1':
                            return HOME_KEY  ;
                            
                        case '2': 
                            return INS_KEY   ;       /* keypad '0' key */
                            
                        case '3':
                            return DEL_KEY   ;
                            
                        case '4':
                            return END_KEY   ;
                            
                        case '5':
                            return PGUP_KEY  ;
                            
                        case '6':
                            return PGDOWN_KEY;
                            
                        case '7':
                            return HOME_KEY  ;
                            
                        case '8':
                            return END_KEY   ;
                    }
                }
                else if(seq[2] == ';')
                {
                    if(read(tty, &seq[1], 1) != 1)
                    {
                        return '\x1b';
                    }
                    if(read(tty, &seq[1], 1) != 1)
                    {
                        return '\x1b';
                    }
                    switch(seq[1])
                    {
                        case 'A':
                            CTRL_MASK = 1;
                            return UP_KEY;
                            
                        case 'B':
                            CTRL_MASK = 1;
                            return DOWN_KEY;
                            
                        case 'C':
                            CTRL_MASK = 1;
                            return RIGHT_KEY;
                            
                        case 'D':
                            CTRL_MASK = 1;
                            return LEFT_KEY;
                            
                        case 'P':
                            CTRL_MASK = 1;
                            return F1_KEY;
                            
                        case 'Q':
                            CTRL_MASK = 1;
                            return F2_KEY;
                            
                        case 'R':
                            CTRL_MASK = 1;
                            return F3_KEY;
                            
                        case 'S':
                            CTRL_MASK = 1;
                            return F4_KEY;
                    }
                }                
                else if(seq[1] == '1' || seq[1] == '2')
                {
                    if(read(tty, &seq[1], 1) != 1)
                    {
                        return '\x1b';
                    }
                    if(seq[1] == ';')
                    {
                        if(read(tty, &seq[1], 1) != 1)
                        {
                            return '\x1b';
                        }
                        CTRL_MASK = 1;
                    }
                    
                    if(seq[1] != '~')
                    {
                        if(read(tty, &seq[1], 1) != 1)
                        {
                            return '\x1b';
                        }
                        CTRL_MASK = 1;
                    }

                    switch(seq[2])
                    {
                        case '0':
                            return F9_KEY ;

                        case '1':
                            return F10_KEY;

                        case '3':
                            return F11_KEY;

                        case '4':
                            return F12_KEY;

                        case '5':
                            return F5_KEY ;

                        case '7':
                            return F6_KEY ;

                        case '8':
                            return F7_KEY ;

                        case '9':
                            return F8_KEY ;
                    }
                }
            }
            else
            {
                switch(seq[1])
                {
                    case 'A':
                        return UP_KEY   ;
                        
                    case 'B':
                        return DOWN_KEY ;
                        
                    case 'C':
                        return RIGHT_KEY;
                        
                    case 'D':
                        return LEFT_KEY ;
                        
                    case 'E':
                        return 0        ;         /* TODO: this is the keypad '5' key. what does it signify? */
                        
                    case 'F':
                        return END_KEY  ;
                        
                    case 'H':
                        return HOME_KEY ;
                }
            }
        }
        else if(seq[0] == 'O')
        {
            switch(seq[1])
            {
                case 'H':
                    return HOME_KEY;
                    
                case 'F':
                    return END_KEY ;
                    
                case 'P':
                    return F1_KEY  ;
                    
                case 'Q':
                    return F2_KEY  ;
                    
                case 'R':
                    return F3_KEY  ;
                    
                case 'S':
                    return F4_KEY  ;
            }
        }
        
        return '\x1b';
        
    }
    else
    {
        if(c == 127)
        {
            return '\b';
        }
        return c;
    }
}
