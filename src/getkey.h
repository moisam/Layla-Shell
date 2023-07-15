/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: getkey.h
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

#ifndef GETKEY_H
#define GETKEY_H

extern char ALT_MASK;
extern char CTRL_MASK;
extern char SHIFT_MASK;
extern char CAPS_MASK;
extern char INSERT_MASK;

/* Control and Function key definitions */
#define CAPS_KEY	1
#define SHIFT_KEY	2
#define CTRL_KEY	3
#define ALT_KEY		4
#define UP_KEY		5
#define DOWN_KEY	6
#define LEFT_KEY	7
#define BACKSPACE_KEY	8
#define TAB_KEY		9
#define RIGHT_KEY	10
#define SCRL_KEY	11
#define	HOME_KEY	12
#define ENTER_KEY	13
#define END_KEY		14
#define INS_KEY		15
#define PGUP_KEY	16
#define PGDOWN_KEY	17
#define F1_KEY		18
#define F2_KEY		19
#define F3_KEY		20
#define F4_KEY		21
#define F5_KEY		22
#define F6_KEY		23
#define F7_KEY		24
#define F8_KEY		25
#define F9_KEY		26
#define ESC_KEY		27
#define F10_KEY		28
#define F11_KEY		29
#define F12_KEY		30
#define NUM_KEY		31
#define SPACE_KEY	32
#define DEL_KEY		127

#define INTR_KEY        0x03    /* ^C */
#define EOF_KEY         0x04    /* ^D */
#define ERASE_KEY       0x08    /* ^H   NOTE: SHOULD BE DEL, RIGHT?? */
#define KILL_KEY        0x15    /* ^U */
#define CTRLV_KEY       0x16
#define CTRLW_KEY       0x17

int getkey(void);

#endif
