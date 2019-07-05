/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../backend/backend.h"

int return_set = 0;

int __return(int argc, char **argv)
{
    /*
     * NOTE: if there are more than two args, we will just ignore the extra ones.
     */
    
    /*
    if(argc > 2)
    {
        fprintf(stderr, "return: too many arguments\r\n");
        return_set = 1;
        return 2;
    }
    */

    int res = 0;
    if(argc > 1)
    {
        char *strend;
        res = strtol(argv[1], &strend, 10);
        if(strend == argv[1])
        {
            fprintf(stderr, "return: invalid return code: %s\r\n", argv[1]);
            res = 2;
        }
        else res &= 0xff;
    }
    else res = exit_status;
    return_set = 1;
    return res;
}
