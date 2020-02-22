/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: keywords.h
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

#ifndef KEYWORDS_H
#define KEYWORDS_H

/* the shell command language keywords */
char *keywords[] =
{
    /* POSIX keywords */
    "if"      ,
    "then"    ,
    "else"    ,
    "elif"    ,
    "fi"      ,
    "do"      ,
    "done"    ,
    "case"    ,
    "esac"    ,
    "while"   ,
    "until"   ,
    "for"     ,
    "{"       ,
    "}"       ,
    "!"       ,
    "in"      ,
    /* non-POSIX keywords */
    "select"  ,
    "function",
    "time"    ,
    "coproc"  ,
};
/* keyword count */
static int keyword_count = sizeof(keywords)/sizeof(char *);

/* shell command language operators */
char *operators[] =
{
    "&&" ,
    "||" ,
    ";;" ,
    "<<" ,
    ">>" ,
    "<&" ,
    ">&" ,
    "<>" ,
    //"<<-",
    ">|" ,
    ">!" ,  /* zsh extension, equivalent to >| */
};

#endif
