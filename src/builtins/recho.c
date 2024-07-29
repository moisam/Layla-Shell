/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2020 (c)
 * 
 *    file: recho.c
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

#define UTILITY             "recho"


void print_arg(char *arg)
{
    char *p = arg;
    
    while(*p)
    {
        if(*p < ' ')
        {
            printf("^%c", *p+64);
        }
        else if(*p == 127)
        {
            printf("^?");
        }
        else
        {
            putchar(*p);
        }
        
        p++;
    }
}


/*
 * The recho builtin utility (non-POSIX). Prints back the arguments passed to it,
 * each argument enclosed in <>, with invisible chars made visible.
 *
 * Returns 0 invariably.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help recho` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int recho_builtin(int argc, char **argv)
{
    int i;
    
    for(i = 1; i < argc; i++)
    {
        printf("arg[%d] = <", i);
        print_arg(argv[i]);
        printf(">\n");
    }
    
    return 0;
}
