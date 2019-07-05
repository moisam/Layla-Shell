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
 * Shell command history.
 *************************************/

struct histent_s cmd_history[MAX_CMD_HISTORY];
int       cmd_history_index   = 0;
int       cmd_history_end     = 0;
FILE     *__hist              = NULL;
char     *__hist_filename     = NULL;
int       hist_file_count     = 0;
char     *saved_hist_filename = NULL;
char     *saved_HISTFILE      = NULL;
// int       hist_new_entries_at = 0;
// #ifdef __detour_kernel
//   char         hist_file[]   = ".sh_history";
// #else
char      hist_file[]         = ".lsh_history";
// #endif
char      DEVNULL[]           = "/dev/null";
int       line_max;
/* the values we will give to some shell variables */
int       HISTSIZE            = 0;
int       HISTCMD             = 0;


void set_HISTCMD(int val)
{
    HISTCMD = val;
    struct symtab_entry_s *entry = add_to_symtab("HISTCMD");
    if(!entry) return;
    char buf[16];
    sprintf(buf, "%d", val);
    symtab_entry_setval(entry, buf);
    entry->flags |= FLAG_EXPORT;
}


void clear_history(int start, int end)
{
    if(start < 0 || end > cmd_history_end) return;

    int i;
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
    /* delete the request cmds */
    for(i = start; i < end; i++)
    {
        free(cmd_history[i].cmd);
        cmd_history[i].cmd = NULL;
    }
    /* shift the list */
    int j = end-start;
    cmd_history_end -= j;
    for(i = start; i < cmd_history_end; i++)
    {
        cmd_history[i] = cmd_history[i+j];
        cmd_history[i+j].cmd = NULL;
    }
    if(cmd_history_index > cmd_history_end) cmd_history_index = cmd_history_end;
}


void init_history()
{
    line_max = get_linemax();
    char __line[line_max];
    struct symtab_entry_s *entry = get_symtab_entry("HISTSIZE");
    char   *hsize = entry ? entry->val : NULL;
    if(!hsize || !*hsize) HISTSIZE = default_HISTSIZE;
    else
    {
        HISTSIZE = atoi(hsize);
        if(HISTSIZE <= 0 || HISTSIZE > MAX_CMD_HISTORY)
            HISTSIZE = default_HISTSIZE;
    }
    entry = get_symtab_entry("HISTFILE");
    char   *hist = entry ? entry->val : NULL;
    if(!hist || !*hist) return;
    else
    {
        __hist_filename = get_malloced_str(hist);
        __hist = fopen(hist, "r");
    }
    if(!__hist) return;
    
    char *line;
    int count = 0;
    while((line = fgets(__line, line_max, __hist)))
    {
        /* history entry timestamps which start with a hash followed by a digit */
        if(__line[0] == '#' && isdigit(__line[1]))
            line = fgets(__line, line_max, __hist);
        if(!line) break;
        /* don't re-count multiline commands */
        size_t len = strlen(line);
        if((len >= 2) && (line[len-1] == '\n') && (line[len-2] == '\\'))
            continue;
        count++;
    }
    hist_file_count = count;
    rewind(__hist);
    /*
     * we want to get HISTSIZE entries from the file. if we have more, get only those.
     * otherwise, get 'em all.
     */
    while(count > HISTSIZE)
    {
        line = fgets(__line, line_max, __hist);
        /* history entry timestamps which start with a hash followed by a digit */
        if(__line[0] == '#' && isdigit(__line[1]))
            line = fgets(__line, line_max, __hist);
        if(!line) break;
        /* read all lines of a multiline command */
        size_t len = strlen(__line);
        while((len >= 2) && (__line[len-1] == '\n') && (__line[len-2] == '\\'))
        {
            fgets(__line, line_max, __hist);
            len = strlen(__line);
        }
        count--;
    }
    /* now read the entries in */
    while((line = fgets(__line, line_max, __hist)))
    {
        time_t t = time(NULL);
        line = fgets(__line, line_max, __hist);
        /* history entry timestamps start with a hash followed by a digit */
        if(__line[0] == '#' && isdigit(__line[1]))
        {
            char *strend;
            long t2 = strtol(line+1, &strend, 10);
            if(strend != line+1) t = t2;
            line = fgets(__line, line_max, __hist);
        }
        if(!line) break;
        /* read all lines of a multiline command */
        size_t len = strlen(line);
        while((len >= 2) && (__line[len-1] == '\n') && (__line[len-2] == '\\'))
        {
            line = fgets(__line+len, line_max, __hist);
            len += strlen(line);
            /* TODO: we should do better than just bail out in case of overflow */
            if(len >= line_max) break;
        }
        line = malloc(len+1);
        if(!line) continue;
        strcpy(line, __line);
        //line = get_malloced_str(__line);
        cmd_history[cmd_history_index  ].time = t   ;
        cmd_history[cmd_history_index++].cmd  = line;
    }
    fclose(__hist);
    cmd_history_end = cmd_history_index;
    set_HISTCMD(cmd_history_end);
    return;    
}


/* called on shut down */
void flush_history()
{
    if(cmd_history_end == 0) return;
    if(!__hist_filename) return;

    int i, j = 0;
    long hfsize = get_shell_varl("HISTFILESIZE", -1);

    if(hfsize < 0)          /* no truncation */
    {
        if(optionx_set(OPTION_HIST_APPEND))
             __hist = fopen(__hist_filename, "a");
        else __hist = fopen(__hist_filename, "w");
    }
    else if(hfsize == 0)    /* trunc to zero */
    {
        __hist = fopen(__hist_filename, "w");
    }
    else                    /* trunc to other size */
    {
        long total = cmd_history_end+hist_file_count;
        if(total > hfsize)
        {
            /* we need to read and discard the first lines of the file,
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
                char __line[line_max];
                char *line;
                /* skip the entries we want to discard */
                while((line = fgets(__line, line_max, __hist)))
                {
                    /* history entry timestamps which start with a hash followed by a digit */
                    if(__line[0] == '#' && isdigit(__line[1]))
                        line = fgets(__line, line_max, __hist);
                    if(!line) break;
                    /* don't re-count multiline commands */
                    size_t len = strlen(line);
                    if((len >= 2) && (line[len-1] == '\n') && (line[len-2] == '\\'))
                        continue;
                    if(--j == 0) break;
                }
                /* temporarily save the rest of the file */
                long curpos = ftell(__hist);
                fseek(__hist, 0, SEEK_END);
                long size = ftell(__hist);
                fseek(__hist, curpos, SEEK_SET);
                long bytes = size-curpos;
                char *tmp = malloc(bytes);
                if(tmp) bytes = fread(tmp, 1, bytes, __hist);
                fflush(__hist);
                fclose(__hist);
                /* trunc to zero and then write our entries */
                __hist = fopen(__hist_filename, "w");
                if(tmp)
                {
                    fwrite(tmp, 1, bytes, __hist);
                    free(tmp);
                }
            }
        }
        else
        {
            __hist = fopen(__hist_filename, "w");
        }
    }

    if(!__hist) return;
    
    char *fmt = get_shell_varp("HISTTIMEFORMAT", NULL);
    for(i = j; i < cmd_history_end; i++)
    {
        if(fmt) fprintf(__hist, "#%ld\n", cmd_history[i].time);
        char *cmd = cmd_history[i].cmd;
        fprintf(__hist, "%s", cmd);
        if(cmd[strlen(cmd)-1] != '\n') fprintf(__hist, "\n");
    }
    fclose(__hist);
    /*
     * free memory used by the command string. not really needed, as we are exiting,
     * but its good practice to balance your malloc/free call ratio. also helps
     * us when profiling our memory usage with valgrind.
     */
    for(i = 0; i < cmd_history_end; i++)
    {
        if(cmd_history[i].cmd) free(cmd_history[i].cmd);
    }
    if(__hist_filename && __hist_filename != DEVNULL) free_malloced_str(__hist_filename);
}


void remove_oldest()
{
    int i;
    if(cmd_history_end == 0) return;
    free(cmd_history[0].cmd);
// remove:
    for(i = 0; i < cmd_history_end; i++)
        cmd_history[i] = cmd_history[i+1];
    cmd_history_end--;
    cmd_history_index--;
    set_HISTCMD(cmd_history_end);
}


void remove_newest()
{
    if(cmd_history_end == 0) return;
    char *cmd = cmd_history[cmd_history_end-1].cmd;
    if(cmd) free(cmd);
    cmd_history[cmd_history_end-1].cmd = NULL;
    cmd_history_end--;
    cmd_history_index--;
    set_HISTCMD(cmd_history_end);
}


char *get_last_cmd_history()
{
    return cmd_history[cmd_history_end-1].cmd;
}


char *save_to_history(char *cmd_buf)
{

    /* first check we can save commands to the history list */
    int hsize = get_shell_vari("HISTSIZE", 0);
    if(!hsize) return NULL;
    else
    {
        HISTSIZE = hsize;
        if(HISTSIZE <= 0 || HISTSIZE > MAX_CMD_HISTORY) HISTSIZE = default_HISTSIZE;
    }
    
    /*
     * parse the $HISTCONTROL variable, a colon-separated list which
     * can contain the values: ignorespace, ignoredups, ignoreboth and
     * erasedups. this is a bash extension.
     */
    int ign_sp = 0, ign_dup = 0, erase_dup = 0;
    char *e1 = get_shell_varp("HISTCONTROL", "");
    while(*e1)
    {
        char *e2 = e1+1;
        while(*e2 && *e2 != ':') e2++;
        char c = *e2;
        *e2 = '\0';
        if     (strcmp(e1, "ignorespace") == 0) ign_sp    = 1;
        else if(strcmp(e1, "ignoredups" ) == 0) ign_dup   = 1;
        else if(strcmp(e1, "ignoreboth" ) == 0) { ign_sp  = 1; ign_dup = 1; }
        else if(strcmp(e1, "erasedups"  ) == 0) erase_dup = 1;
        *e2 = c;
        e1 = e2;
    }

    /* don't save commands that start with a space char */
    if(ign_sp && isspace(*cmd_buf)) return NULL;
    
    if(cmd_history_end > 0)
    {
        /* remove duplicates */
        if(erase_dup)
        {
            int i = cmd_history_end-1;
            for( ; i >= 0; i--)
            {
                if(strcmp(cmd_history[i].cmd, cmd_buf) == 0)
                {
                    //remove_newest();
                    free(cmd_history[i].cmd);
                    cmd_history[i].cmd = NULL;
                    int j = i;
                    for( ; j < cmd_history_end; j++)
                        cmd_history[j] = cmd_history[j+1];
                    cmd_history_end--;
                    cmd_history_index--;
                }
            }
        }
        else if(ign_dup)
        {
            /* don't repeat last cmd saved */
            if(strcmp(cmd_history[cmd_history_end-1].cmd, cmd_buf) == 0)
                return cmd_history[cmd_history_end-1].cmd;
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
            while(*e2 && *e2 != ':') e2++;
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
        if(*p == '\n') len++;
    }

    /* add command to history list */
    if(cmd_history_end >= HISTSIZE /* MAX_CMD_HISTORY */)
    {
        remove_oldest();
    }
    
    /*
     * for more information, see:
     * https://unix.stackexchange.com/questions/353386/when-is-a-multiline-history-entry-aka-lithist-in-bash-possible
     * 
     * TODO: if option histlit is set, we must save the literal (unexpanded) history entry (as in tcsh).
     */
    time_t t = time(NULL);
    cmd_history[cmd_history_end].time = t;
    
    if(optionx_set(OPTION_CMD_HIST))
    {
        if(!(cmd_history[cmd_history_end].cmd = (char *)malloc(len+1)))
        {
            errno = ENOMEM;
            return "";
        }
        
        p2 = cmd_buf;
        p  = cmd_history[cmd_history_end].cmd;
        
        if(optionx_set(OPTION_LIT_HIST))
        {
            for( ; p2 < pend; p++, p2++) *p = *p2;
            if(p[-1] == '\n') p--;
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
                else *p  = *p2;
            }
            if(p[-1] == '\n') p--;
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
            if(!(cmd_history[cmd_history_end].cmd = (char *)malloc(len+1)))
            {
                errno = ENOMEM;
                return "";
            }
            p2 = cmd_buf;
            p  = cmd_history[cmd_history_end].cmd;
            for( ; p2 < pend; p++, p2++) *p = *p2;
            if(p[-1] == '\n') p--;
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
    if(!path || path == INVALID_OPTARG) return;
    saved_hist_filename = __hist_filename;
    __hist_filename = get_malloced_str(path);
    struct symtab_entry_s *entry = get_symtab_entry("HISTFILE");
    if(entry && entry->val)
    {
        if(saved_HISTFILE) free_malloced_str(saved_HISTFILE);
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
        if(__hist_filename) free_malloced_str(__hist_filename);
        __hist_filename = saved_hist_filename;
    }
    struct symtab_entry_s *entry = get_symtab_entry("HISTFILE");
    if(entry) symtab_entry_setval(entry, saved_HISTFILE);
}


static inline void print_hist_entry(char *cmd, char *fmt, int i, int supp_nums)
{
    if(!supp_nums) printf("%4d  ", i+1);
    if(fmt)
    {
        struct tm *t = localtime(&cmd_history[i].time);
        char buf[32];
        if(strftime(buf, 32, fmt, t) > 0) printf("%s %s", buf, cmd);
        else printf("%s", cmd);
    }
    else printf("%s", cmd);
    if(cmd[strlen(cmd)-1] != '\n') putchar('\n');
}


/*
 * the history builtin utility.
 */

int history(int argc, char **argv)
{
    char *fmt = get_shell_varp("HISTTIMEFORMAT", NULL);

    /****************************
     * process the arguments
     ****************************/
    int start = 0, end = 0, supp_nums = 0, reverse = 0;
    int v = 1, c;
    int i;
    char *p, *pend;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
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
                
            case 'a':
                i = optionx_set(OPTION_HIST_APPEND);
                set_optionx(OPTION_HIST_APPEND, 1);
                use_hist_file(__optarg);
                flush_history();
                restore_hist_file();
                set_optionx(OPTION_HIST_APPEND, i);
                break;
                
            case 'c':
                clear_history(0, cmd_history_end);
                break;
                
            case 'd':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: -d option is missing argument: offset\r\n", UTILITY);
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
                            if(pend == buf || start < -cmd_history_end) goto invalid_off;
                            /* offset -1 points to the last command added to the history list */
                            if(start == 0) goto invalid_off;
                            start += cmd_history_end;
                            pend = NULL;
                            end = strtol(p2+1, &pend, 10);
                            if(pend == p2+1) goto invalid_off;
                            if(end < 0) end += cmd_history_end;
                            if(end >= cmd_history_end) goto invalid_off;
                            end++;
                        }
                        else
                        {
                            /* negative offset given */
                            start = strtol(__optarg, &pend, 10);
                            if(pend == __optarg || start < -cmd_history_end) goto invalid_off;
                            /* offset -1 points to the last command added to the history list */
                            if(start == 0) goto invalid_off;
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
                        if(pend == buf || start > cmd_history_end) goto invalid_off;
                        /* offsets are given 1-based, but our indexing is 0-based */
                        if(start == 0) goto invalid_off;
                        start--;
                        pend = NULL;
                        end = strtol(p+1, &pend, 10);
                        if(pend == p+1) goto invalid_off;
                        if(end < 0) end += cmd_history_end;
                        if(end >= cmd_history_end) goto invalid_off;
                        end++;
                    }
                }
                else
                {
                    /* positive offet given */
                    start = strtol(__optarg, &pend, 10);
                    if(pend == __optarg || start > cmd_history_end) goto invalid_off;
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
                
            case 'p':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: -%c option is missing arguments\r\n", UTILITY, 'p');
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
                else return 1;
                
            case 's':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: -%c option is missing arguments\r\n", UTILITY, 's');
                    return 2;
                }
                p = list_to_str(&argv[v], 0);
                if(p)
                {
                    save_to_history(p);
                    free(p);
                }
                return 0;
                
            /* tcsh uses -S instead of -w, which is used by bash */
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
    if(c == -1) return 2;

    /* show [n] history entries */
    if(v < argc)
    {
        char *strend;
        start = strtol(argv[v], &strend, 10);
        if(strend == argv[v] || start < 0) start = 0;
        start = cmd_history_end-start;
    }
    
    if(cmd_history_end == 0) return 0;

    if(reverse)
    {
        for(i = cmd_history_end-1; i >= start; i--)
        {
            print_hist_entry(cmd_history[i].cmd, fmt, i, supp_nums);
        }
    }
    else
    {
        for(i = start; i < cmd_history_end; i++)
        {
            print_hist_entry(cmd_history[i].cmd, fmt, i, supp_nums);
        }
    }
    return 0;
    
invalid_off:
    fprintf(stderr, "%s: invalid offset passed to -d option: %s\r\n", UTILITY, __optarg);
    return 2;
}
