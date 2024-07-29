/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020, 2024 (c)
 * 
 *    file: history.c
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
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../backend/backend.h"
#include "builtins.h"
#include "setx.h"
#include "../include/debug.h"

#define UTILITY         "history"

#define stringify(s)    #s

/*************************************
 * The shell command history facility
 *************************************/

/* the command history list */
// struct histent_s cmd_history[MAX_CMD_HISTORY];
struct histent_s *cmd_history = NULL;
int    cmd_history_size = 0;

/* our current index in the list (where the next command is saved) */
int    cmd_history_index = 0;

/* the last command entered in the list (0 if the list is empty) */
int    cmd_history_end = 0;

/* number of history commands added in this session */
int    hist_cmds_this_session = 0;

// FILE   *hist_file           = NULL;

// char   *hist_filename       = NULL;

/*
 * number of command lines in the history file (helps when we want to
 * truncate the file to a certain size).
 */
int    hist_file_count = 0;

#if 0
/*
 * backup copy of the history filename and the $HISTFILE shell variable
 * (used when we temporarily switch the history filename in the history()
 * function below).
 */
char   *saved_hist_filename = NULL;
char   *saved_HISTFILE      = NULL;
#endif

/* default history filename */
char   default_hist_filename[]         = ".lsh_history";

/* the values we will give to the $HISTSIZE and $HISTCMD shell variables */
int    HISTSIZE            = 0;
int    HISTCMD             = 0;


/*
 * The history facility uses the $HISTCMD shell variable a lot. This variable
 * stores the index of the next history command entry. This function sets the
 * value of $HISTCMD to val.
 */
void set_HISTCMD(int val)
{
    HISTCMD = val;
    char buf[16];
    sprintf(buf, "%d", val);
    set_shell_varp("HISTCMD", buf);
}


char *get_history_filename(void)
{
    char *filename = get_shell_varp("HISTFILE", NULL);

    if(!filename || !*filename)
    {
        filename = "~/.history";
    }
    
    return word_expand_to_str(filename, FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES);
}


/*
 * Remove commands from the history list. The 'start' and 'end' parameters
 * give the zero-based index of the first and last command to remove, respectively.
 */
void clear_history(int start, int end)
{
    /* invalid indices */
    if(start < 0 || end > cmd_history_end)
    {
        return;
    }

    int i;
    /* clear the whole list */
    if(start == 0 && end == cmd_history_end)
    {
        for(i = 0; i < cmd_history_end; i++)
        {
            free(cmd_history[i].cmd);
            cmd_history[i].cmd = NULL;
        }

        cmd_history_end = 0;
        cmd_history_index = 0;
        hist_cmds_this_session = 0;
        return;
    }

    /* remove only the requested cmds */
    for(i = start; i < end; i++)
    {
        free(cmd_history[i].cmd);
        cmd_history[i].cmd = NULL;
    }

    /* shift the list by the number of the removed commands */
    int j = end-start;
    cmd_history_end -= j;
    for(i = start; i < cmd_history_end; i++)
    {
        cmd_history[i] = cmd_history[i+j];
        cmd_history[i+j].cmd = NULL;
    }

    /* make sure our current history index pointer doesn't point past the list end */
    if(cmd_history_index > cmd_history_end)
    {
        cmd_history_index = cmd_history_end;
    }
    
    if(start >= hist_file_count)
    {
        hist_cmds_this_session -= j;
    }
    else if(end >= hist_file_count)
    {
        hist_cmds_this_session -= (end - hist_file_count);
    }
}


/*
 * Read a line from the history file, alloc'ing memory if 'alloc_memory' is non-zero,
 * or discarding the line and returning NULL if 'alloc_memory' is zero. The 'is_timestamp'
 * field is set to 1 if the line is a command timestamp (that starts with # and a digit),
 * otherwise it is set to 0. This helps us differentiate command lines from the (optional)
 * timestamp lines that might precede them.
 */
char *read_line(FILE *file, int alloc_memory, int *is_timestamp)
{
    char buf[1024];
    char *ptr = NULL;
    char ptrlen = 0;
    (*is_timestamp) = 0;
    while(fgets(buf, 1024, file))
    {
        int buflen = strlen(buf);
        /*
         * If we need to allocate memory, start by alloc'ing a buffer that is
         * large enough to store the first line. We will extend the buffer as
         * required (if the command is a multiline command).
         */
        if(alloc_memory)
        {
            if(!ptrlen)
            {
                /* first line. alloc buffer */
                ptr = malloc(buflen+1);
            }
            else
            {
                /* subsequent lines. extend buffer */
                char *ptr2 = realloc(ptr, ptrlen+buflen+1);
                if(ptr2)
                {
                    ptr = ptr2;
                }
                else
                {
                    free(ptr);
                    ptr = NULL;
                }
            }
            /* insufficient memory */
            if(!ptr)
            {
                return NULL;
            }
            /* copy (or append) the line to the buffer */
            strcpy(ptr+ptrlen, buf);
        }
        /* return comment lines if they are timestamps */
        if(buf[0] == '#')
        {
            /* command timestamps start with a # and a digit */
            if(buf[1] != '\0' && isdigit(buf[1]))
            {
                (*is_timestamp) = 1;
                return ptr;
            }
            /* not a timestamp, skip it */
            continue;
        }
        /* skip empty lines */
        if(buf[0] == '\0')
        {
            continue;
        }
        /* check for a multiline command */
        if(buf[buflen-1] == '\n')
        {
            /* return the line if it has only '\n', or it ends in unquoted '\n' */
            if(buflen == 1 || buf[buflen-2] != '\\')
            {
                return ptr;
            }
            /* return the line if it doesn't end in two backslahes and '\n' */
            if(buflen > 2 && buf[buflen-3] != '\\')
            {
                return ptr;
            }
        }
        ptrlen += buflen;
    }
    return ptr;
}


int history_list_add(char *cmd, time_t time)
{
    /* init command history list */
    if(!cmd_history)
    {
        cmd_history = malloc(INIT_CMD_HISTORY_SIZE * sizeof(struct histent_s));

        if(!cmd_history)
        {
            INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "load history list");
            return 0;
        }

        cmd_history_size = INIT_CMD_HISTORY_SIZE;
        cmd_history_end = 0;
        cmd_history_index = 0;
    }
    else if(cmd_history_index == cmd_history_size)
    {
        size_t size = cmd_history_size + INIT_CMD_HISTORY_SIZE;
        struct histent_s *new_cmd_history = realloc(cmd_history, size * sizeof(struct histent_s));
        
        if(!new_cmd_history)
        {
            INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "load history list");
            return 0;
        }
        
        cmd_history_size = size;
    }
    
    cmd_history[cmd_history_index  ].time = time;
    cmd_history[cmd_history_index++].cmd  = cmd ;
    cmd_history_end++;
    
    return 1;
}


int read_history_file(char *filename)
{
    /* get the file path */
    char *path = filename ? filename : get_shell_varp("HISTFILE", NULL);
    
    if(!path)
    {
        PRINT_ERROR(SHELL_NAME, "can't read history: %s", 
                    "$HISTFILE is null or empty");
        return 0;
    }
    
    /* Open the history file */
    FILE *file = fopen(path, "r");

#if 0
    if(path != filename)
    {
        free(path);
    }
#endif
    
    /* failed to open the file */
    if(!file)
    {
        PRINT_ERROR(SHELL_NAME, "failed to read history file: %s", strerror(errno));
        return 0;
    }
    
    /*
     *  Get the total number of entries (not lines) in the history file.
     *  An entry might contain one or more lines, as in multiline commands
     *  that end with the '\\n' sequence. An entry might also contain a
     *  timestamp line, which starts with # an comes before the command line.
     *  The message is: we can't just count the number of lines in the history
     *  file and use that to represent the count of history entries we have.
     */
    char *line;
    int is_timestamp;
    time_t t = time(NULL);
    
    /* now read the entries in */
    while((line = read_line(file, 1, &is_timestamp)))
    {
        /* history entry timestamps start with a hash followed by a digit */
        if(is_timestamp)
        {
            char *strend;
            long t2 = strtol(line+1, &strend, 10);
            free(line);

            if(!*strend || isspace(*strend))
            {
                t = t2;
            }
            continue;
        }
        else
        {
            /* save the history command to our history list */
            history_list_add(line, t);
        }
    }

    fclose(file);

    cmd_history_end = cmd_history_index;
    hist_file_count = cmd_history_index;
    hist_cmds_this_session = 0;
    
    set_HISTCMD(cmd_history_end);
    return 1;
}


/*
 *  Truncate the history file if needed. To do this, we need to get the 
 *  total number of entries (NOT the number of lines) in the history file.
 *  An entry might contain one or more lines, as in multiline commands
 *  that end with the '\\n' sequence. An entry might also contain a
 *  timestamp line, which starts with # and comes before the command line.
 *  The message is: we can't just count the number of lines in the history
 *  file and use that to represent the count of history entries we have.
 *  We need to manually traverse the file, counting the number of lines,
 *  then stop when we reach the required number, and truncate the file there.
 */
void trunc_history_file(char *path)
{
    long target_count = get_shell_varl("HISTFILESIZE", -1);

    if(target_count >= 0)
    {
        /* Open the history file */
        FILE *file = fopen(path, "r+");
        int is_timestamp;
        off_t size = 0;
        
        /* failed to open the file */
        if(!file)
        {
#if 0
            PRINT_ERROR("%s: failed to read history file: %s\n", 
                        SOURCE_NAME, strerror(errno));
#endif
            return;
        }
        
        while(!feof(file))
        {
            read_line(file, 0, &is_timestamp);
            size = ftell(file);
            
            /* history entry timestamps which start with a hash followed by a digit */
            if(is_timestamp)
            {
                continue;
            }
            
            if(--target_count == 0)
            {
                break;
            }
        }
        
        fclose(file);
        
        /* if we reached our target count, truncate the file */
        if(target_count == 0)
        {
            truncate(path, size);
        }
    }
}


/*
 * Initialize the command line history facility.
 * Called upon interactive shell startup and when the -w option is set.
 */
void load_history_list(void)
{
    /* Set $HISTSIZE if not set */
    char *str = get_shell_varp("HISTSIZE", NULL);
    
    if(!str || !*str)
    {
        set_shell_varp("HISTSIZE", stringify(default_HISTSIZE));
    }

    /* Set $HISTFILESIZE if not set */
    str = get_shell_varp("HISTFILESIZE", NULL);
    
    if(!str || !*str)
    {
        set_shell_varp("HISTFILESIZE",
                       get_shell_varp("HISTSIZE", stringify(default_HISTSIZE)));
    }

    /* Get the history file name */
    str = get_shell_varp("HISTFILE", NULL);
    
    if(!str || !*str)
    {
        return;
    }
    
#if 0
    char *path = word_expand_to_str(str);
    
    if(!path)
    {
        return;
    }
    
    if(!*path)
    {
        free(path);
        return;
    }
    
    trunc_history_file(path);
    
    /* now read the history file */
    read_history_file(path);
    free(path);
#endif
    
    trunc_history_file(str);
    read_history_file(str);
}


void write_cmds_to_file(FILE *file, int start, int end)
{
    /*
     *  Check if we need to store the timestamp of each command. If the $HISTTIMEFORMAT
     *  variable is set, we store the timestamp information in a separate line that
     *  starts with #, followed by the time in seconds since the Unix epoch.
     */
    char *fmt = get_shell_varp("HISTTIMEFORMAT", NULL);

    for( ; start <= end; start++)
    {
        /* save the timestamp */
        if(fmt)
        {
            fprintf(file, "#%ld\n", cmd_history[start].time);
        }
        
        /* save the command */
        char *cmd = cmd_history[start].cmd;
        fprintf(file, "%s", cmd);
        
        /* add trailing newline if needed */
        if(cmd[strlen(cmd)-1] != '\n')
        {
            fprintf(file, "\n");
        }
    }
}


int write_history_to_file(char *filename, char *mode, int start, int end)
{
    /* get the file path */
    char *path = filename ? filename : get_shell_varp("HISTFILE", NULL);
    
    if(!path)
    {
        PRINT_ERROR(SHELL_NAME, "can't write history: %s", 
                    "$HISTFILE is null or empty");
        return 0;
    }
    
    /* open (or create) the file */
    FILE *file = fopen(path, mode);

#if 0
    if(path != filename)
    {
        free(path);
    }
#endif
    
    if(!file)
    {
        PRINT_ERROR(SHELL_NAME, "failed to open or create history file: %s", 
                    strerror(errno));
        return 0;
    }
    
    write_cmds_to_file(file, start, end);

    fclose(file);
    return 1;
}


/*
 * Save the history list entries to the history file.
 * Called on shell shut down.
 */
void flush_history(void)
{
    if(hist_cmds_this_session == 0)
    {
        return;
    }
    
    /* Get the history file name */
    char *path = get_shell_varp("HISTFILE", NULL);
    
    if(!path || !*path)
    {
        return;
    }
    
#if 0
    char *str = get_shell_varp("HISTFILE", NULL);
    
    if(!str || !*str)
    {
        return;
    }
    
    char *path = word_expand_to_str(str);
    
    if(!path)
    {
        return;
    }
    
    if(!*path)
    {
        free(path);
        return;
    }
#endif

    /* open the history file in append or write mode, as required */
    FILE *file;
    if(optionx_set(OPTION_HIST_APPEND))
    {
        file = fopen(path, "a");
    }
    else
    {
        file = fopen(path, "w");
    }
    
    
    if(!file)
    {
        return;
    }
    
    long start, end = cmd_history_end-1;
    
    if(optionx_set(OPTION_HIST_APPEND))
    {
        start = hist_file_count;
    }
    else
    {
        start = 0;
    }

    /* make sure we don't write more than $HISTSIZE entries */
    long histsize = get_shell_varl("HISTSIZE", 0);
    if(histsize > 0 && end-start > histsize)
    {
        start = end - histsize + 1;
    }
    
    write_cmds_to_file(file, start, end);
    
    hist_cmds_this_session = 0;
    fclose(file);
    trunc_history_file(path);
}


void remove_history_cmd(int index)
{
    /* list is already empty */
    if(cmd_history_end == 0)
    {
        return;
    }
    
    /* invalid index */
    if(index < 0 || index >= cmd_history_end)
    {
        return;
    }

    /* free the entry at index */
    char *cmd = cmd_history[index].cmd;
    if(cmd)
    {
        free(cmd);
    }
    cmd_history[index].cmd = NULL;

    /* shift the entries up */
    int i;
    for(i = index; i < cmd_history_end; i++)
    {
        cmd_history[i] = cmd_history[i+1];
    }

    /* adjust our indices */
    cmd_history_end--;
    cmd_history_index--;
    
    if(index >= hist_file_count)
    {
        hist_cmds_this_session--;
    }
}


/*
 * Remove the oldest entry in the history list to make room for a new entry
 * at the bottom of the list.
 */
void remove_oldest(void)
{
    remove_history_cmd(0);
    
    /* adjust $HISTCMD */
    set_HISTCMD(cmd_history_end);
}


/*
 * Remove the newest entry in the history list.
 * This function is only called by the fc builtin utility.
 */
void remove_newest(void)
{
    remove_history_cmd(cmd_history_end-1);
    
    /* adjust $HISTCMD */
    set_HISTCMD(cmd_history_end);
}


/*
 * Return the last entry in the history list.
 */
char *get_last_cmd_history(void)
{
    return cmd_history ? cmd_history[cmd_history_end-1].cmd : NULL;
}


/*
 * Compare the given history entries to see if they are the same.
 * Ignores any whitespace at the end of both entries.
 * 
 * Returns 1 if the entries are identical, 0 otherwise.
 */
int same_history_cmds(char *s1, char *s2)
{
    char *p1 = s1, *p2 = s2;
    char *pend1 = p1 + strlen(s1) - 1;
    char *pend2 = p2 + strlen(s2) - 1;
    
    /* skip any leading whitespace chars */
    while(*p1 && isspace(*p1))
    {
        p1++;
    }
    
    while(*p2 && isspace(*p2))
    {
        p2++;
    }
    
    if(!*p1 || !*p2)
    {
        return 0;
    }
    
    /* skip any trailing whitespace chars */
    while(isspace(*pend1))
    {
        pend1--;
    }
    
    while(isspace(*pend2))
    {
        pend2--;
    }

    /* make sure the strings are of the same length */
    if((pend1-p1) != (pend2-p2))
    {
        return 0;
    }
    
    /* compare the two strings */
    while(p1 < pend1)
    {
        if(*p1 != *p2)
        {
            return 0;
        }
        
        p1++;
        p2++;
    }
    
    return 1;
}


/*
 * Add a new command to the history list.
 *
 * Returns a pointer to the newly added entry, or NULL in case of error.
 */
char *save_to_history(char *cmd_buf)
{

#if 0
    /* first check we can save commands to the history list */
    int hsize = get_shell_vari("HISTSIZE", 0);
    if(!hsize)
    {
        return NULL;
    }
    else
    {
        HISTSIZE = hsize;
        if(HISTSIZE <= 0 || HISTSIZE > MAX_CMD_HISTORY)
        {
            HISTSIZE = default_HISTSIZE;
        }
    }
#endif

    /*
     * Parse the $HISTCONTROL variable, a colon-separated list which
     * can contain the values: ignorespace, ignoredups, ignoreboth and
     * erasedups. This variable is a non-POSIX bash extension.
     */
    int ign_sp = 0, ign_dup = 0, erase_dup = 0;
    char *e1 = get_shell_varp("HISTCONTROL", "");
    char *s;
    
    while((s = next_colon_entry(&e1)))
    {
        if(strcmp(s, "ignorespace") == 0)
        {
            ign_sp = 1;
        }
        else if(strcmp(s, "ignoredups") == 0)
        {
            ign_dup = 1;
        }
        else if(strcmp(s, "ignoreboth") == 0)
        {
            ign_sp = 1;
            ign_dup = 1;
        }
        else if(strcmp(s, "erasedups") == 0)
        {
            erase_dup = 1;
        }
        free(s);
    }

    /* don't save commands that start with a space char */
    if(ign_sp && isspace(*cmd_buf))
    {
        return NULL;
    }
    
    /* if there are entries in the history list, check for duplicates */
    if(cmd_history_end > 0)
    {
        /* remove duplicates */
        if(erase_dup)
        {
            int i = cmd_history_end-1;
            for( ; i >= 0; i--)
            {
                /* duplicate found */
                if(same_history_cmds(cmd_history[i].cmd, cmd_buf))
                {
                    remove_history_cmd(i);
                }
            }
        }
        /* ignore duplicates */
        else if(ign_dup)
        {
            /* don't repeat the last cmd saved */
            if(same_history_cmds(cmd_history[cmd_history_end-1].cmd, cmd_buf))
            {
                return cmd_history[cmd_history_end-1].cmd;
            }
        }
    }

    /*
     * Apply bash-like processing. the following algorithm is similar to what we 
     * do in match_ignore() in pattern.c we didn't use that function because we
     * need to process the special '&' and '\&' pattern to match the previous
     * history entry.
     */
    e1 = get_shell_varp("HISTIGNORE", "");
    while((s = next_colon_entry(&e1)))
    {
        if(strcmp(s, "&") == 0 || strcmp(s, "\\&") == 0)
        {
            /* don't repeat last cmd saved */
            if(same_history_cmds(cmd_history[cmd_history_end-1].cmd, cmd_buf))
            {
                free(s);
                return cmd_history[cmd_history_end-1].cmd;
            }
        }
        else if(match_filename(s, cmd_buf, 0, 0))
        {
            free(s);
            return NULL;
        }
        free(s);
    }
    
    /*
     * now parse and save the command.
     */

    size_t len = strlen(cmd_buf);

    /* add extra space for possible replacement of newline chars */
    char *p, *p2;
    char *pend = cmd_buf+len;
    for(p = cmd_buf; *p; p++)
    {
        /* add 1, because '\n' might be replaced by '; ' down there */
        if(*p == '\n')
        {
            len++;
        }
    }

#if 0
    /* add command to history list */
    if(cmd_history_end >= HISTSIZE /* MAX_CMD_HISTORY */)
    {
        remove_oldest();
    }
#endif
    
    /*
     * How we will save the command line in the history list depends on the 'cmdhist'
     * and 'cmdlit' extended options. For more information, see:
     *
     *     https://unix.stackexchange.com/questions/353386/when-is-a-multiline-history-entry-aka-lithist-in-bash-possible
     *
     * As well as the manpages of lsh and bash.
     * 
     * TODO: If option histlit is set, we must save the literal (unexpanded) history entry (as in tcsh).
     */
    time_t t = time(NULL);
    char *cmd = NULL;
    cmd_history_index = cmd_history_end;
    
    if(optionx_set(OPTION_CMD_HIST))
    {
        if(!(cmd = malloc(len+1)))
        {
            errno = ENOMEM;
            return "";
        }
        
        p2 = cmd_buf;
        p  = cmd;
        
        if(optionx_set(OPTION_LIT_HIST))
        {
            for( ; p2 < pend; p++, p2++)
            {
                *p = *p2;
            }
        }
        else
        {
            char *p3 = pend-1;
            for( ; p2 < pend; p++, p2++)
            {
                if(*p2 == '\n' && p2 < p3)
                {
                    *p++ = ';';
                    *p   = ' ';
                }
                else
                {
                    *p   = *p2;
                }
            }
        }
        
        if(p[-1] == '\n')
        {
            p--;
        }
        
        *p = '\0';
    }
    else
    {
        p2 = cmd_buf;
        p = p2;
        for( ; p2 < pend; p2++)
        {
            if(*p2 == '\n')
            {
                *p2 = '\0';
                save_to_history(p);
                p = p2+1;
            }
        }

        if(p < pend)
        {
            if(!(cmd = malloc(len+1)))
            {
                errno = ENOMEM;
                return "";
            }

            p2 = cmd_buf;
            p  = cmd;
            
            for( ; p2 < pend; p++, p2++)
            {
                *p = *p2;
            }

            if(p[-1] == '\n')
            {
                p--;
            }
            
            *p = '\0';
        }
    }

    if(cmd)
    {
        history_list_add(cmd, t);
        hist_cmds_this_session++;
    }
    set_HISTCMD(cmd_history_end);
    
    return cmd_history[cmd_history_end-1].cmd;
}


#if 0
/*
 * Temporarily switch the history file.
 * Called from history() when processing -a, -r, -n, -w options.
 */
void use_hist_file(char *path)
{
    if(!path || path == INVALID_OPTARG)
    {
        return;
    }
    saved_hist_filename = hist_filename;
    hist_filename = get_malloced_str(path);

    struct symtab_entry_s *entry = get_symtab_entry("HISTFILE");
    if(entry && entry->val)
    {
        if(saved_HISTFILE)
        {
            free_malloced_str(saved_HISTFILE);
        }
        saved_HISTFILE = get_malloced_str(entry->val);
        symtab_entry_setval(entry, path);
    }
}

/*
 * Restore the old history file.
 * Called from history() when processing -a, -r, -n, -w options.
 */
void restore_hist_file(void)
{
    if(saved_hist_filename)
    {
        if(hist_filename)
        {
            free_malloced_str(hist_filename);
        }
        hist_filename = saved_hist_filename;
    }

    struct symtab_entry_s *entry = get_symtab_entry("HISTFILE");
    if(entry)
    {
        symtab_entry_setval(entry, saved_HISTFILE);
    }
}
#endif


/*
 * Print the history entry represented by the string 'cmd'. If 'supp_nums' is zero,
 * the history entry index (as given in 'i') is printed before the entry. If 'fmt'
 * is not NULL, it represents the format string we will pass to strftime() in order
 * to print the timestamp info of the history entry. If 'fmt' is NULL, no timestamp
 * is printed. Lastly, the command line is printed.
 */
static inline void print_hist_entry(char *cmd, char *fmt, int i, int supp_nums)
{
    /* print the entry index */
    if(!supp_nums)
    {
        printf("%4d  ", i+1);
    }
    /* format string in not NULL */
    if(fmt)
    {
        /* print the timestamp and the command */
        struct tm *t = localtime(&cmd_history[i].time);
        char buf[32];
        if(strftime(buf, 32, fmt, t) > 0)
        {
            printf("%s %s", buf, cmd);
        }
        else
        {
            printf("%s", cmd);
        }
    }
    else
    {
        /* no timestamp. print only the command */
        printf("%s", cmd);
    }
    /* add newline char if command doesn't end in \n */
    if(cmd[strlen(cmd)-1] != '\n')
    {
        putchar('\n');
    }
}


int get_index(char *str, int *index, char **index_end)
{
    if(!*str)
    {
        return 0;
    }
    
    char *end;
    int i = strtol(str, &end, 10);
    
    /* offsets are given 1-based, but our indexing is 0-based */
    if(i > 0)
    {
        /* positive offset given */
        (*index) = i-1;
        (*index_end) = end;
        return 1;
    }
    else if(i < 0)
    {
        /* negative offset given */
        (*index) = i+cmd_history_end;
        (*index_end) = end;
        return 1;
    }
    else
    {
        /* 0 offset is invalid */
        return 0;
    }
}


/*
 * The history builtin utility (non-POSIX). Used to print, save and load the
 * history list.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help history` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int history_builtin(int argc, char **argv)
{
    /*
     * Get the value of the $HISTTIMEFORMAT shell variable. If not NULL, this
     * variable contains the format string we'll pass to strftime() when printing
     * command history entries.
     */
    char *fmt = get_shell_varp("HISTTIMEFORMAT", NULL);

    int start = 0, end = 0, supp_nums = 0, reverse = 0;
    int v = 1, c;
    int i;
    char *p, *pend;
    
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "a:cd:hn:r:p:s:vw:hRS:L:", &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            /*
             * in tcsh, -h suppresses output of entry numbers.
             */
            case 'h':
                supp_nums = 1;
                break;
                
            /*
             * in tcsh, -r reverses the listing order. we're using -r for reading the
             * history file (following bash), so we will use -R instead.
             */
            case 'R':
                reverse = 1;
                break;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* append the history list to the history file */
            case 'a':
                /* this option accepts an optional argument: history filename */
                if(hist_cmds_this_session)
                {
                    i = !write_history_to_file((internal_optarg == INVALID_OPTARG) ?
                                                NULL : internal_optarg,
                                                "a", hist_file_count, cmd_history_end-1);
                    hist_file_count = cmd_history_end;
                    hist_cmds_this_session = 0;
                }
                else
                {
                    i = 0;
                }
                
                /* bash returns after processing `history -a` */
                return i;
                
            /* clear the history list */
            case 'c':
                clear_history(0, cmd_history_end);
                break;
                
            /* delete some commands from the history list */
            case 'd':
                if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                {
                    goto missing_arg;
                }
                
                /* get the start offset */
                if(!get_index(internal_optarg, &start, &pend))
                {
                    goto invalid_off;
                }
                
                /*
                 * we can either have a start offset (positive or negative),
                 * without an end offset, or we can have both, separated by
                 * a hyphen '-'.
                 */
                if(*pend)
                {
                    if(*pend != '-')
                    {
                        goto invalid_off;
                    }

                    /* get the end offset */
                    if(!get_index(pend+1, &end, &pend))
                    {
                        goto invalid_off;
                    }
                }
                else
                {
                    end = start+1;
                }
                
                if(end < start)
                {
                    i = end;
                    end = start;
                    start = i;
                }

                clear_history(start, end);
                
                /* bash returns after processing `history -d` */
                return 0;
                
            case 'n':
                /*
                 * we should append new lines not read from the history file to the history list,
                 * not clear history and re-read the file as we do here.
                 * 
                 * TODO: fix this.
                 */
                
            /* tcsh uses -L instead of -r, which is used by bash */
            case 'L':
            case 'r':
                i = !read_history_file((internal_optarg == INVALID_OPTARG) ?
                                        NULL : internal_optarg);
                /* bash returns after processing `history -r` */
                return i;
                
            /* load history entry in the command buffer */
            case 'p':
                if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                {
                    goto missing_arg;
                }

                /*
                 * make sure the command buffer is initialized for the 
                 * non-interactive shell.
                 */
                init_cmdbuf();
                i = 0;

                char **p2 = &argv[v];
                for( ; *p2 && !i; p2++)
                {
                    p = *p2;
                    strcpy(cmdbuf, p);
                    cmdbuf_end = strlen(p);
                    
                    if((p = hist_expand(0, 0)) && p != INVALID_HIST_EXPAND)
                    {
                        printf("%s\n", p);
                        free_malloced_str(p);
                    }
                    else
                    {
                        PRINT_ERROR(UTILITY, "history exapnsion failed: %s", p);
                        i = 1;
                    }
                    
                    cmdbuf_end = 0;
                    cmdbuf_index = 0;
                    cmdbuf[0] = '\0';
                }
                
                /* bash returns after processing `history -s` */
                fflush(stdout);
                return i;
                
            /* save the history list to the given history file */
            case 's':
                if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                {
                    goto missing_arg;
                }
                
                p = internal_optarg;
                
                if(p && *p)
                {
                    save_to_history(p);
                }

                /* bash returns after processing `history -s` */
                return 0;
                
            /*
             * save the history list to the history file.
             * tcsh uses -S instead of -w, which is used by bash.
             */
            case 'S':
            case 'w':
                i = !write_history_to_file((internal_optarg == INVALID_OPTARG) ?
                                            NULL : internal_optarg,
                                           "w", 0, cmd_history_end-1);
                /* bash returns after processing `history -w` */
                return i;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* empty history list */
    if(cmd_history_end == 0)
    {
        return 0;
    }

    /* show [n] history entries */
    char *strend = NULL;
    start = argv[v] ? strtol(argv[v], &strend, 10) : 0;
    if((strend && *strend) || start < 0)
    {
        start = 0;
    }
    
    if(start)
    {
        start = cmd_history_end-start;
    }
    
    /* print the entries */
    if(reverse)
    {
        /* in reverse order */
        for(i = cmd_history_end-1; i >= start; i--)
        {
            print_hist_entry(cmd_history[i].cmd, fmt, i, supp_nums);
        }
    }
    else
    {
        /* in normal order */
        for(i = start; i < cmd_history_end; i++)
        {
            print_hist_entry(cmd_history[i].cmd, fmt, i, supp_nums);
        }
    }
    
    fflush(stdout);
    return 0;
    
invalid_off:
    PRINT_ERROR(UTILITY, "invalid offset passed to -d option: %s", internal_optarg);
    return 2;
    
missing_arg:
    OPTION_REQUIRES_ARG_ERROR(UTILITY, c);
    return 2;
}
