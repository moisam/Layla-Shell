/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "builtins.h"
#include "../cmd.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY             "fc"


/* struct to represent replacement strings of the format str1=str2 */
struct replace_str_s
{
    char *str;
    struct replace_str_s *next;
};


/*
 * Add the replacement string str, to the list replace_str.
 * The new string is added at the item pointed to by replace_after.
 */
void add_replacement_str(struct replace_str_s **replace_str,
                         struct replace_str_s **replace_after, char *str)
{
    struct replace_str_s *rep = malloc(sizeof(struct replace_str_s));
    if(!rep)
    {
        PRINT_ERROR("%s: insufficient memory\n", UTILITY);
        return;
    }
    
    rep->str = str;
    rep->next = NULL;
    
    /* list is empty, add to the beginning */
    if(*replace_str == NULL)
    {
        (*replace_str) = rep;
        (*replace_after) = rep;
    }
    else
    {
        (*replace_after)->next = rep;
        (*replace_after) = rep;
    }
}


/*
 * Free the replacement string list. This only frees the struct replace_str_s
 * structures, it doesn't free the memory used by the strings.
 */
void free_replacement_strs(struct replace_str_s *list)
{
    while(list)
    {
        struct replace_str_s *next = list->next;
        free(list);
        list = next;
    }
}


/*
 * Free the memory used to store the editor name.
 * If edit_malloc is non-zero, the editor name was malloc'd.
 * Otherwise, the editor name was alloc'd using the string buffer.
 */
void free_editor_name(int edit_malloc, char *editor)
{
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
}


/*
 * Execute history commands (with the -e option). The executed commands are
 * those with indices starting at first and ending at last, inclusive. The
 * replace_str argument contains the replacement strings, if any are given.
 */
void fc_exec_commands(size_t first, size_t last, struct replace_str_s *replace_str)
{
    struct source_s src;

    for( ; first <= last; first++)
    {
        char *hist_cmd = cmd_history[first].cmd;
        char *orig_cmd = hist_cmd;

        /*
         * find the first occurrence of replace and substitute it with
         * replace_str.
         */
        if(replace_str)
        {
            struct replace_str_s *rep = replace_str;
            
            while(rep)
            {
                char *cmd    = orig_cmd;
                char *equals = strchr(rep->str, '=');
                int   oldlen = equals - rep->str;
                char *new    = equals+1;
                char  old[oldlen+1];
                char *p, *p2;
                
                if(equals == rep->str)
                {
                    rep = rep->next;
                    continue;
                }
                
                if(!orig_cmd)
                {
                    break;
                }
                
                strncpy(old, rep->str, oldlen);
                old[oldlen] = '\0';
                
                while((p = strstr(cmd, old)))
                {
                    size_t start = p - orig_cmd;
                    size_t end = start + oldlen - 1;
                    
                    p2 = substitute_str(orig_cmd, new, start, end);
                    
                    if(orig_cmd != hist_cmd)
                    {
                        free(orig_cmd);
                        orig_cmd = NULL;
                    }
                    
                    if(!p2)
                    {
                        break;
                    }
                    
                    orig_cmd = p2;
                    cmd = orig_cmd + end + 1;
                }
                
                rep = rep->next;
            }

            src.buffer = orig_cmd;
        }
        else
        {
            /* execute the command without replacement */
            src.buffer = hist_cmd;
        }

        if(src.buffer)
        {
            src.bufsize  = strlen(src.buffer)-1;
            src.srctype  = SOURCE_FCCMD;
            src.srcname  = NULL;
            src.curpos   = INIT_SRC_POS;

            /* parse and execute */
            parse_and_execute(&src);
            
            /* free memory */
            if(src.buffer != hist_cmd)
            {
                free(src.buffer);
            }
        }
    }
}


/* internal procedure to output multiline commands */
void output_multi(char *cmd)
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


/* get an index out of a string */
void fc_get_index(char *str, int *index)
{
    int i, j;
    char *strend;
    
    /* check if the argument is numeric: -n, +n or n */
    if(*str == '-' || *str == '+' || isdigit(*str))
    {
        i = strtol(str , &strend, 10);
        if(*strend)
        {
            i = 0;
        }
    }
    else
    {
        /*
         * if non-numeric argument, use it as a string to search for in the
         * history list.
         */
        for(j = cmd_history_end-1; j >= 0; j--)
        {
            if(strstr(cmd_history[j].cmd, str) == cmd_history[j].cmd)
            {
                i = j;
                break;
            }
        }
    }
    
    /* first is -ve, effectively subtracting from the history list count */
    if(i < 0)
    {
        i += cmd_history_end;
    }
    
    (*index) = i;
}


/* print the history entry at the given index */
void print_history_entry(int index, int suppress_numbers /* , int i */)
{
    char *cmd = cmd_history[index].cmd;
    
    /* print the command number in the history list */
    if(!suppress_numbers)
    {
        printf("%d", index+1);
    }
    putc('\t', stdout);
    
    /* print the command */
    if(strstr(cmd, "\\\n"))
    {
        output_multi(cmd);
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


/*
 * The fc builtin utility (POSIX). Used to list, edit and rerun commands from
 * the history list.
 *
 * Returns the exit status of the last command executed.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help fc` or `fc -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int fc_builtin(int argc, char **argv)
{
    
    /*
     * The parser automatically adds new entries to the history list.
     * Remove the newest entry, which is the fc command that brought
     * us here.
     */
    char *h = get_last_cmd_history();
    if(h && strstr(h, "fc ") == h)
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
    int   prev_command     = cmd_history_end;
    struct replace_str_s *replace_str = NULL, *rep2 = NULL;   /* used with -s option */
    
    /****************************
     * process the options
     ****************************/
    int c /* , i */;
    while((c = parse_args(argc, argv, "hvelnrs", &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &FC_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* -e specifies the editor to use to edit commands */
            case 'e':
                if(!argv[v] || !argv[v][0])
                {
                    /*
                     * bash uses the default value of the expansion:
                     *     ${FCEDIT:-${EDITOR:-vi}
                     * 
                     * We use a similar expansion down in the file, which equates to:
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
        return 2;
    }

    /* get replacement strings (if -s option is used) */
    if(direct_exec)
    {
        /* the replace string takes the format str=replacement */
        while(argv[v] && strchr(argv[v], '='))
        {
            add_replacement_str(&replace_str, &rep2, argv[v]);
            v++;
        }
    }
    
    /* get the 'first' operand */
    if(v < argc)
    {
        fc_get_index(argv[v++], &first);
    }

    /* get the 'last' operand */
    if(v < argc)
    {
#if 0
        if(direct_exec)
        {
            PRINT_ERROR("%s: too many arguments\n", UTILITY);
            if(editor)
            {
                free_malloced_str(editor);
            }
            return 2;
        }
#endif
        
        fc_get_index(argv[v++], &last);
    }
    
//check_bounds:
    /*
     * Check that both the first and last command numbers are within the
     * valid range of the history list.
     *
     * If we are directly executing commands with the -s option, check if we
     * have the first command number. If we don't, use the last command in
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
         * We have the 'first' command number but not the 'last'. If we are
         * listing - not executing - commands, list all commands from 'first'
         * to the last command in the history list. If we are executing
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
         * We don't have the 'first' or 'last' command numbers. If we are
         * listing - not executing - commands, list the last 16 commands in
         * the history list. If we are executing commands, execute only the
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
    if(first < 1 || first > cmd_history_end)
    {
        //first = 1;
        PRINT_ERROR("%s: index out of range: %d\n", UTILITY, first);
        free_replacement_strs(replace_str);
        return 2;
    }
    
    if(last < 1 || last > cmd_history_end)
    {
        //last = 1;
        PRINT_ERROR("%s: index out of range: %d\n", UTILITY, last);
        free_replacement_strs(replace_str);
        return 2;
    }
    
#if 0
    if(first > cmd_history_end)
    {
        first = cmd_history_end;
    }
    
    if(last > cmd_history_end)
    {
        last = cmd_history_end;
    }
#endif
    
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
        /* print commands in reverse order */
        if(reverse)
        {
            //i = last-first+1;
            for( ; last >= first; last-- /* , i-- */)
            {
                print_history_entry(last, suppress_numbers /* , i */);
            }
        }
        /* print commands in normal order */
        else
        {
            //i = 1;
            for( ; first <= last; first++ /* , i++ */)
            {
                print_history_entry(first, suppress_numbers /* , i */);
            }
        }
        
        /* free the editor name */
        if(editor)
        {
            free_malloced_str(editor);
        }

        free_replacement_strs(replace_str);
        return 0;
    }

    /**************************************/
    /* Option 2 - execute without editing */
    /**************************************/
    if(direct_exec)
    {
        fc_exec_commands(first, last, replace_str);
        
        /* free the editor name */
        if(editor)
        {
            free_malloced_str(editor);
        }
        
        free_replacement_strs(replace_str);
        return exit_status;
    }
    
    /****************************/
    /* Option 3 - edit commands */
    /****************************/
    if(!editor)
    {
        /*
         * We don't have the editor name.. we will look for a valid editor name
         * in $FCEDIT, $HISTEDIT, and $EDITOR in turn. If none is defined, we
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
     * In order to pass the commands to the editor, we will create a temporary
     * file in which we will write the history commands. After the editor
     * finishes, we will read the temporary file to retrieve the edited commands,
     * which we will then execute.
     */
    /* get a temp filename */
    char *tmpname = get_tmp_filename();
    
    /* create the temp file with the given name */
    int tmp = tmpname ? mkstemp(tmpname) : -1;

    if(!tmpname || tmp < 0)
    {
        PRINT_ERROR("%s: error creating temp file: %s\n", UTILITY, strerror(errno));
        free_editor_name(edit_malloc, editor);
        free_replacement_strs(replace_str);
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
     * Invoke the editor and check its exit status to determine if we should
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
        struct source_s src;
        
        while(fgets(cmd, line_max-1, f))
        {
            src.buffer   = cmd;
            src.bufsize  = strlen(cmd)-1;
            src.srctype  = SOURCE_FCCMD;
            src.srcname  = NULL;
            src.curpos   = INIT_SRC_POS;
            parse_and_execute(&src);
        }
        
        fclose(f);
    }
    
    close(tmp);
    
    /* free temp memory */
    free_editor_name(edit_malloc, editor);
    free(tmpname);
    free_replacement_strs(replace_str);
    
    /* return the exit status of the last command executed */
    return exit_status;
}
