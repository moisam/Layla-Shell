/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2018, 2019, 2020 (c)
 * 
 *    file: debug.c
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

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include "debug.h"

/*
 * Print a debug message, preceded by the filename and function name from which
 * the error/message originated. The message format and any extra arguments are
 * passed to printf to be printed. If the format field is NULL, the function
 * prints the filename and function name in a separate line and returns.
 */
void __debug (const char *file, const char *function, char *format, ...)
{
    fprintf(stderr, "%d: %s: %s: ", getpid(), file, function);
    if(!format)
    {
        fprintf(stderr, "\n");
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}
