/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include <sys/stat.h>
#include "../cmd.h"

#define UTILITY         "umask"

static mode_t _umask = 0;
int cur_perm = 0;

void init_umask()
{
    _umask = umask(0);
    umask(_umask);
}

#define WHO_NOBODY      (0     )
#define WHO_USER        (1 << 0)
#define WHO_GROUP       (1 << 1)
#define WHO_OTHER       (1 << 2)

int get_who(char **__str)
{
    char *str = *__str;
    if(!*str) return WHO_NOBODY;
    int who = WHO_NOBODY;
loop:
    if     (*str == 'u') who |= WHO_USER;
    else if(*str == 'g') who |= WHO_GROUP;
    else if(*str == 'o') who |= WHO_OTHER;
    else if(*str == 'a') who |= (WHO_USER|WHO_GROUP|WHO_OTHER);
    else
    {
        *__str = str;
        return who;
    }
    str++;
    goto loop;
}

#define ACTION_NO_ACTION        (0     )
#define ACTION_INVALID_ACTION   (1 << 0)
#define ACTION_IMPLIED_ACTION   (1 << 1)
#define ACTION_SET_ACTION       (1 << 2)    /* '=' */
#define ACTION_CLEAR_ACTION     (1 << 3)    /* '-' */
#define ACTION_ADD_ACTION       (1 << 4)    /* '+' */

// #define EXTRA_SUID_SGID         (1 << 0)
// #define EXTRA_VTX               (1 << 1)

static inline int isperm(char c)
{
    if(c == 'r' || c == 'w' || c == 'x' ||
       c == 'X' || c == 's' || c == 't')
        return 1;
    return 0;
}

static inline int isop(char c)
{
    if(c == '+' || c == '-' || c == '=') return 1;
    return 0;
}

int get_action(char **__str, int *__perm /*, int *extra */)
{
    char *str = *__str;
    if(!*str) return ACTION_NO_ACTION;
    char op = *str++;
    if(!isop(op))
    {
        fprintf(stderr, "%s: unknown operator -- %c\r\n", UTILITY, op);
        return ACTION_INVALID_ACTION;
    }
    int perm = 0;
    //*extra   = 0;
    /* 1- POSIX permcopy */
    if(*str == 'u' || *str == 'g' || *str == 'o')
    {
        if     (*str == 'u') perm = (cur_perm & S_IRWXU) >> 6;
        else if(*str == 'g') perm = (cur_perm & S_IRWXG) >> 3;
        else if(*str == 'o') perm =  cur_perm & S_IRWXO;
    }
    /* 2- POSIX permlist */
    else if(isperm(*str))
    {
        do
        {
            /* 
             * we take 'other' permissions as they are the
             * common denominator (r=4, w=2, x=1).
             */
            if     (*str == 'r') perm |= S_IROTH;
            else if(*str == 'w') perm |= S_IWOTH;
            else if(*str == 'x') perm |= S_IXOTH;
            else if(*str == 'X')
            {
                if((cur_perm & S_IXUSR) || (cur_perm & S_IXGRP) ||
                   (cur_perm & S_IXOTH))
                    perm |= S_IXOTH;
            }
            /*
            else if(*str == 's') *extra |= EXTRA_SUID_SGID;
            else if(*str == 't') *extra |= EXTRA_VTX;
            */
            else
            {
                fprintf(stderr, "%s: unknown permission bit -- %c\r\n", UTILITY, *str);
                return ACTION_INVALID_ACTION;
            }
        } while(isperm(*++str));
    }
    else if(*str == ',' || *str == '\0' || isop(*str))
    {
        /*
        *__str = str;
        return ACTION_IMPLIED_ACTION;
        */
        goto fin;
    }
    else
    {
        fprintf(stderr, "%s: unknown permission bit -- %c\r\n", UTILITY, *str);
        return ACTION_INVALID_ACTION;
    }
fin:
    *__str  = str;
    *__perm = perm;
    if(op == '+') return ACTION_ADD_ACTION;
    if(op == '-') return ACTION_CLEAR_ACTION;
    return ACTION_SET_ACTION;
}


int __umask(int argc, char **argv)
{
    int   symb_output = 0;
    char *permstr     = NULL;
    int   who         = WHO_NOBODY;
    int   action      = 0;
    int   new_perm    = 0;
    //int   extra_perm = 0;
    cur_perm = ~(_umask) & 0777;

    
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c, format = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvSp", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_UMASK, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'S':
                symb_output = 1;
                break;
                
            case 'p':
                format = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 1;
    
    /* only output the current mask and return */
    if(v >= argc)
    {
        if(!symb_output)
        {
            if(format) printf("umask ");
            printf("0%03o\r\n", _umask);
        }
        else
        {
            if(format) printf("umask -S ");
            printf("u=");
            if(!(_umask & S_IRUSR)) putchar('r');
            if(!(_umask & S_IWUSR)) putchar('w');
            if(!(_umask & S_IXUSR)) putchar('x');
            putchar(',');
            printf("g=");
            if(!(_umask & S_IRGRP)) putchar('r');
            if(!(_umask & S_IWGRP)) putchar('w');
            if(!(_umask & S_IXGRP)) putchar('x');
            putchar(',');
            printf("o=");
            if(!(_umask & S_IROTH)) putchar('r');
            if(!(_umask & S_IWOTH)) putchar('w');
            if(!(_umask & S_IXOTH)) putchar('x');
            putchar('\r');
            putchar('\n');
        }
        return 0;
    }
    
    permstr = argv[v];
    /* octal integer mode */
    if(*permstr == '0')
    {
        new_perm = 0;
        while(*++permstr)
        {
            if(*permstr >= '0' && *permstr <= '7')
                new_perm = (new_perm*8) + (*permstr-'0');
            else
            {
                fprintf(stderr, "%s: illegal octal mode: %s\r\n", UTILITY, permstr);
                return 1;
            }
        }
        /* forget about the SUID and SGID bits */
        new_perm &= 0777;
        _umask   = new_perm;
        goto fin;
    }
    
    /* symbolic mode */
    while(*permstr)
    {
        /* 1- get WHO */
        if(*permstr == '+' || *permstr == '-' || *permstr == '=')
            who = (WHO_USER|WHO_OTHER|WHO_GROUP);
        else if(*permstr == 'u' || *permstr == 'g' ||
                *permstr == 'o' || *permstr == 'a')
            who = get_who(&permstr);
        else
        {
            fprintf(stderr, "%s: unknown who/action -- %c\r\n", UTILITY, *permstr);
            break;
        }
        
        
        /* 2- get ACTIONs */
        while(isop(*permstr))
        {
            if((action = get_action(&permstr, &new_perm /*, &extra_perm */))
                        == ACTION_INVALID_ACTION)
            {
                return 2;
            }
            
            
            /* 3- apply ACTION */
            if(new_perm == 0 && action != ACTION_SET_ACTION) continue;
            //if(action == ACTION_IMPLIED_ACTION) continue;
            if(who & WHO_USER)
            {
                int perm = (new_perm << 6);
                if(action == ACTION_ADD_ACTION) cur_perm |= perm;
                else
                {
                    if(action == ACTION_SET_ACTION)
                    {
                        cur_perm &= 0077; //07077;
                        cur_perm |= perm;
                    }
                    else if(perm)
                    {
                        //cur_perm &= 0077; //07077;
                        cur_perm &= ~perm;
                    }
                }
                //if(extra_perm & EXTRA_SUID_SGID) cur_perm |= S_ISUID;
            }
            if(who & WHO_GROUP)
            {
                int perm = (new_perm << 3);
                if(action == ACTION_ADD_ACTION) cur_perm |= perm;
                else
                {
                    if(action == ACTION_SET_ACTION)
                    {
                        cur_perm &= 0707; //07707;
                        cur_perm |= perm;
                    }
                    else if(perm)
                    {
                        //cur_perm &= 0707; //07707;
                        cur_perm &= ~perm;
                    }
                }
                //if(extra_perm & EXTRA_SUID_SGID) cur_perm |= S_ISGID;
            }
            if(who & WHO_OTHER)
            {
                int perm = (new_perm << 0);
                if(action == ACTION_ADD_ACTION) cur_perm |= perm;
                else
                {
                    if(action == ACTION_SET_ACTION)
                    {
                        cur_perm &= 0770; //07770;
                        cur_perm |= perm;
                    }
                    else if(perm)
                    {
                        //cur_perm &= 0770; //07770;
                        cur_perm &= ~perm;
                    }
                }
            }
        }
        //if(action == ACTION_INVALID_ACTION) break;        
        while(*permstr == ',') permstr++;
    }
    _umask = ~cur_perm;
    _umask &= 0777;
    
fin:
    umask(_umask);
    return 0;
}
