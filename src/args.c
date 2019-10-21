/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: args.c
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
#include <string.h>
#include <errno.h>
#include "cmd.h"
#include "symtab/symtab.h"
#include "debug.h"

/* original argv passed to the shell on startup */
char **shell_argv;

/* original argc passed to the shell on startup */
int    shell_argc;


/*
 * create a duplicate argv list from the given argv list.
 *
 * returns the duplicate argv list, NULL on error.
 */
char **dup_argv(int argc, char **argv)
{
    char **argv2 = malloc((argc+1) * sizeof(char *));
    if(!argv2)
    {
        return NULL;
    }
    int   i;
    for(i = 0; i < argc; i++)
    {
        argv2[i] = get_malloced_str(argv[i]);
    }
    argv2[i] = NULL;
    return argv2;
}


/*
 * free the given argv list.
 */
void free_argv(char ***__argv)
{
    char **argv = *__argv;
    if(!argv)
    {
        return;
    }
    int i = 0;
    for( ; argv[i]; i++)
    {
        if(argv[i])
        {
            free_malloced_str(argv[i]);
        }
    }
    free(argv);
    *__argv = NULL;
}

/*
 * test to see if the passed argvs are the same. this is used by
 * parse_args() to check if the newly passed argv is the same as 
 * the old saved argv, so that it will continue parsing from where
 * it stopped.
 *
 * returns 1 if the two argv arrays are the same, 0 otherwise.
 */
int same_argv(char **argv1, char **argv2)
{
    int i, j;
    for(i = 0, j = 0; argv1[i]; i++, j++)
    {
        /* argv1 is longer than argv2 */
        if(!argv2[j])
        {
            return 0;
        }
        /* argv1 and argv2 are not the same */
        if(strcmp(argv1[i], argv2[j]) != 0)
        {
            return 0;
        }
    }
    /* argv2 is longer than argv1 */
    if(argv2[j])
    {
        return 0;
    }
    /* if all tests passed, argv1 and argv2 are the same */
    return 1;
}

/*
 * parse arguments passed to the builtin utilities.
 * parameters are:
 *   __argc     : argc for the utility
 *   __argv     : argv for the utility
 *   __ops      : options string that the utility accepts as legal options
 *   *__argi    : pointer to an integer in which we will save the index of
 *                the next argument after the options finish, or the index 
 *                of current argument in case the caller expects an arg-option
 *   errexit    : if set, report unknown options
 * 
 * return values:
 *   -1         : in case of illegal option
 *   alpha      : the letter representing the selected option
 *   0          : options finished. index of the first operand stored in *__argi
 */
char *__optarg = 0;
char  __opterr = 0;
int   argi     = 0;
// #define INVALID_OPTARG      ((char *)-1)

int parse_args(int __argc, char **__argv, char *__ops, int *__argi, int errexit)
{
    static char  *ops  = NULL;
    static char **argv = NULL;
    static int    argc = 0;
    static char  *p    = NULL;
    /* call from a new utility? */
    int new = 0;
    if(!argi || !argv)
    {
        new = 1;
    }
    else
    {
        if(argc != __argc)
        {
            new = 1;
        }
        else if(!same_argv(argv, __argv))
        {
            new = 1;
        }
    }
    
    if(new)
    {
        /*
         * we should have at least 2 args, the first is the name of
         * the calling utility.
         */
        if(__argc == 1)
        {
            free_argv(&argv);
            return *__argi = 1, 0;
        }
        struct symtab_entry_s *OPTIND = get_symtab_entry("OPTIND");
        if(!OPTIND || !OPTIND->val)
        {
            argi = 1;
        }
        else
        {
            argi = atoi(OPTIND->val);
            if(!argi)
            {
                argi = 1;
            }
            else if(argi > __argc)
            {
                argi = __argc;
            }
        }
        if(argv)
        {
            free_argv(&argv);   /* forget the old saved argv */
        }
        ops  = __ops;
        argv = dup_argv(__argc, __argv);
        if(!argv)
        {
            fprintf(stderr, "%s: failed to process argv: %s\n", argv[0], strerror(errno));
            free_argv(&argv);
            /* POSIX says non-interactive shell should exit on Utility syntax errors */
            if(!option_set('i'))
            {
                exit_gracefully(EXIT_FAILURE, NULL);
            }
            return -1;
        }
        argc = __argc;
        p    = argv[argi];
    }
    if(ops != __ops && *ops == ':')
    {
        argi++;
    }
    
loop:
    if(p == argv[argi])
    {
        if(!p)
        {
            free_argv(&argv);
            return *__argi = argi, 0;
        }
        if(*p != '-')
        {
            /* only accept '+' options when option string begins with '+' */
            if(*p != '+' || *__ops != '+')
            {
                free_argv(&argv);
                return *__argi = argi, 0;
            }
        }
        if(strcmp(p, "--") == 0)
        {
            free_argv(&argv);
            return *__argi = argi+1, 0;
        }
    }
    ops      = __ops;
    __optarg = NULL;
    while(*++p)
    {
        while(*ops)
        {
            if(*ops == ':')
            {
                ops++;
                continue;
            }
            if(*p == *ops)
            {
                char c = *p;
                if(ops[1] == ':')
                {
                    /* take the rest of this option string as the argument */
                    if(p[1])
                    {
                        __optarg = p+1;
                        p = argv[++argi];
                    }
                    /* take the next argument as the option argument */
                    else
                    {
                        if(++argi >= argc)
                        {
                            __optarg = INVALID_OPTARG;
                            __opterr = *p;
                        }
                        else
                        {
                            /* argument starts with '-'? this is an option, not an argument */
                            if(argv[argi][0] == '-')
                            {
                                __optarg = INVALID_OPTARG;
                                __opterr = *p;
                            }
                            else
                            {
                                __optarg = argv[argi++];
                            }
                            p = argv[argi];
                        }
                    }
                }
                return *__argi = argi, (int)c;
            }
            ops++;
        }
        __opterr = *p;
        if(!errexit)
        {
            free_argv(&argv);
            return *__argi = argi, -1;
        }
        fprintf(stderr, "%s: unknown option: %c\n", argv[0], *p);
        free_argv(&argv);
        /* POSIX says non-interactive shell should exit on Utility syntax errors */
        if(!option_set('i'))
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }
        return -1;
    }
    
    if(++argi < argc)
    {
        p   = argv[argi];
        goto loop;
    }
    free_argv(&argv);
    return *__argi = argi, 0;
}
