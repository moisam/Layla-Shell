/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../backend/backend.h"

char *eval_filename = "EVALCMD";


int eval(int argc, char **argv)
{
    set_exit_status(0, 0);
    if(argc == 1)
    {
        return 0;
    }
    
    char   *cmd  = NULL;
    int     i    = 1;
    size_t  len  = 0;
    for( ; i < argc; i++) len += strlen(argv[i])+1;
    len++;
    cmd = (char *)malloc(len);
    if(!cmd)
    {
        fprintf(stderr, "eval: insufficient memory\r\n");
        return 1;
    }
    i = 1;
    strcpy(cmd, argv[i++]);
    for( ; i < argc; i++)
    {
        strcat(cmd, argv[i]);
        strcat(cmd, " "    );
    }
    cmd[len-2] = '\0';
    
    struct source_s save_src = __src;
    __src.buffer   = cmd;
    __src.bufsize  = len-1;
    __src.filename = eval_filename;
    __src.curpos   = -2;
    do_cmd();
    __src          = save_src;
    return exit_status;
}
