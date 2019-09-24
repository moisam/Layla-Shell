/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: glob.c
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

#include <wchar.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "../cmd.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY             "glob"


/*
 * the glob builtin utility (non-POSIX).. prints its arguments list in a way similar
 * to what echo does, except that glob null-terminates each argument.
 *
 * returns 0.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help glob` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int __glob(int argc, char **argv)
{
    /*
     * in bash, shopt option 'xpg_echo' is used to indicate whether escape
     * sequences are enabled by echo by default.. this behavior can be overriden
     * by use of the -e and -E options (see below).
     */
    int v, allow_escaped = optionx_set(OPTION_XPG_ECHO);
    char *p;
    /* process options and arguments */
    for( ; v < argc; v++)
    { 
        /* options start with '-' */
        if(argv[v][0] == '-')
        {
            p = argv[v]+1;
            int op = 1;
            /*
             * check the validity of the options string.. we only accept
             * three options: e, n and E.. if the string contains any other
             * letter, we treat it as an argument to be printed, not an option.
             */
            while(*p)
            {
                if(*p != 'e' && *p != 'n' && *p != 'E')
                {
                    op = 0;
                    break;
                }
                p++;
            }
            if(!op)
            {
                break;
            }
            /* now process the options */
            p = argv[v]+1;
            while(*p)
            {
                switch(*p)
                {
                    /* -e: allow escape characters (see the manpage for the details) */
                    case 'e':
                        allow_escaped = 1;
                        break;
                        
                    /* -E: don't allow escape characters (see the manpage for the details) */
                    case 'E':
                        allow_escaped = 0;
                        break;
                }
                p++;
            }
        }
        else break;
    }
    int flags = allow_escaped ? FLAG_ECHO_ALLOW_ESCAPED : 0;
    flags |= FLAG_ECHO_NULL_TERM;
    do_echo(v, argc, argv, flags);
    return 0;
}
