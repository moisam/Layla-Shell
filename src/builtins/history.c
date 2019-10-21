/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../backend/backend.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY         "history"

/*************************************
 * the shell command history facility
 *************************************/

/* the command history list */
struct histent_s cmd_history[MAX_CMD_HISTORY];

/* our current index in the list (where the next command is saved) */
int       cmd_history_index   = 0;

/* the last command entered in the list (0 if the list is empty) */
int       cmd_history_end     = 0;

/* pointer to the history file struct */
FILE     *__hist              = NULL;

/* the name of the history file */
char     *__hist_filename     = NULL;

/*
 * number of command lines in the history file (helps when we want to
 * truncate the file to a certain size).
 */
int       hist_file_count     = 0;

/*
 * backup copy of the history filename and the $HISTFILE shell variable
 * (used when we temporarily switch the history filename in the history()
 * function below).
 */
char     *saved_hist_filename = NULL;
char     *saved_HISTFILE      = NULL;

/* default history filename */
char      hist_file[]         = ".lsh_history";

/* the values we will give to the $HISTSIZE and $HISTCMD shell variables */
int       HISTSIZE            = 0;
int       HISTCMD             = 0;


/*
 * the history facility uses the $HISTCMD shell variable a lot.. this variable
 * stores the index of the next history command entry.. this function sets the
 * value of $HISTCMD to val.
 */
void set_HISTCMD(int val)
{
    HISTCMD = val;
    //struct symtab_entry_s *entry = add_to_symtab("HISTCMD");
    //if(!entry) return;
    char buf[16];
    sprintf(buf, "%d", val);
    //symtab_entry_setval(entry, buf);
    //entry->flags |= FLAG_EXPORT;
    set_shell_varp("HISTCMD", buf);
}


/*
 * remove commands from the history list.. the 'start' and 'end' parameters
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
}


/*
 * read a line from the history file, alloc'ing memory if 'alloc_memory' is non-zero,
 * or discarding the line and returning NULL if 'alloc_memory' is zero.. the 'is_timestamp'
 * field is set to 1 if the line is a command timestamp (that starts with # and a digit),
 * otherwise it is set to 0.. this helps us differentiate command lines from the (optional)
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
         * if we need to allocate memory, start by alloc'ing a buffer that is
         * large enough to store the first line.. we will extend the buffer as
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


/*
 * initialize the command line history facility.
 * called upon interactive shell startup.
 */
void init_history()
{
    /* get the maxium count of history entries we need to remember */
    struct symtab_entry_s *entry = get_symtab_entry("HISTSIZE");
    char   *hsize = entry ? entry->val : NULL;
    if(!hsize || !*hsize)
    {
        /* null entry. use our default value */
        HISTSIZE = default_HISTSIZE;
    }
    else
    {
        /* get the int value of the entry */
        HISTSIZE = atoi(hsize);
        if(HISTSIZE <= 0 || HISTSIZE > MAX_CMD_HISTORY)
        {
            /* invalid entry. use our default value */
            HISTSIZE = default_HISTSIZE;
        }
    }
    /* get the history file path */
    entry = get_symtab_entry("HISTFILE");
    char   *hist = entry ? entry->val : NULL;
    if(!hist || !*hist)
    {
        /* no history file. no commands to load */
        return;
    }
    else
    {
        /* open the history file */
        __hist_filename = get_malloced_str(hist);
        __hist = fopen(hist, "r");
    }
    /* failed to open the file */
    if(!__hist)
    {
        return;
    }
    /*
     *  get the total number of entries (not lines) in the history file..
     *  an entry might contain one or more lines, as in multiline commands
     *  that end with the '\\n' sequence.. an entry might also contain a
     *  timestamp line, which starts with # an comes before the command line..
     *  the message is: we can't just count the number of lines in the history
     *  file and use that to represent the count of history entries we have.
     */
    char *line;
    int count = 0;
    int is_timestamp;
    while(!feof(__hist))
    {
        read_line(__hist, 0, &is_timestamp);
        /* history entry timestamps which start with a hash followed by a digit */
        if(is_timestamp)
        {
            continue;
        }
        count++;
    }
    hist_file_count = count;
    /* history file is empty */
    if(!count)
    {
        fclose(__hist);
        return;
    }
    /* rewind the file to read the commands */
    rewind(__hist);
    /*
     * we want to get HISTSIZE entries from the file. if we have more, get only those.
     * otherwise, get 'em all.
     */
    while(count > HISTSIZE)
    {
        if(feof(__hist))
        {
            break;
        }
        read_line(__hist, 0, &is_timestamp);
        /* history entry timestamps which start with a hash followed by a digit */
        if(is_timestamp)
        {
            continue;
        }
        count--;
    }
    /* now read the entries in */
    while((line = read_line(__hist, 1, &is_timestamp)))
    {
        time_t t = time(NULL);
        /* history entry timestamps start with a hash followed by a digit */
        if(is_timestamp)
        {
            char *strend;
            long t2 = strtol(line+1, &strend, 10);
            if(strend != line+1)
            {
                t = t2;
            }
            free(line);
            continue;
        }
        /* save the history command to our history list */
        cmd_history[cmd_history_index  ].time = t   ;
        cmd_history[cmd_history_index++].cmd  = line;
    }
    fclose(__hist);
    cmd_history_end = cmd_history_index;
    set_HISTCMD(cmd_history_end);
    return;    
}


/*
 * save the history list entries to the history file.
 * called on shell shut down.
 */
void flush_history()
{
    /* empty list */
    if(cmd_history_end == 0)
    {
        return;
    }
    /* no valid history file */
    if(!__hist_filename)
    {
        return;
    }

    int i, j = 0;
    /*
     *  get the count of entries we ought to save.. we will truncate the history
     *  file to accommodate only that number of entries, discarding older entries
     *  if the file contains more than we need.
     */
    long hfsize = get_shell_varl("HISTFILESIZE", -1);

    if(hfsize < 0)          /* no history file truncation needed */
    {
        /* open the history file in append or write mode, as required */
        if(optionx_set(OPTION_HIST_APPEND))
        {
             __hist = fopen(__hist_filename, "a");
        }
        else
        {
            __hist = fopen(__hist_filename, "w");
        }
    }
    else if(hfsize == 0)    /* trunc history file to zero length */
    {
        __hist = fopen(__hist_filename, "w");
    }
    else                    /* trunc history file to some other size */
    {
        long total = cmd_history_end+hist_file_count;
        /*
         * the total number of entries (in the history file AND in our in-memory
         * history list) exceeds the maximum number we are allowed to save.
         */
        if(total > hfsize)
        {
            /*
             *  we need to read and discard the first lines of the file,
             * leaving only the desired number of entries, so that when
             * we write new entries, the total will equal hfsize.
             * j is the number of old entries we need to discard.
             */
            j = total-hfsize;
            if(hist_file_count <= j)
            {
                __hist = fopen(__hist_filename, "w");
                j -= hist_file_count;
            }
            else
            {
                __hist = fopen(__hist_filename, "r");
                int is_timestamp;
                /* skip the entries we want to discard */
                while(!feof(__hist))
                {
                    read_line(__hist, 0, &is_timestamp);
                    /* history entry timestamps which start with a hash followed by a digit */
                    if(is_timestamp)
                    {
                        continue;
                    }
                    j--;
                }
                /* temporarily save the rest of the file */
                long curpos = ftell(__hist);
                fseek(__hist, 0, SEEK_END);
                long size = ftell(__hist);
                fseek(__hist, curpos, SEEK_SET);
                long bytes = size-curpos;
                char *tmp = malloc(bytes);
                if(tmp)
                {
                    bytes = fread(tmp, 1, bytes, __hist);
                }
                fflush(__hist);
                fclose(__hist);
                /* trunc the file to zero and then write our entries */
                __hist = fopen(__hist_filename, "w");
                if(tmp)
                {
                    fwrite(tmp, 1, bytes, __hist);
                    free(tmp);
                }
            }
        }
        /*
         * the total number of entries in the file AND the list combined does not
         * exceed the maximum allowed.. we can go ahead and add our history list
         * entries to the file.
         */
        else
        {
            /* open the history file in append or write mode, as required */
            if(optionx_set(OPTION_HIST_APPEND))
            {
                 __hist = fopen(__hist_filename, "a");
            }
            else
            {
                __hist = fopen(__hist_filename, "w");
            }
        }
    }

    /* error opening the history file */
    if(!__hist)
    {
        return;
    }
    
    /*
     *  check if we need to store the timestamp of each command.. if the $HISTTIMEFORMAT
     *  variable is set, we store the timestamp information in a separate line that
     *  starts with #, followed by the time in seconds since the Unix epoch.
     */
    char *fmt = get_shell_varp("HISTTIMEFORMAT", NULL);
    for(i = j; i < cmd_history_end; i++)
    {
        /* save the timestamp */
        if(fmt)
        {
            fprintf(__hist, "#%ld\n", cmd_history[i].time);
        }
        /* save the command */
        char *cmd = cmd_history[i].cmd;
        fprintf(__hist, "%s", cmd);
        /* add trailing newline if needed */
        if(cmd[strlen(cmd)-1] != '\n')
        {
            fprintf(__hist, "\n");
        }
    }
    fclose(__hist);
    /*
     * free memory used by the command string. not really needed, as we are exiting,
     * but its good practice to balance your malloc/free call ratio. also helps
     * us when profiling our memory usage with valgrind.
     */
    for(i = 0; i < cmd_history_end; i++)
    {
        if(cmd_history[i].cmd)
        {
            free(cmd_history[i].cmd);
        }
    }
    if(__hist_filename)
    {
        free_malloced_str(__hist_filename);
    }
}


/*
 * remove the oldest entry in the history list to make room for a new entry
 * at the bottom of the list.
 */
void remove_oldest()
{
    int i;
    /* list is already empty */
    if(cmd_history_end == 0)
    {
        return;
    }
    /* free the 0th entry */
    free(cmd_history[0].cmd);
    /* shift the entries up */
    for(i = 0; i < cmd_history_end; i++)
    {
        cmd_history[i] = cmd_history[i+1];
    }
    /* adjust our indices */
    cmd_history_end--;
    cmd_history_index--;
    /* adjust $HISTCMD */
    set_HISTCMD(cmd_history_end);
}


/*
 * remove the newest entry in the history list.
 * this function is only called by the fc builtin utility.
 */
void remove_newest()
{
    /* list is already empty */
    if(cmd_history_end == 0)
    {
        return;
    }
    /* free the last entry */
    char *cmd = cmd_history[cmd_history_end-1].cmd;
    if(cmd)
    {
        free(cmd);
    }
    cmd_history[cmd_history_end-1].cmd = NULL;
    /* adjust our indices */
    cmd_history_end--;
    cmd_history_index--;
    /* adjust $HISTCMD */
    set_HISTCMD(cmd_history_end);
}


/*
 * return the last entry in the history list.
 */
char *get_last_cmd_history()
{
    return cmd_history[cmd_history_end-1].cmd;
}


/*
 * add a new command to the history list.
 *
 * returns a pointer to the newly added entry, or NULL in case of error.
 */
char *save_to_history(char *cmd_buf)
{

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
    
    /*
     * parse the $HISTCONTROL variable, a colon-separated list which
     * can contain the values: ignorespace, ignoredups, ignoreboth and
     * erasedups.. this variable is a non-POSIX bash extension.
     */
    int ign_sp = 0, ign_dup = 0, erase_dup = 0;
    char *e1 = get_shell_varp("HISTCONTROL", "");
    while(*e1)
    {
        char *e2 = e1+1;
        while(*e2 && *e2 != ':')
        {
            e2++;
        }
        char c = *e2;
        *e2 = '\0';
        if     (strcmp(e1, "ignorespace") == 0)
        {
            ign_sp    = 1;
        }
        else if(strcmp(e1, "ignoredups" ) == 0)
        {
            ign_dup   = 1;
        }
        else if(strcmp(e1, "ignoreboth" ) == 0)
        {
            ign_sp    = 1;
            ign_dup   = 1;
        }
        else if(strcmp(e1, "erasedups"  ) == 0)
        {
            erase_dup = 1;
        }
        *e2 = c;
        e1 = e2;
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
                if(strcmp(cmd_history[i].cmd, cmd_buf) == 0)
                {
                    /* remove entry */
                    free(cmd_history[i].cmd);
                    cmd_history[i].cmd = NULL;
                    /* shift entries up */
                    int j = i;
                    for( ; j < cmd_history_end; j++)
                    {
                        cmd_history[j] = cmd_history[j+1];
                    }
                    cmd_history_end--;
                    cmd_history_index--;
                }
            }
        }
        /* ignore duplicates */
        else if(ign_dup)
        {
            /* don't repeat the last cmd saved */
            if(strcmp(cmd_history[cmd_history_end-1].cmd, cmd_buf) == 0)
            {
                return cmd_history[cmd_history_end-1].cmd;
            }
        }
    }

    /*
     * apply bash-like processing. the following algorithm is similar to what we 
     * do in match_ignore() in pattern.c we didn't use that function because we
     * need to process the special '&' and '\&' pattern to match the previous
     * history entry.
     */
    e1 = get_shell_varp("HISTIGNORE", "");
    while(*e1)
    {
        char *e2 = e1+1;
        while(*e2 && *e2 != ':')
        {
            e2++;
        }
        char c = *e2;
        *e2 = '\0';
        if(strcmp(e1, "&") == 0 || strcmp(e1, "\\&") == 0)
        {
            /* don't repeat last cmd saved */
            if(strcmp(cmd_history[cmd_history_end-1].cmd, cmd_buf) == 0)
            {
                *e2 = c;
                return cmd_history[cmd_history_end-1].cmd;
            }
        }
        else if(match_filename(e1, cmd_buf, 0, 0))
        {
            *e2 = c;
            return NULL;
        }
        *e2 = c;
        e1 = e2;
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

    /* add command to history list */
    if(cmd_history_end >= HISTSIZE /* MAX_CMD_HISTORY */)
    {
        remove_oldest();
    }
    
    /*
     * how we will save the command line in the history list depends on the 'cmdhist'
     * and 'cmdlit' extended options.. for more information, see:
     *
     *     https://unix.stackexchange.com/questions/353386/when-is-a-multiline-history-entry-aka-lithist-in-bash-possible
     *
     * as well as the manpages of lsh and bash.
     * 
     * TODO: if option histlit is set, we must save the literal (unexpanded) history entry (as in tcsh).
     */
    time_t t = time(NULL);
    cmd_history[cmd_history_end].time = t;
    
    if(optionx_set(OPTION_CMD_HIST))
    {
        if(!(cmd_history[cmd_history_end].cmd = malloc(len+1)))
        {
            errno = ENOMEM;
            return "";
        }
        
        p2 = cmd_buf;
        p  = cmd_history[cmd_history_end].cmd;
        
        if(optionx_set(OPTION_LIT_HIST))
        {
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
                    *p  = *p2;
                }
            }
            if(p[-1] == '\n')
            {
                p--;
            }
            *p = '\0';
        }
        cmd_history_end++;
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
            if(!(cmd_history[cmd_history_end].cmd = malloc(len+1)))
            {
                errno = ENOMEM;
                return "";
            }
            p2 = cmd_buf;
            p  = cmd_history[cmd_history_end].cmd;
            for( ; p2 < pend; p++, p2++)
            {
                *p = *p2;
            }
            if(p[-1] == '\n')
            {
                p--;
            }
            *p = '\0';
            cmd_history_end++;
        }
    }
    cmd_history_index = cmd_history_end;
    set_HISTCMD(cmd_history_end);
    return cmd_history[cmd_history_end-1].cmd;
}


/*
 * temporarily switch the history file.
 * called from history() when processing -a, -r, -n, -w options.
 */
void use_hist_file(char *path)
{
    if(!path || path == INVALID_OPTARG)
    {
        return;
    }
    saved_hist_filename = __hist_filename;
    __hist_filename = get_malloced_str(path);

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
 * restore the old history file.
 * called from history() when processing -a, -r, -n, -w options.
 */
void restore_hist_file()
{
    if(saved_hist_filename)
    {
        if(__hist_filename)
        {
            free_malloced_str(__hist_filename);
        }
        __hist_filename = saved_hist_filename;
    }

    struct symtab_entry_s *entry = get_symtab_entry("HISTFILE");
    if(entry)
    {
        symtab_entry_setval(entry, saved_HISTFILE);
    }
}


/*
 * print the history entry represented by the string 'cmd'.. if 'supp_nums' is zero,
 * the history entry index (as given in 'i') is printed before the entry.. if 'fmt'
 * is not NULL, it represents the format string we will pass to strftime() in order
 * to print the timestamp info of the history entry.. if 'fmt' is NULL, no timestamp
 * is printed.. lastly, the command line is printed.
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


/*
 * the history builtin utility (non-POSIX).. used to print, save and load the
 * history list.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help history` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int history(int argc, char **argv)
{
    /*
     * get the value of the $HISTTIMEFORMAT shell variable.. if not NULL, this
     * variable contains the format string we'll pass to strftime() when printing
     * command history entries.
     */
    char *fmt = get_shell_varp("HISTTIMEFORMAT", NULL);

    int start = 0, end = 0, supp_nums = 0, reverse = 0;
    int v = 1, c;
    int i;
    char *p, *pend;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "a:cd:hn:r:p:s:vw:hRS:L:", &v, 0)) > 0)
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
                i = optionx_set(OPTION_HIST_APPEND);
                set_optionx(OPTION_HIST_APPEND, 1);
                use_hist_file(__optarg);
                flush_history();
                restore_hist_file();
                set_optionx(OPTION_HIST_APPEND, i);
                break;
                
            /* clear the history list */
            case 'c':
                clear_history(0, cmd_history_end);
                break;
                
            /* delete some commands from the history list */
            case 'd':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: -d option is missing argument: offset\n", UTILITY);
                    return 2;
                }
                p  = strchr(__optarg, '-');
                pend = NULL;
                if(p)
                {
                    /* '-' is the first char in arg */
                    if(p == __optarg)
                    {
                        char *p2 = strchr(p+1, '-');
                        if(p2)
                        {
                            /* start-end range given, start is negative */
                            i = p2-__optarg;
                            char buf[i+1];
                            strncpy(buf, __optarg, i);
                            buf[i] = '\0';
                            start = strtol(buf, &pend, 10);
                            if(pend == buf || start < -cmd_history_end)
                            {
                                goto invalid_off;
                            }
                            /* offset -1 points to the last command added to the history list */
                            if(start == 0)
                            {
                                goto invalid_off;
                            }
                            start += cmd_history_end;
                            pend = NULL;
                            end = strtol(p2+1, &pend, 10);
                            if(pend == p2+1)
                            {
                                goto invalid_off;
                            }
                            if(end < 0)
                            {
                                end += cmd_history_end;
                            }
                            if(end >= cmd_history_end)
                            {
                                goto invalid_off;
                            }
                            end++;
                        }
                        else
                        {
                            /* negative offset given */
                            start = strtol(__optarg, &pend, 10);
                            if(pend == __optarg || start < -cmd_history_end)
                            {
                                goto invalid_off;
                            }
                            /* offset -1 points to the last command added to the history list */
                            if(start == 0)
                            {
                                goto invalid_off;
                            }
                            start += cmd_history_end;
                            end = start+1;
                        }
                    }
                    else
                    {
                        /* start-end range given, start is positive */
                        i = p-__optarg;
                        char buf[i+1];
                        strncpy(buf, __optarg, i);
                        buf[i] = '\0';
                        start = strtol(buf, &pend, 10);
                        if(pend == buf || start > cmd_history_end)
                        {
                            goto invalid_off;
                        }
                        /* offsets are given 1-based, but our indexing is 0-based */
                        if(start == 0)
                        {
                            goto invalid_off;
                        }
                        start--;
                        pend = NULL;
                        end = strtol(p+1, &pend, 10);
                        if(pend == p+1)
                        {
                            goto invalid_off;
                        }
                        if(end < 0)
                        {
                            end += cmd_history_end;
                        }
                        if(end >= cmd_history_end)
                        {
                            goto invalid_off;
                        }
                        end++;
                    }
                }
                else
                {
                    /* positive offet given */
                    start = strtol(__optarg, &pend, 10);
                    if(pend == __optarg || start > cmd_history_end)
                    {
                        goto invalid_off;
                    }
                    /* offsets are given 1-based, but our indexing is 0-based */
                    end = start;
                    start--;
                }
                
                if(end < start)
                {
                    i = end;
                    end = start;
                    start = i;
                }
                clear_history(start, end);
                break;
                
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
                use_hist_file(__optarg);
                clear_history(0, cmd_history_end);
                init_history();
                restore_hist_file();
                break;
                
            /* load history entry in the command buffer */
            case 'p':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: -%c option is missing arguments\n", UTILITY, 'p');
                    return 2;
                }
                p = list_to_str(&argv[v], 0);
                if(p)
                {
                    strcpy(cmdbuf, p);
                    cmdbuf_end = strlen(p);
                    free(p);
                    if((p = hist_expand(0)) && p != INVALID_HIST_EXPAND)
                    {
                        printf("%s\n", p);
                        free_malloced_str(p);
                    }
                    cmdbuf_end = 0;
                    cmdbuf_index = 0;
                    cmdbuf[0] = '\0';
                    return 0;
                }
                else
                {
                    return 1;
                }
                
            /* save the history list to the given history file */
            case 's':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: -%c option is missing arguments\n", UTILITY, 's');
                    return 2;
                }
                p = list_to_str(&argv[v], 0);
                if(p)
                {
                    save_to_history(p);
                    free(p);
                }
                return 0;
                
            /*
             * save the history list to the history file.
             * tcsh uses -S instead of -w, which is used by bash.
             */
            case 'S':
            case 'w':
                i = optionx_set(OPTION_HIST_APPEND);
                set_optionx(OPTION_HIST_APPEND, 0);
                use_hist_file(__optarg);
                flush_history();
                restore_hist_file();
                set_optionx(OPTION_HIST_APPEND, i);
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }

    /* show [n] history entries */
    if(v < argc)
    {
        char *strend;
        start = strtol(argv[v], &strend, 10);
        if(strend == argv[v] || start < 0)
        {
            start = 0;
        }
        start = cmd_history_end-start;
    }
    
    /* history list is empty */
    if(cmd_history_end == 0)
    {
        return 0;
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
    return 0;
    
invalid_off:
    fprintf(stderr, "%s: invalid offset passed to -d option: %s\n", UTILITY, __optarg);
    return 2;
}
