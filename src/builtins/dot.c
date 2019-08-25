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
char *dot_filename = "DOTFILE";


int dot(int argc, char **argv)
{
    /*
     * strictly POSIX speaking, dot must have only two arguments. but ksh has a
     * very useful extension where you can supply positional parameters are 
     * additional arguments. we use the ksh-style here.
     */
    if(argc < 2)
    {
        fprintf(stderr, "%s: incorrect number of arguments\r\n"
                        "     usage: . file [args...]\r\n", UTILITY);
        /* POSIX says non-interactive shell should exit on expansion errors */
        if(!option_set('i')) exit_gracefully(EXIT_FAILURE, NULL);
        return 1;
    }
    
    char *file = argv[1];
    char *path = NULL;
    struct source_s save_src = __src;
    if(strchr(file, '/'))
    {
        /* is this shell restricted? */
        if(startup_finished && option_set('r'))
        {
            /* bash says r-shells can't specify commands with '/' in their names */
            fprintf(stderr, "%s: can't execute dot script: restricted shell\r\n", UTILITY);
            return 2;
        }
        if(!read_file(file, src)) goto err;
    }
    else
    {
        /* bash extension */
        if(optionx_set(OPTION_SOURCE_PATH)) path = search_path(file, NULL, 0);
        else path = file;
        /*
         * in case of $PATH failure, bash searches CWD (when not in --posix mode), which is 
         * not required by POSIX. we don't.
         * NOTE: maybe follow bash in the future?
         */
        if(!path) return 127;
        if(!read_file(path, src)) goto err;
        if(path != file) free_malloced_str(path);
    }
    src->filename = dot_filename;
    set_exit_status(0, 0);
    
    /* save current positional parameters */
    char **pos = NULL;
    /* set the new positional parameters, if any */
    int i, count = argc-2;
    struct symtab_entry_s *entry;
    char buf[32];
    if(count)
    {
        pos = get_pos_paramsp();
        for(i = 2; i < argc; i++)
        {
            sprintf(buf, "%d", i-1);
            entry = add_to_symtab(buf);
            if(entry) symtab_entry_setval(entry, argv[i]);
        }
    }
    entry = add_to_symtab("#");
    if(entry)
    {
        sprintf(buf, "%d", count);
        symtab_entry_setval(entry, buf);
    }
    
    /* reset the OPTIND variable */
    entry = get_symtab_entry("OPTIND");
    if(!entry) entry = add_to_symtab("OPTIND");
    if(entry) symtab_entry_setval(entry, "1");
    
    /* save the DEBUG trap (bash behavior) */
    struct trap_item_s *debug = NULL;
    if(!option_set('T'))
    {
        debug = save_trap("DEBUG");
    }
    
    callframe_push(callframe_new(file, save_src.filename, save_src.curline));

    /* now invoke the dot script */
    do_cmd();
    
    /* bash executes RETURN traps when dot script finishes */
    trap_handler(RETURN_TRAP_NUM);

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
            if(debug->action_str) free_malloced_str(debug->action_str);
            free(debug);
        }
    }
    
    /* and restore pos parameters */
    if(pos)
    {
        set_pos_paramsp(pos);
        for(i = 0; pos[i]; i++) free(pos[i]);
        free(pos);
    }
    /* and return */
    __src = save_src;
    return exit_status;
    
err:
    fprintf(stderr, "%s: failed to read '%s': %s\r\n", UTILITY, file, strerror(errno));
    if(path) free(path);
    /* POSIX says non-interactive shell should exit on special builtin errors */
    if(!option_set('i')) exit_gracefully(127, NULL);
    return 127;
}
