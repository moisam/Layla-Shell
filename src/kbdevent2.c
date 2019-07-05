/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "kbdevent.h"
#include "debug.h"

struct termios tty_attr_old;
struct termios tty_attr    ;

char ALT_MASK   ;
char CTRL_MASK  ;
char SHIFT_MASK ;
char CAPS_MASK  ;
char INSERT_MASK;

char ERASE_KEY ;
char KILL_KEY  ;
char INTR_KEY  ;
char EOF_KEY   ;
char VLNEXT_KEY;


char *get_key_str(int c)
{
    static char buf[16];
    
    switch(c)
    {
        case ESC_KEY:    return "ESC"      ;
        case RESKEY:     return "UNKNOWN"  ;
        case '\b':       return "BACKSPACE";
        case '\t':       return "TAB"      ;
        case '\n':       return "NEWLINE"  ;
        case CTRL_KEY:   return "CTRL"     ;
        case SHIFT_KEY:  return "SHIFT"    ;
        case ALT_KEY:    return "ALT"      ;
        case ' ':        return "SPACE"    ;
        case CAPS_KEY:   return "CAPSLOCK" ;
        case F1_KEY:     return "F1"       ;
        case F2_KEY:     return "F2"       ;
        case F3_KEY:     return "F3"       ;
        case F4_KEY:     return "F4"       ;
        case F5_KEY:     return "F5"       ;
        case F6_KEY:     return "F6"       ;
        case F7_KEY:     return "F7"       ;
        case F8_KEY:     return "F8"       ;
        case F9_KEY:     return "F9"       ;
        case F10_KEY:    return "F10"      ;
        case F11_KEY:    return "F11"      ;
        case F12_KEY:    return "F12"      ;
        case HOME_KEY:   return "HOME"     ;
        case PGUP_KEY:   return "PGUP"     ;
        case UP_KEY:     return "UP"       ;
        case LEFT_KEY:   return "LEFT"     ;
        case RIGHT_KEY:  return "RIGHT"    ;
        case END_KEY:    return "END"      ;
        case DOWN_KEY:   return "DOWN"     ;
        case PGDOWN_KEY: return "PGDN"     ;
        case INS_KEY:    return "INSERT"   ;
        case DEL_KEY:    return "DELETE"   ;
        default:
            if(c < 32)
            {
                sprintf(buf, "^%c", 64+c);
            }
            else
            {
                sprintf(buf, "%c (%d)", c, c);
            }
            return buf;
    }
}


void rawoff()
{
    if(isatty(0)) tcsetattr(0, TCSAFLUSH, &tty_attr_old);
}


int rawon()
{
    if(tcgetattr(0, &tty_attr_old) == -1) return 0;
    atexit(rawoff);
    /* get the special control keys */
    ERASE_KEY  = tty_attr_old.c_cc[VERASE];
    KILL_KEY   = tty_attr_old.c_cc[VKILL ];
    INTR_KEY   = tty_attr_old.c_cc[VINTR ];
    EOF_KEY    = tty_attr_old.c_cc[VEOF  ];
    VLNEXT_KEY = tty_attr_old.c_cc[VLNEXT];
    if(!ERASE_KEY ) ERASE_KEY  = DEF_ERASE_KEY;
    if(!KILL_KEY  ) KILL_KEY   = DEF_KILL_KEY ;
    if(!INTR_KEY  ) INTR_KEY   = DEF_INTR_KEY ;
    if(!EOF_KEY   ) EOF_KEY    = DEF_EOF_KEY  ;
    if(!VLNEXT_KEY) VLNEXT_KEY = CTRLV_KEY    ;
    /* now modify the terminal's attributes */
    tty_attr            = tty_attr_old;
    /* turn off buffering, echo and key processing */
    tty_attr.c_lflag    &= ~(ICANON | ECHO | /* ISIG | */ IEXTEN);
    tty_attr.c_iflag    &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF | INPCK | BRKINT);
    //tty_attr.c_oflag    &= ~(OPOST);
    tty_attr.c_cc[VMIN]  = 0;          /* wait until at least one keystroke available */
    tty_attr.c_cc[VTIME] = 1;         /* no timeout */
    if((tcsetattr(0, TCSAFLUSH, &tty_attr) == -1)) return 0;
    return 1;
}


int get_next_key()
// int get_next_key2()
{
    int nread;
    char c;
    while((nread = read(0, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN)
        {
            return 0;
        }
    }
    
    if(c == '\x1b')
    {
        char seq[3];        
        if(read(0, &seq[0], 1) != 1) return '\x1b';
        if(read(0, &seq[1], 1) != 1) return '\x1b';

        if(seq[0] == '[')
        {
            if(seq[1] >= '0' && seq[1] <= '9')
            {
                if(read(0, &seq[2], 1) != 1) return '\x1b';                
                                
                if(seq[2] == '~')
                {
                    switch(seq[1])
                    {
                        case '1': return HOME_KEY  ;
                        case '2': return INS_KEY   ;       /* keypad '0' key */
                        case '3': return DEL_KEY   ;
                        case '4': return END_KEY   ;
                        case '5': return PGUP_KEY  ;
                        case '6': return PGDOWN_KEY;
                        case '7': return HOME_KEY  ;
                        case '8': return END_KEY   ;
                    }
                }
                else if(seq[2] == ';')
                {
                    if(read(0, &seq[1], 1) != 1) return '\x1b';
                    if(read(0, &seq[1], 1) != 1) return '\x1b';
                    switch(seq[1])
                    {
                        case 'P': CTRL_MASK = 1; return F1_KEY;
                        case 'Q': CTRL_MASK = 1; return F2_KEY;
                        case 'R': CTRL_MASK = 1; return F3_KEY;
                        case 'S': CTRL_MASK = 1; return F4_KEY;
                    }
                }                
                else if(seq[1] == '1' || seq[1] == '2')
                {
                    if(read(0, &seq[1], 1) != 1) return '\x1b';
                    if(seq[1] == ';')
                    {
                        if(read(0, &seq[1], 1) != 1) return '\x1b';
                        CTRL_MASK = 1;
                    }
                    
                    if(seq[1] != '~')
                    {
                        if(read(0, &seq[1], 1) != 1) return '\x1b';
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
                    case 'A': return UP_KEY   ;
                    case 'B': return DOWN_KEY ;
                    case 'C': return RIGHT_KEY;
                    case 'D': return LEFT_KEY ;
                    case 'E': return 0        ;         /* TODO: this is the keypad '5' key. what does it signify? */
                    case 'F': return END_KEY  ;
                    case 'H': return HOME_KEY ;
                }
            }
        }
        else if(seq[0] == 'O')
        {
            switch(seq[1])
            {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY ;
                case 'P': return F1_KEY  ;
                case 'Q': return F2_KEY  ;
                case 'R': return F3_KEY  ;
                case 'S': return F4_KEY  ;
            }
        }
        
        return '\x1b';
        
    }
    else
    {
        if(c == 127) return '\b';
        return c;
    }
}
