/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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

struct keyword_s
{
    int   len;
    char *str;
};

struct keyword_s keywords[] =
{
    /* POSIX keywords */
    { 2, "if"       },
    { 4, "then"     },
    { 4, "else"     },
    { 4, "elif"     },
    { 2, "fi"       },
    { 2, "do"       },
    { 4, "done"     },
    { 4, "case"     },
    { 4, "esac"     },
    { 5, "while"    },
    { 5, "until"    },
    { 3, "for"      },
    { 1, "{"        },
    { 1, "}"        },
    { 1, "!"        },
    { 2, "in"       },
    /* non-POSIX keywords */
    { 6, "select"   },
    { 8, "function" },
    { 4, "time"     },
};

static int keyword_count = sizeof(keywords)/sizeof(struct keyword_s);

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

//static int operator_count = sizeof(operators)/sizeof(char *);

#endif
