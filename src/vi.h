/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: vi.h
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

#ifndef LSH_VI
#define LSH_VI

/* vi_keys.c */
void clear_cmd(int startat);
void output_cmd(void);
void do_insert(char c);
void do_kill_key(void);
void do_del_key(int count);
void do_backspace(int count);
void do_up_key(int count);
void do_down_key(int count);
void do_right_key(int count);
void do_left_key(int count);
void do_home_key(void);
void do_end_key(void);
void print_ctrl_key(char c);
void yank(int start, int end);

extern char *savebuf     ;
extern int   savebuf_size;

/* vi.c */
int vi_cmode(void);

#endif
