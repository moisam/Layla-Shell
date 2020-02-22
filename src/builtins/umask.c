/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: umask.c
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

#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include "builtins.h"
#include "../cmd.h"

#define UTILITY         "umask"

/* umask value we will use when setting the umask in umask_builtin() below */
int cur_perm = 0;

/* constant values to represent the user, group and others */
#define WHO_USER                (1 << 0)
#define WHO_GROUP               (1 << 1)
#define WHO_OTHER               (1 << 2)

/* constant values to represent the actions we can perform on umask fields */
#define ACTION_INVALID_ACTION   (1 << 0)
#define ACTION_SET_ACTION       (1 << 1)    /* '=' */
#define ACTION_CLEAR_ACTION     (1 << 2)    /* '-' */
#define ACTION_ADD_ACTION       (1 << 3)    /* '+' */


/*
 * convert a symbolic string of the chars u (user), g (group), o (others),
 * and a (all) to a numeric value that is easy to parse.
 */
int get_who(char **str)
{
    int who = 0;
    char *str2 = (*str);
    while(*str2)
    {
        switch(*str2)
        {
            case 'u':
                who |= WHO_USER;
                break;

            case 'g':
                who |= WHO_GROUP;
                break;

            case 'o':
                who |= WHO_OTHER;
                break;

            case 'a':
                who |= (WHO_USER|WHO_GROUP|WHO_OTHER);
                break;

            default:
                (*str) = str2;
                return who;
        }
        str2++;
    }

    return who;
}


/*
 * process the symbolic permission string str, which should contain
 * one or more chars (r, w, x), representing the permissions we should set.
 *
 * the result permissions field is placed in perm, and consists of a
 * combination of read (04), write (02), and execute (01) perm bits.
 *
 * return value can be:
 * - ACTION_ADD_ACTION if the permissions bits should be added to the
 *     current bits
 * - ACTION_CLEAR_ACTION if the permissions bits should cleared
 * - ACTION_SET_ACTION if the permissions bits should be set exactly to the bits
 *     we have put in perm
 * - ACTION_INVALID_ACTION if str contains an invalid value.
 */
int get_perm(char **str, int *perm)
{
    if(!str || !*str || !**str)
    {
        return ACTION_INVALID_ACTION;
    }

    char *str2 = (*str);
    char op = *str2++;
    int perm2 = 0, endme = 0;

    if(op != '+' && op != '-' && op != '=')
    {
        PRINT_ERROR("%s: unknown operator -- %c\n", UTILITY, op);
        return ACTION_INVALID_ACTION;
    }

    switch(*str2)
    {
        case 'r':
        case 'w':
        case 'x':
        case 'X':
            do
            {
                /* 
                 * we take 'other' permissions as they are the
                 * common denominator (r=4, w=2, x=1).
                 */
                switch(*str2)
                {
                    case 'r':
                        perm2 |= S_IROTH;
                        break;

                    case 'w':
                        perm2 |= S_IWOTH;
                        break;

                    case 'x':
                        perm2 |= S_IXOTH;
                        break;

                    case 'X':
                        if((cur_perm & S_IXUSR) || (cur_perm & S_IXGRP) ||
                           (cur_perm & S_IXOTH))
                            perm2 |= S_IXOTH;
                        break;

                    case ',':
                        endme = 1;
                        break;
                        
                    default:
                        PRINT_ERROR("%s: unknown permission bit -- %c\n", UTILITY, *str2);
                        return ACTION_INVALID_ACTION;
                }
                
                if(endme)
                {
                    break;
                }
            } while((*++str2));
            break;

        default:
            PRINT_ERROR("%s: unknown permission bit -- %c\n", UTILITY, *str2);
            return ACTION_INVALID_ACTION;
    }

    (*str) = str2;
    (*perm) = perm2;

    if(op == '+')
    {
        return ACTION_ADD_ACTION;
    }

    if(op == '-')
    {
        return ACTION_CLEAR_ACTION;
    }
    
    return ACTION_SET_ACTION;
}


/*
 * the umask builtin utility (POSIX).. used to print and set the shell umask.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help umask` or `umask -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int umask_builtin(int argc, char **argv)
{
    int   symb_output = 0;
    char *permstr     = NULL;
    int   who         = 0;
    int   action      = 0;
    int   new_perm    = 0;
    int   v = 1, c, format = 0;

    mode_t cur_umask = umask(0);
    umask(cur_umask);
    cur_perm = ~(cur_umask) & 0777;
    
    /*
     * recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "S" : "hvpS";

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &UMASK_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* print a symbolic output */
            case 'S':
                symb_output = 1;
                break;
                
            /* print 'umask' in front of the umask */
            case 'p':
                format = 1;
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 1;
    }
    
    /* no arguments. output the current mask and return */
    if(v >= argc)
    {
        if(!symb_output)
        {
            /* normal output (octal number) */
            if(format)
            {
                printf("umask ");
            }
            printf("0%03o\n", cur_umask);
        }
        else
        {
            /* symbolic output */
            if(format)
            {
                printf("umask -S ");
            }
            
            char *letters = "ugo";
            /* print the permission bits for u, g, o */
            for(v = 0; v < 3; v++)
            {
                printf("%c=", letters[v]);
                
                /* print the permision bits */
                if(cur_perm & S_IRUSR)
                {
                    putchar('r');
                }
            
                if(cur_perm & S_IWUSR)
                {
                    putchar('w');
                }
            
                if(cur_perm & S_IXUSR)
                {
                    putchar('x');
                }
                
                /* print the separator char */
                if(v == 0 || v == 1)
                {
                    putchar(',');
                }
                else
                {
                    putchar('\n');
                }
                
                /* check the next set of permission bits */
                cur_perm <<= 3;
            }
        }
        return 0;
    }

    /* get the new umask value */
    permstr = argv[v];
    
    /* octal integer mode */
    if(isdigit(*permstr))
    {
        new_perm = 0;
        do
        {
            if(*permstr >= '0' && *permstr <= '7')
            {
                new_perm = (new_perm * 8) + ((*permstr) - '0');
            }
            else
            {
                PRINT_ERROR("%s: illegal octal mode: %s\n", UTILITY, permstr);
                return 1;
            }
        } while(*++permstr);
        cur_umask = new_perm;
    }
    else
    {
        /* symbolic mode */
        while(*permstr)
        {
            /* 1- get the WHO */
            if(*permstr == '+' || *permstr == '-' || *permstr == '=')
            {
                who = (WHO_USER|WHO_OTHER|WHO_GROUP);
            }
            else if(*permstr == 'u' || *permstr == 'g' ||
                    *permstr == 'o' || *permstr == 'a')
            {
                who = get_who(&permstr);
            }
            else
            {
                PRINT_ERROR("%s: unknown who/action -- %c\n", UTILITY, *permstr);
                return 1;
            }
        
            /* 2- get the ACTIONs */
            while(*permstr == '+' || *permstr == '-' || *permstr == '=')
            {
                if((action = get_perm(&permstr, &new_perm)) == ACTION_INVALID_ACTION)
                {
                    return 2;
                }
            
                /* 3- apply the ACTION */
                if(new_perm == 0 && action != ACTION_SET_ACTION)
                {
                    /* we can't apply + or - actions to an empty permission mask */
                    continue;
                }
                
                int mask_who = WHO_OTHER;
                int mask_perm = 0007;
                int perm = new_perm;
                
                for(v = 0; v < 3; v++)
                {
                    if(who & mask_who)
                    {
                        switch(action)
                        {
                            case ACTION_ADD_ACTION:
                                cur_perm |= perm;
                                break;
                                
                            case ACTION_SET_ACTION:
                                cur_perm &= ~mask_perm;
                                cur_perm |= perm;
                                break;
                                
                            default:
                                cur_perm &= ~perm;
                                break;
                        }
                    }
                    
                    mask_who >>= 1;
                    mask_perm <<= 3;
                    perm <<= 3;
                }
            }

            /* skip any separator commans */
            while(*permstr == ',')
            {
                permstr++;
            }
        }
        cur_umask = ~cur_perm;
    }

    /* set the umask (umask's manpage says it ANDs the mask with 0777 for us) */
    umask(cur_umask);
    return 0;
}
