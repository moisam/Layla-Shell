/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: getkey.c
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
#include "cmd.h"
#include "getkey.h"


int getkey(void)
{
    char buf[1];
    int res;
// read:
    res = read(0, &buf[0], 1);
    if(res == -1) { return -1; }
    switch (buf[0])
    {
	/* scancodes for keypresses */
	case 0x01 : return ESC_KEY;
	case 0x02 : return SHIFT_MASK?'!':'1';
	case 0x03 : return SHIFT_MASK?'@':'2';
	case 0x04 : return SHIFT_MASK?'#':'3';
	case 0x05 : return SHIFT_MASK?'$':'4';
	case 0x06 : return SHIFT_MASK?'%':'5';
	case 0x07 :
            if(CTRL_MASK) return 0x1e;
            return SHIFT_MASK?'^':'6';
	case 0x08 : return SHIFT_MASK?'&':'7';
	case 0x09 : return SHIFT_MASK?'*':'8';
	case 0x0a : return SHIFT_MASK?'(':'9';
	case 0x0b : return SHIFT_MASK?')':'0';
	case 0x0c :
            if(CTRL_MASK) return 0x1f;
            return SHIFT_MASK?'_':'-';
	case 0x0d : return SHIFT_MASK?'+':'=';
	case 0x0e : return BACKSPACE_KEY;
	case 0x0f : return TAB_KEY;
	case 0x10 :
            if(CTRL_MASK) return 0x11;
            return SHIFT_MASK?'Q':'q';
	case 0x11 :
            if(CTRL_MASK) return 0x17;
            return SHIFT_MASK?'W':'w';
	case 0x12 :
            if(CTRL_MASK) return 0x05;
            return SHIFT_MASK?'E':'e';
	case 0x13 :
            if(CTRL_MASK) return 0x12;
            return SHIFT_MASK?'R':'r';
	case 0x14 :
            if(CTRL_MASK) return 0x14;
            return SHIFT_MASK?'T':'t';
	case 0x15 :
            if(CTRL_MASK) return 0x19;
            return SHIFT_MASK?'Y':'y';
	case 0x16 :
            if(CTRL_MASK) return 0x15;
            return SHIFT_MASK?'U':'u';
	case 0x17 :
            if(CTRL_MASK) return 0x09;
            return SHIFT_MASK?'I':'i';
	case 0x18 :
            if(CTRL_MASK) return 0x0f;
            return SHIFT_MASK?'O':'o';
	case 0x19 :
            if(CTRL_MASK) return 0x10;
            return SHIFT_MASK?'P':'p';
	case 0x1a :
            if(CTRL_MASK) return 0x1b;
            return SHIFT_MASK?'{':'[';
	case 0x1b :
            if(CTRL_MASK) return 0x1d;
            return SHIFT_MASK?'}':']';
	case 0x1c : return ENTER_KEY;
	case 0x1d : CTRL_MASK  = 1; return 0;
	case 0x1e :
            if(CTRL_MASK) return 0x01;
            return SHIFT_MASK?'A':'a';
	case 0x1f :
            if(CTRL_MASK) return 0x13;
            return SHIFT_MASK?'S':'s';
	case 0x20 :
            if(CTRL_MASK) return 0x04; /* EOF; */
            return SHIFT_MASK?'D':'d';
	case 0x21 :
            if(CTRL_MASK) return 0x06;
            return SHIFT_MASK?'F':'f';
	case 0x22 :
            if(CTRL_MASK) return 0x07;
            return SHIFT_MASK?'G':'g';
	case 0x23 :
            if(CTRL_MASK) return 0x08;
            return SHIFT_MASK?'H':'h';
	case 0x24 :
            if(CTRL_MASK) return 0x0a;
            return SHIFT_MASK?'J':'j';
	case 0x25 :
            if(CTRL_MASK) return 0x0b;
            return SHIFT_MASK?'K':'k';
	case 0x26 :
            if(CTRL_MASK) return 0x0c;
            return SHIFT_MASK?'L':'l';
	case 0x27 : return SHIFT_MASK?':':';';
	case 0x28 : return SHIFT_MASK?'"':'\'';
	case 0x29 : return SHIFT_MASK?'~':'`';
	case 0x2a : SHIFT_MASK = 1; return 0; /* SHIFT_MASK_DOWN; LSH */
	case 0x2b :
            if(CTRL_MASK) return 0x1c;
            return SHIFT_MASK?'|':'\\';
	case 0x2c :
            if(CTRL_MASK) return 0x1a;
            return SHIFT_MASK?'Z':'z';
	case 0x2d :
            if(CTRL_MASK) return 0x18;
            return SHIFT_MASK?'X':'x';
	case 0x2e :
            if(CTRL_MASK) return 0x03;
            return SHIFT_MASK?'C':'c';
	case 0x2f :
            if(CTRL_MASK) return 0x16;
            return SHIFT_MASK?'V':'v';
	case 0x30 :
            if(CTRL_MASK) return 0x02;
            return SHIFT_MASK?'B':'b';
	case 0x31 :
            if(CTRL_MASK) return 0x0e;
            return SHIFT_MASK?'N':'n';
	case 0x32 :
            if(CTRL_MASK) return 0x0d;
            return SHIFT_MASK?'M':'m';
	case 0x33 : return SHIFT_MASK?'<':',';
	case 0x34 : return SHIFT_MASK?'>':'.';
	case 0x35 : return SHIFT_MASK?'?':'/';
	case 0x36 : SHIFT_MASK = 1; return 0; /* SHIFT_MASK_DOWN; RSH */

        case 0x38 : ALT_MASK   = 1; return 0;
	case 0x39 : return SPACE_KEY;
	case 0x3a : return CAPS_KEY;	
	case 0x3b : return F1_KEY;
        case 0x3c : return F2_KEY;
        case 0x3d : return F3_KEY;
        case 0x3e : return F4_KEY;
        case 0x3f : return F5_KEY;
        case 0x40 : return F6_KEY;
        case 0x41 : return F7_KEY;
        case 0x42 : return F8_KEY;
        case 0x43 : return F9_KEY;
        case 0x44 : return F10_KEY;
        case 0x45 : return NUM_KEY;
        case 0x46 : return SCRL_KEY;
        case 0x56 : return SHIFT_MASK?'>':'<';
        case 0x57 : return F11_KEY;
        case 0x58 : return F12_KEY;
	/* scancodes for keyreleases */
	case -86: SHIFT_MASK = 0; return 0; /* SHIFT_MASK_UP; LSH */
	case -99: CTRL_MASK  = 0; return 0; /* LCT */
	case -72: ALT_MASK   = 0; return 0; /* LAL */
	case -74: SHIFT_MASK = 0; return 0; /* SHIFT_MASK_UP; RSH */
	case -32:
	  res = read(0, &buf[0], 1);
          
	  if(buf[0] == 73) return PGUP_KEY;
	  if(buf[0] == 81) return PGDOWN_KEY;
	  if(buf[0] == 72) return UP_KEY;
	  if(buf[0] == 71) return HOME_KEY;
	  if(buf[0] == 79) return END_KEY;
	  if(buf[0] == 82) return INS_KEY;
	  if(buf[0] == 83) return DEL_KEY;
	  if(buf[0] == 75) return LEFT_KEY;
	  if(buf[0] == 80) return DOWN_KEY;
	  if(buf[0] == 77) return RIGHT_KEY;
	  if(buf[0] == 29 ) { CTRL_MASK = 1; return 0;/* RCT */ }
	  if(buf[0] == -99) { CTRL_MASK = 0; return 0;/* RCT */ }
	  if(buf[0] == 56 ) { ALT_MASK  = 1; return 0;/* RAL */ }
	  if(buf[0] == -72) { ALT_MASK  = 0; return 0;/* RAL */ }
    }
    return 0;
}
