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

struct callframe_s *cur_callframe  = NULL;
struct callframe_s *zero_callframe = NULL;


struct callframe_s *callframe_new(char *funcname, char *srcfile, int lineno)
{
    struct callframe_s *cf = malloc(sizeof(struct callframe_s));
    if(!cf) return NULL;
    cf->funcname = get_malloced_str(funcname);
    cf->srcfile  = get_malloced_str(srcfile );
    cf->lineno   = lineno;
    cf->prev     = NULL;
    return cf;
}

int callframe_push(struct callframe_s *cf)
{
    if(!cf) return 0;
    if(!zero_callframe) zero_callframe = cf;
    cf->prev = cur_callframe;
    cur_callframe = cf;
    return 1;
}

struct callframe_s *callframe_pop()
{
    if(!cur_callframe) return NULL;
    struct callframe_s *cf = cur_callframe;
    cur_callframe = cur_callframe->prev;
    if(!cur_callframe) zero_callframe = NULL;
    return cf;
}

void callframe_popf()
{
    if(!cur_callframe) return;
    struct callframe_s *cf = cur_callframe;
    cur_callframe = cur_callframe->prev;
    if(!cur_callframe) zero_callframe = NULL;
    if(cf->funcname) free_malloced_str(cf->funcname);
    if(cf->srcfile ) free_malloced_str(cf->srcfile );
    free(cf);
}

int get_callframe_count()
{
    int count = 0;
    struct callframe_s *cf = cur_callframe;
    while(cf) count++, cf = cf->prev;
    return count;
}


int caller(int argc, char **argv)
{
    int level = -1;
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
    if(!cur_callframe) return 1;
    
    if(level == -1) printf("%d %s\n", cur_callframe->lineno, cur_callframe->srcfile);
    else
    {
        struct callframe_s *cf = cur_callframe;
        while(level-- && cf) cf = cf->prev;
        if(cf == NULL)
        {
            fprintf(stderr, "%s: invalid callframe number: %s\n", UTILITY, argv[1]);
            return 2;
        }
        printf("%d %s %s\n", cf->lineno, cf->funcname, cf->srcfile);
    }
    return 0;
}
