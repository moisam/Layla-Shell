/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: source.c
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
#include "builtins.h"
#include "../include/cmd.h"
#include "setx.h"
#include "../include/debug.h"

int do_source_script(char *utility, char *file, int argc, char **argv);


/*
 * The source builtin utility (non-POSIX). Used to source (read and execute) a script file,
 * similar to what the dot builtin utility does.
 *
 * Returns the exit status of the last command executed.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help source` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int source_builtin(int argc, char **argv)
{
    /*
     * This utility does the same work as dot, but we have some options that we
     * borrowed from tcsh's source, which are not supported by dot (ksh, bash).
     * That's why we parse our options here, then call dot to do the actual
     * sourcing.
     */
    int v = 1, c;
    while((c = parse_args(argc, argv, "h:v", &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            /*
             * in tcsh, the -h option causes commands to be loaded into the history
             * list, much like 'history -L'.
             */
            case 'h':
                if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                {
                    OPTION_REQUIRES_ARG_ERROR("source", 'h');
                    return 2;
                }

                char *argv2[] = { "history", "-L", internal_optarg, NULL };
                
                return do_builtin_internal(history_builtin, 3, argv2);
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* check we have a filename */
    if(v >= argc)
    {
        PRINT_ERROR(argv[0], "missing argument: file name\n"
                    "usage: %s file [args...]", argv[0]);
        return 2;
    }
    
    return do_source_script("source", argv[v], argc-(v+1), &argv[v+1]);
}


/*
 * Load and execute the shell script given in the 'file' argument, passing it
 * the arguments indicated by 'argc' and 'argv'. The 'utility' argument
 * contains the name of the calling utility (source or dot).
 * 
 * Returns the exit status of the last command executed in the script, or 
 * non-zero in case of error.
 */
int do_source_script(char *utility, char *file, int argc, char **argv)
{
    /* get the dot file path */
    char *path  = NULL;

    /*
     * Strictly POSIX speaking, dot must have only two arguments. But ksh has a
     * very useful extension where you can supply positional parameters are 
     * additional arguments. We use the ksh-style here.
     */
    int posix_set = option_set('P');
    
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
            PRINT_ERROR(utility, "can't execute dot script: restricted shell");
            return 2;
        }
        path = file;
    }
    /* no slashes in the dot filename */
    else
    {
        /*
         *  Search for the dot file using $PATH as per POSIX. If we are not
         *  running in --posix mode, we check the 'sourcepath' extended option and
         *  only use $PATH if it is set (bash extension).
         */
        if(optionx_set(OPTION_SOURCE_PATH) || posix_set)
        {
            path = search_path(file, NULL, 0);
        }

        /*
         * In case of $PATH failure, bash searches CWD (when not in --posix mode), which is 
         * not required by POSIX. We don't do the search if we are not running in --posix
         * mode either.
         */
        if(!path && !posix_set)
        {
            char tmp[strlen(file)+3];
            sprintf(tmp, "./%s", file);
            path = get_malloced_str(tmp);
        }
        
        /* still no luck? */
        if(!path)
        {
            PRINT_ERROR(utility, "failed to find file: %s", file);
            return EXIT_ERROR_NOENT;
        }
    }
        
    /* try to read the dot file */
    if(!read_file(path, &src))
    {
        PRINT_ERROR(utility, "failed to read `%s`: %s", file, strerror(errno));
        if(path != file)
        {
            free_malloced_str(path);
        }
        return errno == ENOENT ? EXIT_ERROR_NOENT : EXIT_ERROR_NOEXEC;
    }
    
    /* set the input source type after we've read the dot script */
    src.srctype = SOURCE_DOTFILE;

    /* free temp memory */
    if(path != file)
    {
        free_malloced_str(path);
    }
    
    /* set the new positional parameters */
    set_local_pos_params(argc, argv);
    
    /* reset the OPTIND variable */
    set_shell_varp("OPTIND", "1");
    set_shell_varp("OPTSUB", "0");
    
    /* save and reset the DEBUG trap if -T is not set (bash) */
    struct trap_item_s *debug = NULL;
    if(!option_set('T'))
    {
        debug = save_trap("DEBUG");
    }
    
    /* add a new entry to the callframe stack to reflect the new scope we're entering */
    callframe_push(callframe_new(file, src.srcname, src.curline));

    /* now execute the dot script */
    set_internal_exit_status(0);
    parse_and_execute(&src);
    
    /* bash executes RETURN traps when dot script finishes */
    trap_handler(RETURN_TRAP_NUM);

    /* pop the callframe entry we've added to the stack */
    callframe_popf();
    
    /*
     * If -T is not set and the dot script changed the DEBUG trap, keep the 
     * changes and free the old DEBUG trap. Otherwise, reset the trap (bash).
     */
    struct trap_item_s *debug2 = get_trap_item("DEBUG");
    if(debug2 && debug2->action_str)
    {
        if(debug->action_str)
        {
            free_malloced_str(debug->action_str);
        }
        free(debug);
    }
    else
    {
        restore_trap("DEBUG", debug);
    }
    
    /* 
     * NOTE: the positional parameters will be restored when we pop the local
     *       symbol table back in do_simple_command().
     */

    free(src.buffer);

    /* and return */
    return exit_status;
}
