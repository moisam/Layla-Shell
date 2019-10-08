/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: caller.c
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

#define UTILITY         "caller"

/*
 * in this file, we handle the call stack. we don't actually implement a
 * full-blown call stack for function calls in here, we simply store the function
 * name, the name of the file in which the function was defined, and a pointer
 * to the previous function in the call stack (see the definition of 
 * 'struct callframe_s' in ../cmd.h). the stack itself is a simple linked list.
 * the top of the stack represents the last function call (the function that is
 * currently executing), while the bottom of the stack is always the 'main' function,
 * or the shell itself.
 */

struct callframe_s *cur_callframe  = NULL;      /* the current call frame */
struct callframe_s *zero_callframe = NULL;      /* the very first call frame */


/*
 * create a new callframe for a function call, given the function's name,
 * source file name and line number where it was declared.
 * returns a struct callframe_s pointer if the callframe is created successfully,
 * NULL otherwise.
 */
struct callframe_s *callframe_new(char *funcname, char *srcfile, int lineno)
{
    struct callframe_s *cf = malloc(sizeof(struct callframe_s));
    if(!cf)
    {
        return NULL;
    }
    cf->funcname = funcname ? get_malloced_str(funcname) : NULL;
    cf->srcfile  = srcfile ? get_malloced_str(srcfile) : NULL;
    cf->lineno   = lineno;
    cf->prev     = NULL;
    return cf;
}


/*
 * get a pointer to the current callframe.
 * returns the current callframe, or NULL if the stack is empty.
 */
struct callframe_s *get_cur_callframe()
{
    return cur_callframe;
}


/*
 * push a callframe on top the call stack.
 * returns 1 on success, 0 on error.
 */
int callframe_push(struct callframe_s *cf)
{
    if(!cf)
    {
        return 0;
    }
    /* first callframe ever */
    if(!zero_callframe)
    {
        zero_callframe = cf;
    }
    /* link to the previous callframe */
    cf->prev = cur_callframe;
    /* make the new callframe the current one (i.e. push on top the stack) */
    cur_callframe = cf;
    return 1;
}


/*
 * pop a callframe off the call stack.
 * returns the popped callframe, or NULL if the stack is empty.
 */
struct callframe_s *callframe_pop()
{
    if(!cur_callframe)
    {
        return NULL;
    }
    /* get the current callframe (top of the stack) */
    struct callframe_s *cf = cur_callframe;
    /* pop it off the stack */
    cur_callframe = cur_callframe->prev;
    /* if last callframe on the stack, reset our zero_callframe pointer */
    if(!cur_callframe)
    {
        zero_callframe = NULL;
    }
    /* return the popped callframe */
    return cf;
}


/*
 * pop a callframe off the call stack and free its structure.
 * doesn't return anything as the popped callframe is freed.
 */
void callframe_popf()
{
    if(!cur_callframe)
    {
        return;
    }
    struct callframe_s *cf = cur_callframe;
    cur_callframe = cur_callframe->prev;
    if(!cur_callframe)
    {
        zero_callframe = NULL;
    }
    /* free the memory used by the callframe */
    if(cf->funcname)
    {
        free_malloced_str(cf->funcname);
    }
    if(cf->srcfile )
    {
        free_malloced_str(cf->srcfile );
    }
    free(cf);
}


/*
 * return the number of callframes in the stack, which equals the
 * number of nested function calls executed by the shell.
 */
int get_callframe_count()
{
    int count = 0;
    struct callframe_s *cf = cur_callframe;
    while(cf)
    {
        count++, cf = cf->prev;
    }
    return count;
}


/*
 * the caller builtin utility (non-POSIX bash extension).
 * 
 * for usage, see the manpage, or run: `help caller` from lsh prompt to
 * see a short explanation on how to use this utility.
 */
int caller(int argc, char **argv)
{
    int level = -1;
    /* if an argument is supplied, it gives the callframe number the user wants */
    if(argc > 1)
    {
        char *s;
        level = strtol(argv[1], &s, 10);
        if(s == argv[1])
        {
            fprintf(stderr, "%s: invalid callframe number: %s\n", UTILITY, argv[1]);
            return 2;
        }
    }
    /* empty stack, return error */
    if(!cur_callframe)
    {
        return 1;
    }
    
    /* -1 means last (current) callframe, or top of the stack */
    if(level == -1)
    {
        printf("%d %s\n", cur_callframe->lineno, cur_callframe->srcfile);
    }
    else
    {
        /*
         * start from the top of the stack and count backwards, until we
         * reach the requested callframe.
         */
        struct callframe_s *cf = cur_callframe;
        while(level-- && cf)
        {
            cf = cf->prev;
        }
        /* the number requested is out of the stack bounds */
        if(cf == NULL)
        {
            fprintf(stderr, "%s: invalid callframe number: %s\n", UTILITY, argv[1]);
            return 2;
        }
        printf("%d %s %s\n", cf->lineno, cf->funcname, cf->srcfile);
    }
    return 0;
}
