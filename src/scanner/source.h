/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: source.h
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

#ifndef SOURCE_H
#define SOURCE_H

#define EOF     (-1)
#define ERRCHAR ( 0)

/* structure to hold information about the source file being translated */

struct source_s
{
    /* values for the srctype field below */
#define SOURCE_CMDSTR           1
#define SOURCE_STDIN            2
#define SOURCE_DOTFILE          3
#define SOURCE_FIFO             4
#define SOURCE_EVAL             5
#define SOURCE_FCCMD            6
#define SOURCE_EXTERNAL_FILE    7
#define SOURCE_FUNCTION         8
    int  srctype;       /* the type of this input source */
    char *srcname;      /* for functions and external files */
    char *buffer;       /* the input text */
    long bufsize;       /* size of the input text */
    long curline,       /* current line in source */
         curchar,       /* current char in source */
         curpos,        /* absolute char position in source */
         curlinestart;  /* absolute start of current line in source */
    long wstart;        /* start of currently parsed commandline */
};

/* functions to manipulate input sources */
char peek_char(struct source_s *src);
char next_char(struct source_s *src);
char prev_char(struct source_s *src);
void unget_char(struct source_s *src);
void skip_white_spaces(struct source_s *src);

#endif
