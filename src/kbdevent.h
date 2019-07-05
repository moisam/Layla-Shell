/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: kbdevent.h
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

#ifndef KBDEVENT_H
#define KBDEVENT_H

#include <linux/input.h>
#include <linux/kd.h>

#define RESKEY      0
#define MAX_KEY     256

extern char ALT_MASK;
extern char CTRL_MASK;
extern char SHIFT_MASK;
extern char CAPS_MASK;
extern char INSERT_MASK;
extern int  kbd_fd;
extern char using_con;

/* Control and Function key definitions */
#define CAPS_KEY        1000
#define SHIFT_KEY       1001
#define CTRL_KEY        1002
#define ALT_KEY         1003
#define UP_KEY          1004
#define DOWN_KEY        1005
#define LEFT_KEY        1006
#define SCRL_KEY        1009
#define	HOME_KEY        1010
#define END_KEY         1012
#define INS_KEY         1013
#define PGUP_KEY        1014
#define PGDOWN_KEY      1015
#define F1_KEY          1016
#define F2_KEY          1017
#define F3_KEY          1018
#define F4_KEY          1019
#define F5_KEY          1020
#define F6_KEY          1021
#define F7_KEY          1022
#define F8_KEY          1023
#define F9_KEY          1024
#define ESC_KEY         1025
#define F10_KEY         1026
#define F11_KEY         1027
#define F12_KEY         1028
#define NUM_KEY         1029
#define DEL_KEY         1030
#define RIGHT_KEY       1031

#define BACKSPACE_KEY   '\b'
#define TAB_KEY         '\t'
#define SPACE_KEY       ' '
#define ENTER_KEY       '\n'

/* default values for the control keys */
#define DEF_INTR_KEY    0x03  /* ^C */
#define DEF_EOF_KEY     0x04  /* ^D */
#define DEF_ERASE_KEY   0x08  /* ^H */
#define DEF_KILL_KEY    0x15  /* ^U */
#define QUIT_KEY        0x1C  /* ^\ -- Not used by this shell */
#define SUSP_KEY        0x1A  /* ^Z -- Not used by this shell */
#define CTRLV_KEY       0x16
#define CTRLW_KEY       0x17
extern char ERASE_KEY ;
extern char KILL_KEY  ;
extern char INTR_KEY  ;
extern char EOF_KEY   ;
extern char VLNEXT_KEY;

/*
 * key order is defined in /usr/include/linux/input-event-codes.h,
 * which is included in /usr/include/linux/input.h
 */

/*
static int keytable[MAX_KEY] =
{
    RESKEY, ESC_KEY, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', CTRL_KEY, 'a', 's', 'd', 'f', 'g', 'h',
    'j', 'k', 'l', ';', '\'', '`', SHIFT_KEY, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', SHIFT_KEY, '*', ALT_KEY, ' ', CAPS_KEY,
    F1_KEY, F2_KEY, F3_KEY, F4_KEY, F5_KEY, F6_KEY, F7_KEY, F8_KEY,
    F9_KEY, F10_KEY, NUM_KEY, SCRL_KEY, '7', '8', '9', '-', '4', '5', '6',
    '+', '1', '2', '3', '0', '.', RESKEY, RESKEY, RESKEY, F11_KEY, F12_KEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, ',', '\n',
    CTRL_KEY, '/', RESKEY, ALT_KEY, '\n', HOME_KEY, UP_KEY, PGUP_KEY,
    LEFT_KEY, RIGHT_KEY, END_KEY, DOWN_KEY, PGDOWN_KEY, INS_KEY, DEL_KEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, '=', RESKEY, RESKEY, RESKEY,
    ',', RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, 
};

static int keytable_shift[MAX_KEY] =
{
    RESKEY, ESC_KEY, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', CTRL_KEY, 'A', 'S', 'D', 'F', 'G', 'H',
    'J', 'K', 'L', ':', '"', '~', SHIFT_KEY, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', SHIFT_KEY, '*', ALT_KEY, ' ', CAPS_KEY,
    F1_KEY, F2_KEY, F3_KEY, F4_KEY, F5_KEY, F6_KEY, F7_KEY, F8_KEY,
    F9_KEY, F10_KEY, NUM_KEY, SCRL_KEY, '7', '8', '9', '-', '4', '5', '6',
    '+', '1', '2', '3', ',', '.', RESKEY, RESKEY, RESKEY, F11_KEY, F12_KEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, ',', '\n',
    CTRL_KEY, '/', RESKEY, ALT_KEY, '\n', HOME_KEY, UP_KEY, PGUP_KEY,
    LEFT_KEY, RIGHT_KEY, END_KEY, DOWN_KEY, PGDOWN_KEY, INS_KEY, DEL_KEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, '=', RESKEY, RESKEY, RESKEY,
    ',', RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY,
    RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, RESKEY, 
};
*/


int rootperm(char *tag);
// int open_kbd_device(char *tag);
// int open_con_device(char *tag);
// int get_next_key();
// int get_next_key_con();
// int get_next_key_kbd();
int process_key(struct input_event event);
// extern int (*get_next_key)();

/* kbdevent2.c */
int  rawon();
void rawoff();
// int  get_next_key2();
int  get_next_key();
char *get_key_str(int c);

#endif
