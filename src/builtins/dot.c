/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: dot.c
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
#include "../symtab/symtab.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY         "dot"


/*
 * the dot builtin utility (POSIX).. used to execute dot scripts.
 *
 * returns the exit status of the last command executed.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help dot` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int dot(int argc, char **argv)
{
    /*
     * strictly POSIX speaking, dot must have only two arguments.. but ksh has a
     * very useful extension where you can supply positional parameters are 
     * additional arguments.. we use the ksh-style here.
     */
    /* first check we have exactly 2 args if we're in --posix mode */
    if(argc != 2 && option_set('P'))
    {
        fprintf(stderr, "%s: incorrect number of arguments\n"
                        "     usage: %s file\n", UTILITY, argv[0]);
        return 1;
    }
    /* then check we have enough args for the ksh-like usage */
    if(argc < 2)
    {
        fprintf(stderr, "%s: incorrect number of arguments\n"
                        "     usage: %s file [args...]\n", UTILITY, argv[0]);
        return 1;
    }
    /* get the dot file path */
    char *file  = argv[1];
    char *path  = NULL;

    /*
     * we will set src.srcname and src.curline et al. in the call to
     * read_file() below.
     */
    struct source_s src;
    src.buffer  = NULL;

    /* does the dot filename has slashes in it? */
    if(strchr(file, '/'))
    {
        /* is this shell restricted? */
        if(startup_finished && option_set('r'))
        {
            /* bash says r-shells can't specify commands with '/' in their names */
            fprintf(stderr, "%s: can't execute dot script: restricted shell\n", UTILITY);
            return 2;
        }
        /* try to read the dot file */
        if(!read_file(file, &src))
        {
            goto err;
        }
    }
    /* no slashes in the dot filename */
    else
    {
        /*
         *  search for the dot file using $PATH as per POSIX.. if we are not
         *  running in --posix mode, we check the 'sourcepath' extended option and
         *  only use $PATH if it is set (bash extension).
         */
        if(optionx_set(OPTION_SOURCE_PATH) || option_set('P'))
        {
            path = search_path(file, NULL, 0);
        }
        else
        {
            path = file;
        }
        /*
         * in case of $PATH failure, bash searches CWD (when not in --posix mode), which is 
         * not required by POSIX.. we don't do the search if we are not running in --posix
         * mode either.
         */
        if(!path && !option_set('P'))
        {
            char tmp[strlen(file)+3];
            sprintf(tmp, "./%s", file);
            path = get_malloced_str(tmp);
        }
        /* still no luck? */
        if(!path)
        {
            fprintf(stderr, "%s: failed to find file: %s\n", UTILITY, file);
            return 127;
        }
        /* try to read the dot file */
        if(!read_file(path, &src))
        {
            goto err;
        }
        /* free temp memory */
        if(path != file)
        {
            free_malloced_str(path);
        }
    }
    src.srctype = SOURCE_DOTFILE;
    set_exit_status(0);
    
    /* save current positional parameters */
    char **pos = NULL;
    /* set the new positional parameters, if any */
    int i, count = argc-2;
    char buf[32];
    /* we have new positional parameters */
    if(count)
    {
        /* save a copy of the current positional parameters list */
        pos = get_pos_paramsp();
        /* and replace them with the new positional parameters */
        for(i = 2; i < argc; i++)
        {
            sprintf(buf, "%d", i-1);
            set_shell_varp(buf, argv[i]);
        }
        /*
         * if the new positional parameters list is shorted than the old one,
         * set the rest of the old parameters list to NULL.
         */
        int count2 = pos_param_count();
        for( ; i <= count2; i++)
        {
            sprintf(buf, "%d", i);
            set_shell_varp(buf, NULL);
        }
    }
    sprintf(buf, "%d", count);
    set_shell_varp("#", buf);
    
    /* reset the OPTIND variable */
    set_shell_varp("OPTIND", "1");
    
    /* save the DEBUG trap (bash behavior) */
    struct trap_item_s *debug = NULL;
    if(!option_set('T'))
    {
        debug = save_trap("DEBUG");
    }
    
    /* add a new entry to the callframe stack to reflect the new scope we're entering */
    callframe_push(callframe_new(file, src.srcname, src.curline));

    /* now execute the dot script */
    parse_and_execute(&src);
    
    /* bash executes RETURN traps when dot script finishes */
    trap_handler(RETURN_TRAP_NUM);

    /* pop the callframe entry we've added to the stack */
    callframe_popf();
    
    /*
     * if the dot script changed the DEBUG trap, keep the change and 
     * free the old DEBUG trap (also bash behavior).
     */
    if(!option_set('T'))
    {
        struct trap_item_s *debug2 = get_trap_item("DEBUG");
        if(debug2 && debug2->action_str)
        {
            if(debug->action_str)
            {
                free_malloced_str(debug->action_str);
            }
            free(debug);
        }
    }
    
    /* and restore positional parameters */
    if(pos)
    {
        set_pos_paramsp(pos);
        /* free the memory used by the temp list */
        for(i = 0; pos[i]; i++)
        {
            free(pos[i]);
        }
        free(pos);
    }
    free(src.buffer);
    /* and return */
    return exit_status;
    
err:
    fprintf(stderr, "%s: failed to read '%s': %s\n", UTILITY, file, strerror(errno));
    if(path)
    {
        free_malloced_str(path);
    }
    if(src.buffer)
    {
        free(src.buffer);
    }
    return EXIT_ERROR_NOENT;    /* exit status: 127 */
}
