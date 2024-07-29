/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: return.c
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
#include "../include/cmd.h"
#include "../backend/backend.h"

int return_set = 0;


/*
 * The return builtin utility (non-POSIX). Used to return from functions and
 * dot scripts.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help return` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int return_builtin(int argc, char **argv)
{
    /*
     * NOTE: if we are not running in --posix mode and there are more than two args,
     *       we will just ignore the extra ones.
     */
    
    if(option_set('P') && argc > 2)
    {
        PRINT_ERROR("return", "too many arguments");
        return_set = 1;
        return 2;
    }

    int res = 0;
    if(argc > 1)
    {
        char *strend;
        res = strtol(argv[1], &strend, 10);
        if(strend == argv[1])
        {
            PRINT_ERROR("return", "invalid return code: %s", argv[1]);
            res = 2;
        }
        else
        {
            res &= 0xff;
        }
    }
    else
    {
        res = exit_status;
    }
    
    /* set the return flag so the other functions will know we've encountered return */
    return_set = 1;
    return res;
}
