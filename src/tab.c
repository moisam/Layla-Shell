/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: tab.c
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
#include <ctype.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>
#include "cmd.h"
#include "getkey.h"
#include "builtins/setx.h"
#include "symtab/symtab.h"
#include "backend/backend.h"
#include "vi.h"
#include "debug.h"

#define MAX_CMDS    2048

char  *HOSTS_FILE    = "/etc/hosts";    /* default hosts file */
char **hostnames     = NULL;            /* hostnames buffer */
int    hn_len        = 0;               /* max number of entries (size of buffer) */
int    hn_count      = 0;               /* number of entries */
time_t hn_last_check = 0;               /* last time we accessed the hosts file */

char  *PASSWD_FILE   = "/etc/passwd";   /* default passwd database file */
char **usernames     = NULL;            /* user names buffer */
int    un_len        = 0;               /* max number of entries (size of buffer) */
int    un_count      = 0;               /* number of entries */
time_t un_last_check = 0;               /* last time we accessed the passwd file */

int    match_hostname(char *name, char **matches, int max);
int    match_username(char *name, char **matches, int max);
char **get_hostnames();
char **get_usernames();


/*
 * perform auto-completion for filenames, matching files in the given *dir to the
 * given *path, which is treated as a regex pattern that specifies which filename(s)
 * we should match.. this happens when the user hits tab after entering a partial
 * command name.. this process is similar to pathname expansion, which is done by
 * get_filename_matches() (see backend/pattern.c).
 *
 * returns a char ** pointer to the list of matched filenames, or NULL if nothing matched.
 */
char **get_name_matches(char *dir, char *path, glob_t *matches)
{
    /* to guard our caller from trying to free an unused struct in case of expansion failure */
    matches->gl_pathc = 0;
    matches->gl_pathv = NULL;

    if(!dir || !path)
    {
        return NULL;
    }

    if(strcmp(dir, cwd) != 0)
    {
        if(chdir(dir) == -1)
        {
            return NULL;
        }
    }

    /* set up the flags */
    int flags = 0;
    if(optionx_set(OPTION_ADD_SUFFIX)) flags |= GLOB_MARK  ;

    /* perform the match */
    if(glob(path, flags, NULL, matches) != 0)
    {
        globfree(matches);
        if(dir != cwd)
        {
            chdir(cwd);
        }
        return NULL;
    }

    if(dir != cwd)
    {
        chdir(cwd);
    }
    return matches->gl_pathv;
}


/*
 * auto-complete a pathname when the user enters a partial pathname and presses
 * tab.. the __count parameter contains the number of entries already stored in
 * the **results array.. the function saves the matched pathnames in the **results
 * array and returns the count of the matched pathnames in addition to __count.
 */
int autocomplete_path(char *file, char **results, int __count)
{
    extern char *__next_path;
    extern char *get_next_filename(char *__path, int *n, int report_err);
    extern struct dirent **eps;
    extern char *default_path;      /* builtins/command.c */

    char *PATH = get_shell_varp("PATH", default_path);
    char *p    = PATH;
    char *p2;
    int  count = __count;
    
    /* search the next directory in $PATH for possible matches */
    while(p && *p)
    {
        /* get the next entry in $PATH */
        p2 = p;
        while(*p2 && *p2 != ':')
        {
            p2++;
        }
        int  plen = p2-p;
        if(!plen)
        {
            plen = 1;
        }
        /* copy the entry */
        char path[plen+1];
        strncpy(path, p, p2-p);
        path[p2-p] = '\0';
        if(path[plen-1] == '/')
        {
            path[plen-1] = '\0';
        }
        /* search for possible matches in the directory */
        int matches_count = 0;
        int i;
        struct stat st;
        __next_path = NULL;
        char *next = get_next_filename(path, &matches_count, 0);
        /* no matches. move on to the next $PATH entry */
        if(!next)
        {
            p = p2;
            if(*p2 == ':')
            {
                p++;
            }
            continue;
        }
        do
        {
            /* we have a matching name */
            if(strstr(next, file) == next)
            {
                char exefile[plen+strlen(next)+2];
                sprintf(exefile, "%s/%s", path, next);
                if(stat(exefile, &st) == 0)
                {
                    if(!S_ISREG(st.st_mode))    /* not a regular file */
                    {
                        continue;
                    }
                    /* if we should recognize only exe files, check if the file is executable */
                    if(optionx_set(OPTION_RECOGNIZE_ONLY_EXE) && access(path, X_OK) != 0)
                    {
                        continue;
                    }
                    /* check for duplicates */
                    int duplicate = 0;
                    for(i = 0; i < count; i++)
                    {
                        if(strcmp(results[i], next) == 0)
                        {
                            duplicate = 1;
                            break;
                        }
                    }
                    if(duplicate)
                    {
                        continue;
                    }
                    /* add the command to our list */
                    results[count++] = get_malloced_str(next);
                    if(count >= MAX_CMDS)
                    {
                        break;
                    }
                }
            }
        } while((next = get_next_filename(path, NULL, 0)));
        /* free temp memory */
        for(i = 0; i < matches_count; i++)
        {
            free(eps[i]);
        }
        free(eps);
        /* maximum number of entries reached */
        if(count >= MAX_CMDS)
        {
            return count;
        }
        /* move on to the next $PATH entry */
        p = p2;
        if(*p2 == ':')
        {
            p++;
        }
    }
    return count;
}


/*
 * get the longest column's width for formatting our output.. the **cmds parameter
 * holds the matched entries which we are going to print, while count is their count.
 * 
 * returns the column width that is guaranteed to accommodate all of the entries.
 */
int get_col_width(char **cmds, int count)
{
    int k;
    int w = 0;
    /* find the longest entry */
    for(k = 0; k < count; k++)
    {
        int len = strlen(cmds[k]);
        if(len > w)
        {
            w = len+1;
        }
    }
    /* try to put 4 columns on the screen */
    k = VGA_WIDTH/4;
    if(w > VGA_WIDTH)   /* our width is larger than the screen's */
    {
        w = VGA_WIDTH;
    }
    else if(w < k)      /* our width is smaller than screen width/4 */
    {
        w = k;
    }
    return w;
}


/*
 * print the list of matched entries, **cmds, which contains count entries,
 * adjusted to a column length of w.
 */
void __output_results(char **cmds, int count, int w)
{
    int k, col = 0, col2;
    for(k = 0; k < count; k++)
    {
        printf("%s", cmds[k]);
        col2 = col+w;
        col += strlen(cmds[k]);
        printf("%*s", col2-col, " ");
        if(col2+w > VGA_WIDTH)
        {
            putchar('\n');
            col = 0;
        }
        else
        {
            col = col2;
        }
    }
    if(count % (VGA_WIDTH/w))
    {
        putchar('\n');
    }
}


/*
 * output the results of tab completion.
 */
void output_results(char **cmds, int count)
{
    putchar('\r');
    putchar('\n');
    /* get the longest field width */
    int w = get_col_width(cmds, count);
    int cols  = VGA_WIDTH/w;
    int lines = count/cols;
    if(lines >= VGA_HEIGHT)
    {
        /* we have more lines than can be printed on one screen */
        printf("Show all %d results? [y/N]: ", count);
        term_canon(1);
        int c = getc(stdin);
        if(c == 'y' || c == 'Y')
        {
            __output_results(cmds, count, w);
        }
        term_canon(0);
    }
    else
    {
        __output_results(cmds, count, w);
    }
}


/*
 * get the common prefix from the list of results.
 */
char *get_common_prefix(char **cmds, int count)
{
    if(!cmds || !count)
    {
        return NULL;
    }
    int i = 0, j = 0, nomatch;
    char c;
    while(1)
    {
        c = cmds[0][j];
        nomatch = 0;
        for(i = 1; i < count; i++)
        {
            if(cmds[i][j] != c)
            {
                nomatch = 1;
                break;
            }
        }
        if(nomatch)
        {
            break;
        }
        j++;
    }
    if(j == 0)
    {
        return NULL;
    }
    return get_malloced_strl(cmds[0], 0, j);
}


/*
 * this procedure will do command, variable and filename auto-completion.
 * 
 * returns 1 if tab completion is successful, 0 and beep otherwise.
 */
int do_tab(char *cmdbuf, uint16_t *__cmdbuf_index, uint16_t *__cmdbuf_end)
{
    extern int start_row, start_col;
    uint16_t cmdbuf_index = *__cmdbuf_index;
    uint16_t cmdbuf_end   = *__cmdbuf_end  ;
    uint16_t j, k, i    = 0;
    int      is_cmd     = 0;
    int      first_word = 0;

    i = cmdbuf_index;
    if(i != 0)
    {
        i--;
    }
    /* get start of cur word */
    int endme = 0;
    while(i != 0)
    {
        switch(cmdbuf[i])
        {
            case '@':
            case '~':
            case '$':
                endme = 1;
                break;

            default:
                if(isspace(cmdbuf[i]))
                {
                    if(cmdbuf[i-1]  != '\\')
                    {
                        endme = 1;
                        break;
                    }
                }
                break;
        }
        if(endme)
        {
            break;
        }
        i--;
    }
#if 0
    while(!isspace(cmdbuf[i]) && cmdbuf[i] != '@' && 
          cmdbuf[i] != '~' && cmdbuf[i] != '$' && i != 0)
    {
        i--;
    }
#endif
    if(isspace(cmdbuf[i]))
    {
        i++;
    }
    /* are we at the start of the command line? */
    if(i == 0)
    {
        j = cmdbuf_index, first_word = 1;
    }
    else if(i >= cmdbuf_index)
    {
        j = i;          /* we've got an empty word */
    }
    else
    {
        j = i-1;
        while(isspace(cmdbuf[j]) && j != 0)
        {
            j--;    /* skip prev spaces */
        }
        if(cmdbuf[j] == ';' || cmdbuf[j] == '|' || cmdbuf[j] == '&' ||
           cmdbuf[j] == '(' || cmdbuf[j] == '{' || cmdbuf[j] == ' ')
        {
            first_word = 1;
        }
        j = cmdbuf_index;
    }
    /*
     * is it a command word or a filename? command word must be the first in line,
     * or the first after an operator (like ; | ( { for example). command words should
     * also not start with ~ and not contain slashes.
     */
    if(first_word)
    {
        if(cmdbuf[i] != '~')
        {
            k = 0;
            j = i;
            while(j < cmdbuf_end)
            {
                switch(cmdbuf[j])
                {
                    case '/':
                        k = 1;
                        break;
                        
                    case ' ':
                    case '\t':
                    case '\n':
                    case '\r':
                        if(!k)
                        {
                            k = 2;
                        }
                        break;
                }
                if(k == 2)
                {
                    break;
                }
                j++;
            }
            if(k != 1)
            {
                is_cmd = 1;
            }
        }
    }
    /* copy the word we are about to tab-complete */
    char  tmp[j-i+2];    /* 2 chars for '\0' and possible trailing '*' */
    int   eword = j;
    int   res = 0;
    char *cmds[MAX_CMDS];
    char *p, *p2, *p3;
    char *comm_prefix = NULL;

    /* copy the word, handling backslash-escaped chars */
    p  = cmdbuf+i;
    p2 = tmp;
    p3 = p+j;
    while(p < p3)
    {
        /* check the backslash is not the last char in the word */
        if(*p == '\\' && p[1])
        {
            /* skip the backslash */
            p++;
        }
        *p2++ = *p++;
    }
    *p2 = '\0';
    //strncpy(tmp, cmdbuf+i, j-i);
    //tmp[j-i] = '\0';
    
    /*
     * auto-completion for host names.
     */
    if(optionx_set(OPTION_HOST_COMPLETE) && (p = strchr(tmp, '@')))
    {
        res = match_hostname(p+1, cmds, MAX_CMDS);
        
        if(res)     /* matches found */
        {
            if(res == 1)
            {
                p = cmds[0]+strlen(tmp)-1;  /* subtract 1 for the @ */
                goto one_res;
            }
            /* output the results */
            output_results(cmds, res);
            comm_prefix = get_common_prefix(cmds, res);
            if(comm_prefix)
            {
                p = comm_prefix+strlen(tmp)-1;
            }
            /* free used memory */
            for(j = 0; j < res; j++)
            {
                free(cmds[j]);
            }
        }
        else        /* no matches found */
        {
            beep();  /* ring a bell */
            return 0;
        }
    }
    /*
     * auto-completion for user names, which start with ~. if the ~ is followed by /,
     * we most probably want to list the contents of our home directory, not search for a user
     * whose name starts with /.
     */
    else if(optionx_set(OPTION_USER_COMPLETE) && (p = strchr(tmp, '~')) && p[1] != '/')
    {
        res = match_username(p+1, cmds, MAX_CMDS);
        
        if(res)     /* matches found */
        {
            if(res == 1)
            {
                p = cmds[0]+strlen(tmp)-1;  /* subtract 1 for the ~ */
                goto one_res;
            }
            /* output the results */
            output_results(cmds, res);
            comm_prefix = get_common_prefix(cmds, res);
            if(comm_prefix)
            {
                p = comm_prefix+strlen(tmp)-1;
            }
            /* free used memory */
            for(j = 0; j < res; j++)
            {
                free(cmds[j]);
            }
        }
        else        /* no matches found */
        {
            beep();  /* ring a bell */
            return 0;
        }
    }
    /*
     * auto-completion for variable names.
     */
    else if(tmp[0] == '$')
    {
        char *vars = get_all_vars(tmp+1);
        if(!vars)
        {
            beep();  /* ring a bell */
            return 0;
        }
        j = strlen(vars);
        res = 0;
        cmds[res++] = vars;
        for(i = 0; i < j; i++)
        {
            if(vars[i] == ' ')
            {
                vars[i] = '\0';
                if(vars[i+1] != '\0')
                {
                    cmds[res++] = vars+i+1;
                    if(res == MAX_CMDS)
                    {
                        break;
                    }
                }
            }
        }
        if(res == 1)        /* one match found */
        {
            p = cmds[0]+strlen(tmp)-1;
            goto one_res;
        }
        /* output the results */
        output_results(cmds, res);
        comm_prefix = get_common_prefix(cmds, res);
        if(comm_prefix)
        {
            p = comm_prefix+strlen(tmp)-1;
        }
        /* free used memory */
        free(vars);
    }
    /*
     * auto-completion for command words.
     */
    else if(is_cmd)
    {
        /* search for compatible builtin commands */
        for(k = 0; k < special_builtin_count; k++)
        {
            char *cmd = special_builtins[k].name;
            if(strstr(cmd, tmp) == cmd)
            {
                cmds[res++] = cmd;
            }
        }
        for(k = 0; k < regular_builtin_count; k++)
        {
            char *cmd = regular_builtins[k].name;
            if(strstr(cmd, tmp) == cmd)
            {
                cmds[res++] = cmd;
            }
        }
        /* search for aliases */
        extern struct  alias_s __aliases[MAX_ALIASES];  /* builtins/alias.c */
        for(i = 0; i < MAX_ALIASES; i++)
        {
            char *cmd = __aliases[i].name;
            if(!cmd)
            {
                continue;
            }
            if(strstr(cmd, tmp) != cmd)
            {
                continue;
            }
            cmds[res++] = cmd;
            /* maximum results reached */
            if(res == MAX_CMDS)
            {
                break;
            }
        }
        
        /* search for defined functions */
        struct symtab_entry_s *entry = get_symtab_entry(tmp);
        if(entry && entry->val_type == SYM_FUNC)
        {
            cmds[res++] = entry->name;
        }

        int internals = res;
        /* search for stand-alone commands using $PATH */
        res = autocomplete_path(tmp, cmds, res);
    
        /* print list of results */
        if(res == 0)        /* no matches found */
        {
            beep();  /* ring a bell */
            return 0;
        }
        else if(res == 1)   /* one match found */
        {
            p = cmds[0]+strlen(tmp);
            printf("%s", p);
            strcat(cmdbuf, p);
            if(p[strlen(p)-1] != '/' && optionx_set(OPTION_ADD_SUFFIX))
            {
                putchar(' ');
                strcat(cmdbuf, " ");
            }
            *__cmdbuf_index = strlen(cmdbuf);
            *__cmdbuf_end   = *__cmdbuf_index;
            if(!internals)
            {
                free_malloced_str(cmds[0]);
            }
            return 1;
        }
        /* output the results */
        output_results(cmds, res);
        comm_prefix = get_common_prefix(cmds, res);
        if(comm_prefix)
        {
            p = comm_prefix+strlen(tmp);
        }
        /* free used memory */
        for(i = internals; i < res; i++)
        {
            free_malloced_str(cmds[i]);
        }
    }
    else
    {
        /*
         * auto-completion for file names.
         */
        i = strlen(tmp);
        /* append '*' only if tmp doesn't contain regex chars *, ? and [ */
        int star = 0;
        if(!has_regex_chars(tmp, i))
        {
            tmp[i] = '*';
            tmp[i+1] = '\0';
            star = 1;
        }
        glob_t glob;
        char **matches = NULL;

        char *slash = strrchr(tmp, '/');
        if(slash)       /* file name has a slash char */
        {
            *slash = '\0';
            /* perform word expansion */
            char *dir = word_expand_to_str(tmp);
            if(dir)
            {
                if(*dir)
                {
                    matches = get_name_matches(dir, slash+1, &glob);
                }
                else
                {
                    matches = get_name_matches("/", slash+1, &glob);
                }
                free(dir);
            }
            else
            {
                if(*tmp)
                {
                    matches = get_name_matches(tmp, slash+1, &glob);
                }
                else
                {
                    matches = get_name_matches("/", slash+1, &glob);
                }
            }
            *slash = '/';
        }
        else            /* file name with no slash chars */
        {
            matches = get_name_matches(cwd, tmp, &glob);
        }
        
        if(matches)
        {
            if(matches[0])
            {
                res = 0;
                for(j = 0; j < glob.gl_pathc; j++)
                {
                    if(matches[j][0] == '.')
                    {
                        /* skip '..' and '.' */
                        if(matches[j][1] == '.' || matches[j][1] == '/' || matches[j][1] == '\0')
                        {
                            continue;
                        }
                    }
                    cmds[res++] = get_malloced_str(matches[j]);
                    /* maximum results reached */
                    if(res == MAX_CMDS)
                    {
                        break;
                    }
                }
                globfree(&glob);
                if(res == 1)        /* found one match */
                {
                    if(slash)
                    {
                        p = cmds[0]+strlen(slash+1);
                    }
                    else
                    {
                        p = cmds[0]+strlen(tmp);
                    }

                    if(star)
                    {
                        p--;
                    }
                    goto one_res;
                }
                /* output the results */
                output_results(cmds, res);
                comm_prefix = get_common_prefix(cmds, res);
                if(comm_prefix)
                {
                    if(slash)
                    {
                        p = comm_prefix+strlen(slash+1);
                    }
                    else
                    {
                        p = comm_prefix+strlen(tmp);
                    }

                    if(star)
                    {
                        p--;
                    }
                }
            }
            else        /* no matches found */
            {
                beep();  /* ring a bell */
                globfree(&glob);
                return 0;
            }
        }
        else            /* no matches found */
        {
            beep();  /* ring a bell */
            globfree(&glob);
            return 0;
        }
        globfree(&glob);
    }

    printf("\n");
    print_prompt();
    update_row_col();
    start_row = get_terminal_row();
    start_col = get_terminal_col();
    printf("%s", cmdbuf);
    if(!comm_prefix)
    {
        /* return the cursor to where it was */
        if(cmdbuf_index != cmdbuf_end)
        {
            do_left_key(cmdbuf_end-cmdbuf_index);
        }
        return res;
    }
    cmds[0] = NULL;
    
one_res: ;
    /* we will add a space after a complete file name, a slash after a directory name */
    int plen = strlen(p);
    int addsp = (p[plen-1] != '/' && cmdbuf[eword] != ' ');
    if(comm_prefix || !optionx_set(OPTION_ADD_SUFFIX))
    {
        addsp = 0;
    }
    if(optionx_set(OPTION_COMPLETE_FULL_QUOTE))
    {
        char *p2 = p;
        while(*p2)
        {
            /* escape quotes with backslash chars */
            if(*p2 == '$' || *p2 == '`' || *p2 == '"' || *p2 == '\'' || *p2 == '\\' || *p2 == ' ')
            {
                plen++;
                putchar('\\');
            }
            putchar(*p2++);
        }
        /* add a space after a file name */
        if(addsp)
        {
            putchar(' ');
        }
        printf("%s", cmdbuf+eword);
        /* make some room */
        p2 = cmdbuf+cmdbuf_end;
        char *p3 = p2+plen;
        char *p4 = cmdbuf+eword;
        if(addsp)
        {
            p3++;
        }
        while(p2 >= p4)
        {
            *p3-- = *p2--;
        }
        /* copy the word to the buffer */
        p2 = p;
        p3 = cmdbuf+eword;
        while(*p2)
        {
            /* escape quotes with backslash chars */
            if(*p2 == '$' || *p2 == '`' || *p2 == '"' || *p2 == '\'' || *p2 == '\\' || *p2 == ' ')
            {
                *p3++ = '\\';
            }
            *p3++ = *p2++;
        }
        /* add a space after a file name */
        if(addsp)
        {
            *p3 = ' ';
        }
    }
    else
    {
        printf("%s", p);
        /* add a space after a file name */
        if(addsp)
        {
            putchar(' ');
        }
        printf("%s", cmdbuf+eword);
        /* make some room */
        char *p2 = cmdbuf+cmdbuf_end;
        char *p3 = p2+plen;
        char *p4 = cmdbuf+eword;
        if(addsp)
        {
            p2++, p3++;
        }
        while(p2 >= p4)
        {
            *p3-- = *p2--;
        }
        /* copy the word */
        if(addsp)
        {
            cmdbuf[eword++] = ' ';
        }
        strncpy(cmdbuf+eword, p, plen);
    }
    *__cmdbuf_index = strlen(cmdbuf);
    *__cmdbuf_end   = *__cmdbuf_index;
    update_row_col();
    if(cmds[0])
    {
        free_malloced_str(cmds[0]);
    }
    if(comm_prefix)
    {
        free(comm_prefix);
    }
    return 1;
}

/*
 * match a partial hostname to the hostnames database entries.
 * 
 * returns the number of matched names, saving the entries in the matches array.
 */
int match_hostname(char *name, char **matches, int max)
{
    /* get the hostnames */
    if(!get_hostnames())
    {
        return 0;
    }
    int i = 0, j = 0;
    int len = strlen(name);
    /* search for matches */
    for( ; i < hn_count && j < max; i++)
    {
        if(strncmp(name, hostnames[i], len) == 0)
        {
            matches[j++] = get_malloced_str(hostnames[i]);
        }
    }
    return j;
}

/*
 * get all the hostnames from the hosts database.
 */
char **get_hostnames()
{
    int checkf = 0;
    int i = 0;
    char *path = get_shell_varp("HOSTFILE", HOSTS_FILE);
    struct stat st;
    /* check the existence and modification time of the file */
    if((i = stat(path, &st)) == 0)
    {
        if(S_ISREG(st.st_mode) && difftime(st.st_mtime, hn_last_check) > 0)
        {
            checkf = 1;
        }
    }
    else
    {
        /* invalid hosts file given in $HOSTFILE */
        if(i && path != HOSTS_FILE)
        {
            /* try to check the standard hosts file */
            path = HOSTS_FILE;
            if(stat(path, &st) == 0)
            {
                if(S_ISREG(st.st_mode) && difftime(st.st_mtime, hn_last_check) > 0)
                {
                    checkf = 1;
                }
            }
        }
    }
    /* first call or hosts file has been modified since our last access */
    if(!hostnames || checkf)
    {
        if(hostnames)
        {
            /*
             * free the entries we have stored in the host names array.
             * 
             * TODO: bash adds new entries to the list, instead of overwriting it.
             *       follow that behaviour.
             */
            for(i = 0; i < hn_count; i++)
            {
                free_malloced_str(hostnames[i]);
            }
            free(hostnames);
            hostnames = NULL;
            hn_len    = 0;
            hn_count  = 0;
        }        
        hn_last_check = time(NULL);
        /*
         * read the host names from the hosts file. each line in the file
         * represents an entry in the format:
         * 
         *     127.0.0.1   localhost localhost.localdomain localhost4 localhost4.localdomain4
         *     ^           ^         ^                     ^          ^
         *     |           |         |---------------------|----------|
         *     IP address  hostname                     aliases
         */
        char *line = NULL;
        FILE *f = fopen(path, "r");
        if(!f)
        {
            return NULL;
        }
        int line_max = get_linemax();
        char buf[line_max];
        while((line = fgets(buf, line_max, f)))
        {
            if(!line[0])    /* empty line. skip */
            {
                continue;
            }
            char *p = line, *p2;
            /* skip the host address */
            while(*p && !isspace(*p))
            {
                p++;
            }
            /* reached the end of line */
            if(!*p)
            {
                continue;
            }
            /* get the host name and its aliases */
            while(*p)
            {
                /* extend the buffer */
                if(!check_buffer_bounds(&hn_count, &hn_len, &hostnames))
                {
                    /* insufficient memory. return the list we've got so far */
                    return hostnames;
                }
                /* skip the spaces */
                while(*p && isspace(*p))
                {
                    p++;
                }
                /* reached the end of line */
                if(!*p)
                {
                    break;
                }
                /* get the end of the name */
                p2 = p;
                while(*p2 && !isspace(*p2))
                {
                    p2++;
                }
                if(p == p2)
                {
                    break;
                }
                /* get a copy of the hostname */
                char *host = get_malloced_strl(line, p-line, p2-p);
                /* don't repeat entries */
                int found = 0;
                for(i = 0; i < hn_count; i++)
                {
                    if(strcmp(hostnames[i], host) == 0)
                    {
                        free_malloced_str(host);
                        host = NULL;
                        found = 1;
                        break;
                    }
                }
                /* add the hostname to the list */
                if(!found && host)
                {
                    hostnames[hn_count++] = host;
                }
                p = p2;
            }
        }
    }
    /* return the hostnames list */
    return hostnames;
}


/*
 * match a partial username to the passwd database entries.
 * 
 * returns the number of matched names, saving the entries in the matches array.
 */
int match_username(char *name, char **matches, int max)
{
    /* get the usernames */
    if(!get_usernames())
    {
        return 0;
    }
    int i = 0, j = 0;
    int len = strlen(name);
    /* search for the username */
    for( ; i < un_count && j < max; i++)
    {
        if(strncmp(name, usernames[i], len) == 0)
        {
            matches[j++] = get_malloced_str(usernames[i]);
        }
    }
    return j;
}

/*
 * get all the usernames from the passwd database.
 */
char **get_usernames()
{
    int checkf = 0;
    int i = 0;
    char *path = PASSWD_FILE;
    struct stat st;
    /* check the existence and modification time of the file */
    if((i = stat(path, &st)) == 0)
    {
        if(S_ISREG(st.st_mode) && difftime(st.st_mtime, un_last_check) > 0)
        {
            checkf = 1;
        }
    }
    /* first call or passwd file has been modified since our last access */
    if(!usernames || checkf)
    {
        if(usernames)
        {
            /*
             * free the entries we have stored in the user names array.
             * 
             * TODO: add new entries to the list, instead of overwriting it.
             */
            for(i = 0; i < un_count; i++)
            {
                free_malloced_str(usernames[i]);
            }
            free(usernames);
            usernames = NULL;
            un_len    = 0;
            un_count  = 0;
        }        
        un_last_check = time(NULL);
        /*
         * read the user names from the passwd file. each line in the file
         * represents an entry in the format:
         * 
         *     root:x:0:0:root:/root:/bin/bash
         *     ^    ^ ^ ^ ^    ^     ^
         *     |    | | | |    |     +---------------+
         *     |    | | | |    +------------+        |
         *     |    | | | +----------+      |        |
         *     |    | | +-------+    |      |        |
         *     |    | +----+    |    |      |        |
         *     |    ++     |    |    |      |        |
         *     |     |     |    |    |      |        |
         *     user  pass  uid  gid  group  homedir  login shell
         */
        char *line = NULL;
        FILE *f = fopen(path, "r");
        if(!f)
        {
            return NULL;
        }
        int line_max = get_linemax();
        char buf[line_max];
        while((line = fgets(buf, line_max, f)))
        {
            if(!line[0])        /* empty line. skip */
            {
                continue;
            }
            char *p = line;
            /* get to the first colon */
            while(*p && *p != ':')
            {
                p++;
            }
            /* extend the buffer */
            if(!check_buffer_bounds(&un_count, &un_len, &usernames))
            {
                /* insufficient memory. return the list we've got so far */
                return usernames;
            }
            /* get the user name */
            char *user = get_malloced_strl(line, 0, p-line+1);
            user[p-line] = '/';
            /* don't repeat entries */
            int found = 0;
            for(i = 0; i < un_count; i++)
            {
                if(strcmp(usernames[i], user) == 0)
                {
                    free_malloced_str(user);
                    user = NULL;
                    found = 1;
                    break;
                }
            }
            if(!found && user)
            {
                usernames[un_count++] = user;
            }
        }
    }
    /* return the usernames list */
    return usernames;
}
