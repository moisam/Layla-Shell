/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: eval.c
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "builtins.h"
#include "../cmd.h"
#include "../backend/backend.h"


/*
 * the eval builtin utility (POSIX).. used to evaluate arguments and run them as
 * commands. returns the exit status of the command run.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help eval` from lsh prompt to see a short explanation on
 * how to use this utility.
 */

int eval_builtin(int argc, char **argv)
{
    /* nothing to be done */
    if(argc == 1)
    {
        return 0;
    }
    
    /*
     * copy the list of arguments into a buffer, which we'll pass to do_cmd() so that
     * it will parse and execute it as if it was a script file.
     */
    char *cmd = list_to_str(&argv[1]);
    if(!cmd)
    {
        PRINT_ERROR("eval: insufficient memory\n");
        return 1;
    }

    /* POSIX says we shall return 0 if we have NULL arguments */
    if(!*cmd)
    {
        return 0;
    }

    /* parse and execute the buffer */
    struct source_s src;
    src.buffer   = cmd;
    src.bufsize  = strlen(cmd);
    src.srctype  = SOURCE_EVAL;
    src.curpos   = INIT_SRC_POS;
    src.srcname  = NULL;
    src.curline  = 1;

    /* add a new entry to the callframe stack to reflect the new scope we're entering */
    callframe_push(callframe_new(argv[1], src.srcname, src.curline));

    /* execute the commands */
    parse_and_execute(&src);

    /* pop the callframe entry we've added to the stack */
    callframe_popf();
    
    /* free used memory */
    free(cmd);

    /* return the last command's exit status */
    return exit_status;
}
