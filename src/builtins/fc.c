/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: fc.c
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

#define _XOPEN_SOURCE   500     /* for mkstemp() */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <wait.h>
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY             "fc"

/* internal procedure to output multiline commands */
void __output_multi(char *cmd)
{
    char *p = cmd;
    while(*p)
    {
        putc(*p  , stdout);
        /* indent the next line */
        if(*p == '\n' && p[-1] == '\\')
        {
            putc('\t', stdout);
        }
        p++;
    }
}


/*
 * the fc builtin utility (POSIX).. used to list, edit and rerun commands from
 * the history list.
 *
 * returns the exit status of the last command executed.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help fc` or `fc -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int fc(int argc, char **argv)
{
    
    /*
     * the parser automatically adds new entries to the history list.
     * remove the newest entry, which is the fc command that brought
     * us here.
     */
    char *h = get_last_cmd_history();
    if(h && strstr(h, "fc ") == 0)
    {
        remove_newest();
    }
    
    int   v = 1;
    int   suppress_numbers = 0;     /* -n option */
    int   reverse          = 0;     /* -r option */
    int   list_only        = 0;     /* -l option */
    int   direct_exec      = 0;     /* -s option */
    int   first = 0, last  = 0;
    char *editor           = NULL;  /* used with -e option */
    int   edit_malloc      = 0;
    char *replace_str      = NULL;  /* used with -s option */
    int   hist_total       = cmd_history_end;
    int   prev_command     = hist_total;
    
    /****************************
     * process the options
     ****************************/
    int c, i;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    while((c = parse_args(argc, argv, "hvelnrs", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_FC, 1, 0);
                break;
                
            case 'v':
                printf("%s", shell_ver);
                break;
                
            /* -e specifies the editor to use to edit commands */
            case 'e':
                if(!argv[v] || !argv[v][0])
                {
                    /*
                     * bash uses the default value of the expansion:
                     *     ${FCEDIT:-${EDITOR:-vi}
                     * 
                     * we use a similar expansion down in the file, which equates to:
                     *     ${FCEDIT:-${HISTEDIT:-${EDITOR:-/bin/ed}}}
                     */
                    editor = NULL;
                }
                else
                {
                    editor = search_path(argv[v], NULL, 1);
                    /* remember we malloc'd the editor name so we can free it later */
                    if(editor)
                    {
                        edit_malloc = 1;
                    }
                    v++;
                }
                break;
                
             /* -l lists commands without invoking them */
            case 'l':
                list_only = 1;
                break;
                
            /* -n suppresses output of command numbers */
            case 'n':
                suppress_numbers = 1;
                break;
                
            /* -r reverses the order of printed/edited commands */
            case 'r':
                reverse = 1;
                break;
                
            /* -s executes the commands without invoking the editor */
            case 's':
                direct_exec = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }

    /* get the 'first' operand */
    if(v < argc)
    {
        char *f = argv[v++];
        /* the replace string takes the format str=replacement */
        if(direct_exec && strchr(f, '='))
        {
            replace_str = f;
            if(v >= argc)
            {
                goto check_bounds;
            }
            f = argv[v++];
        }
        /* check if the argument is numeric: -n, +n or n */
        if(*f == '-' || *f == '+' || isdigit(*f))
        {
            first = atoi(f);
        }
        else
        {
            /*
             * if non-numeric argument, use it as a string to search for in the
             * history list.
             */
            for(i = cmd_history_end-1; i >= 0; i--)
            {
                if(strstr(cmd_history[i].cmd, f) == cmd_history[i].cmd)
                {
                    first = i;
                    break;
                }
            }
        }
        /* first is -ve, effectively subtracting from the history list count */
        if(first < 0)
        {
            first  = hist_total+first;
        }
    }
    /* get the 'last' operand */
    if(v < argc)
    {
        char *f = argv[v++];
        if(direct_exec)
        {
            fprintf(stderr, "%s: too many arguments\n", UTILITY);
            if(editor)
            {
                free_malloced_str(editor);
            }
            return 3;
        }
        /* check if the argument is numeric: -n, +n or n */
        if(*f == '-' || *f == '+' || isdigit(*f))
        {
            last = atoi(f);
        }
        else
        {
            /*
             * if non-numeric argument, use it as a string to search for in the
             * history list.
             */
            for(i = cmd_history_end-1; i >= 0; i--)
            {
                if(strstr(cmd_history[i].cmd, f) == cmd_history[i].cmd)
                {
                    last = i;
                    break;
                }
            }
        }
        /* last is -ve, effectively subtracting from the history list count */
        if(last < 0)
        {
            last  = hist_total+last;
        }
    }
    
check_bounds:
    /*
     * check that both the first and last command numbers are within the
     * valid range of the history list.
     *
     * if we are directly executing commands with the -s option, check if we
     * have the first command number.. if we don't, use the last command in
     * the history list.
     */
    if(direct_exec)
    {
        if(first == 0)
        {
            first = prev_command;
        }
        last = first;
    }
    else
    {
        /*
         * we have the 'first' command number but not the 'last'.. if we are
         * listing - not executing - commands, list all commands from 'first'
         * to the last command in the history list.. if we are executing
         * commands, execute only the command whose number is given by the
         * 'first' argument.
         */
        if(last == 0 && first)
        {
            if(list_only)
            {
                last = prev_command;
            }
            else
            {
                last = first;
            }
        }
        /*
         * we don't have the 'first' or 'last' command numbers.. if we are
         * listing - not executing - commands, list the last 16 commands in
         * the history list.. if we are executing commands, execute only the
         * last command in the history list.
         */
        if(last == 0 && first == 0)
        {
            last  = prev_command;
            if(list_only)
            {
                first = last-16;
            }
            else
            {
                first = last;
            }
        }
    }

    /* boundary-check the given ranges */
    if(first < 1)
    {
        first = 1;
    }
    if(last  < 1)
    {
        last  = 1;
    }
    if(first > hist_total)
    {
        first = hist_total;
    }
    if(last  > hist_total)
    {
        last  = hist_total;
    }
    /* reverse the first and last numbers if needed */
    if(first > last)
    {
        int temp = first;
        first    = last;
        last     = temp;
        reverse  = 1;
    }
    /*
     *  our history indices are zero-based, so subtract one from
     *  first and last.
     */
    first--;
    last--;

    /*************************************/
    /* Option 1 - list the commands only */
    /*************************************/
    if(list_only)
    {
        i = 1;
        /* print commands in reverse order */
        if(reverse)
        {
            for( ; last >= first; last--, i++)
            {
                char *cmd = cmd_history[last].cmd;
                /* print the command number in the history list */
                if(!suppress_numbers)
                {
                    printf("%d", i);
                }
                putc('\t', stdout);
                /* print the command */
                if(strstr(cmd, "\\\n"))
                {
                    __output_multi(cmd);
                }
                else
                {
                    printf("%s", cmd);
                }
                /* add newline char if the command doesn't end in \n */
                if(cmd[strlen(cmd)-1] != '\n')
                {
                    putc('\n', stdout);
                }
            }
        }
        /* print commands in normal order */
        else
        {
            for( ; first <= last; first++, i++)
            {
                char *cmd = cmd_history[first].cmd;
                /* print the command number in the history list */
                if(!suppress_numbers)
                {
                    printf("%d", i);
                }
                putc('\t', stdout);
                /* print the command */
                if(strstr(cmd, "\\\n"))
                {
                    __output_multi(cmd);
                }
                else
                {
                    printf("%s", cmd);
                }
                /* add newline char if the command doesn't end in \n */
                if(cmd[strlen(cmd)-1] != '\n')
                {
                    putc('\n', stdout);
                }
            }
        }
        /* free the editor name */
        if(editor)
        {
            free_malloced_str(editor);
        }
        return 0;
    }

    /**************************************/
    /* Option 2 - execute without editing */
    /**************************************/
    if(direct_exec)
    {
        int replace = 1;
        struct source_s save_src = __src;
        for( ; first <= last; first++)
        {
            /*
             * find the first occurrence of replace and substitute it with
             * replace_str.
             */
            if(replace && replace_str)
            {
                char *cmd    = cmd_history[first].cmd;
                char *equals = strchr(replace_str, '=');
                char *old    = replace_str;
                int   oldlen = equals-replace_str;
                *equals = '\0';
                char *new    = equals+1;
                int   newlen = strlen(new);
                int   diff   = newlen > oldlen ? (newlen-oldlen) : 0;
                char  tmp[strlen(cmd)+diff+1];
                char *rep    = strstr(cmd, old);
                *equals = '=';
                if(rep)
                {
                    strncpy(tmp, cmd, rep-cmd);
                    tmp[rep-cmd] = '\0';
                    strcat(tmp, new);
                    strcat(tmp, rep+oldlen);
                    __src.buffer   = tmp;
                }
                else
                {
                    __src.buffer   = cmd_history[first].cmd;
                }
                __src.bufsize  = strlen(__src.buffer)-1;
                __src.srctype  = SOURCE_FCCMD;
                __src.srcname  = NULL;
                __src.curpos   = -2;
                /* parse and execute */
                do_cmd();
                replace = 0;
            }
            else
            {
                /* execute the command without replacement */
                __src.buffer   = cmd_history[first].cmd;
                __src.bufsize  = strlen(cmd_history[first].cmd)-1;
                __src.srctype  = SOURCE_FCCMD;
                __src.srcname  = NULL;
                __src.curpos   = -2;
                do_cmd();
            }
        }
        /* restore the input source */
        __src = save_src;
        /* free the editor name */
        if(editor)
        {
            free_malloced_str(editor);
        }
        return exit_status;
    }
    
    /****************************/
    /* Option 3 - edit commands */
    /****************************/
    if(!editor)
    {
        /*
         * we don't have the editor name.. we will look for a valid editor name
         * in $FCEDIT, $HISTEDIT, and $EDITOR in turn.. if none is defined, we
         * will use the default editor /bin/ed.
         */
        char *fcedit = get_shell_varp("FCEDIT", NULL);
        if(!fcedit || !*fcedit)
        {
            /* $HISTEDIT is not a POSIX-defined var, but is widely
             * used by shells, so there is a chance it is set by the
             * user/shell instead of FCEDIT.
             */
            fcedit = get_shell_varp("HISTEDIT", NULL);
            /* bash checks $EDITOR if $FCEDIT is not set when in --posix mode */
            if(!fcedit || !*fcedit)
            {
                fcedit = get_shell_varp("EDITOR", NULL);
            }
        }
        if(!fcedit || !*fcedit)
        {
            editor = get_malloced_str("/bin/ed");
            edit_malloc = 1;
        }
        else
        {
            /* word-expand the editor name */
            editor = word_expand_to_str(fcedit);
            if(!editor)
            {
                editor = get_malloced_str("/bin/ed");
                edit_malloc = 1;
            }
        }
    }

    /*
     * in order to pass the commands to the editor, we will create a temporary
     * file in which we will write the history commands.. after the editor
     * finishes, we will read the temporary file to retrieve the edited commands,
     * which we will then execute.
     */
    /* get a temp filename */
    char *tmpname = get_tmp_filename();
    if(!tmpname)
    {
        fprintf(stderr, "%s: error creating temp file: %s\n", UTILITY, strerror(errno));
        if(editor)
        {
            /* free the editor name */
            if(edit_malloc)
            {
                free_malloced_str(editor);
            }
            else
            {
                free(editor);
            }
        }
        return 4;
    }
    /* create the temp file with the given name */
    int tmp = mkstemp(tmpname);
    if(tmp < 0)
    {
        fprintf(stderr, "%s: error creating temp file: %s\n", UTILITY, strerror(errno));
        free(tmpname);
        if(editor)
        {
            /* free the editor name */
            if(edit_malloc)
            {
                free_malloced_str(editor);
            }
            else
            {
                free(editor);
            }
        }
        return 4;
    }
    
    /* write the commands to the temp file */
    if(reverse)
    {
        for( ; last >= first; last--)
        {
            write(tmp, cmd_history[last ].cmd, strlen(cmd_history[last ].cmd));
        }
    }
    else
    {
        for( ; first <= last; first++)
        {
            write(tmp, cmd_history[first].cmd, strlen(cmd_history[first].cmd));
        }
    }

    /*
     * invoke the editor and check its exit status to determine if we should
     * execute the commands or not (the user might have cancelled her editing,
     * in which case we should not execute the commands).
     */
    char *editor_argv[3] = { editor, tmpname, NULL };
    int   status = fork_command(2, editor_argv, NULL, UTILITY, 0, 0);
    unlink(tmpname);
    if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        /* read the edited commands */
        lseek(tmp, 0, SEEK_SET);
        FILE *f = fdopen(tmp, "r");
        int  line_max = sysconf(_SC_LINE_MAX);
        if(line_max <= 0)
        {
            line_max = DEFAULT_LINE_MAX;
        }
        char cmd[line_max];
        struct source_s save_src = __src;
        while(fgets(cmd, line_max-1, f))
        {
            __src.buffer   = cmd;
            __src.bufsize  = strlen(cmd)-1;
            __src.srctype  = SOURCE_FCCMD;
            __src.srcname  = NULL;
            __src.curpos   = -2;
            do_cmd();
        }
        __src = save_src;
        fclose(f);
    }
    close(tmp);
    /* free the editor name */
    if(editor)
    {
        if(edit_malloc)
        {
            free_malloced_str(editor);
        }
        else
        {
            free(editor);
        }
    }
    free(tmpname);
    /* return the exit status of the last command executed */
    return exit_status;
}
