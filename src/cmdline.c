/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: cmdline.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include "cmd.h"
#include "vi.h"
// #include "getkey.h"
#include "kbdevent.h"
#include "scanner/scanner.h"
#include "scanner/source.h"
#include "parser/parser.h"
#include "parser/node.h"
#include "backend/backend.h"
#include "builtins/setx.h"
#include "debug.h"


/* we will save incomplete commands here until being processed */
char     *incomplete_cmd = (char *)NULL;
/* our main command line buffer */
char     *cmdbuf         = NULL;        /* ptr to buffer */
uint16_t  cmdbuf_index   = 0;           /* whrere to add incoming key */
uint16_t  cmdbuf_end     = 0;           /* index of last entered key */
uint16_t  cmdbuf_size    = 0;           /* actual malloc'd size` */
//#define   CMD_BUF_SIZE	 ((LINE_MAX)-1)
long      CMD_BUF_SIZE   = 0;
/* the marker word that indicates the end of a here-document */
//char     *heredoc_mark   = NULL;
/* flag to tell us if we are inside a here-document */
int       in_heredoc     = -1;
/* assume heredocs for every possible file descriptor */
char     *heredoc_mark [FOPEN_MAX] = { 0, };
/* if operator "<<-" is used (instead of "<<"), strip TABs from the heredoc */
//int       strip_heredoc[FOPEN_MAX] = { 0, };
/* how many heredocs expected */
int       heredocs       = 0;
int       terminal_row   = 0;
int       terminal_col   = 0;
int       VGA_WIDTH      = 80;
int       VGA_HEIGHT     = 25;
int       start_row      = 0;
int       start_col      = 0;
/* flag to indicate if INS was pressed */
int       insert         = 0;
char     *stdin_filename = "STDIN";

/* 
 * flag to indicate if a SIGALRM signal was received as a result of the user
 * setting the $TPERIOD shell variable (which works similar to tcsh's tperiod).
 */
int       do_periodic    = 0;
timer_t   timerid        = 0;          /* id of the timer used to send SIGALRM */

/* max EOFs before exit */
#define MAX_EOFS        10

/* variables used by is_incomplete_cmd() function */
static int open_cb = 0, close_cb = 0;      /* count curly braces */
static int open_rb = 0, close_rb = 0;      /* count round brackets */
static int quotes  = 0;
static int __heredocs = 0;


void kill_input()
{
    cmdbuf_index = 0;
    cmdbuf_end   = 0;
    fprintf(stderr, "\r\n");
    print_prompt();
    cmdbuf[0]    = '\0';
    update_row_col();
    start_row    = get_terminal_row();
    start_col    = get_terminal_col();
}

/*
 * main program loop.
 */
void cmdline()
{
    char *cmd;
    int  stat = EXIT_SUCCESS;
    //signal(SIGINT , SIGINT_handler);
    //SIGINT_received = 0;
    
    if(optionx_set(OPTION_CLEAR_SCREEN)) clear_screen();
    fprintf(stdout, "\r\n\nWelcome to Layla shell\r\n\n");
    int i;
    for(i = 0; i < FOPEN_MAX; i++) heredoc_mark[i] = NULL;
        
    do
    {
        cmd_history_index = cmd_history_end;
        if(option_set('m')) check_on_children();
        if(check_for_mail()) mail(2, (char *[]){"mail", "-q", NULL});
        /* in tcsh, special alias periodic is run every tperiod minutes */
        if(do_periodic)
        {
            run_alias_cmd("periodic");
            do_periodic = 0;
        }
        /* in tcsh, special alias precmd is run before printing the next command prompt */
        run_alias_cmd("precmd");
        /* print the prompt */
        print_prompt();
        if(!ready_to_read(0)) break;
        term_canon(0);
        cmd = read_cmd();
        if(!cmd)
        {
            /* accept EOF only if 'ignoreeof' option is not set */
            if(option_set('o'))
            {
                /* tcsh outputs a message in this case */
                fprintf(stderr, "Use \"exit\" to leave\r\n");
                continue;
            }
            exit_gracefully(stat, NULL);
        }
        if(cmd[0] == '\0' || strcmp(cmd, "\n") == 0) continue;

        __src.buffer   = cmd;
        //__src.bufsize  = len;
        __src.bufsize  = cmdbuf_end;
        __src.filename = stdin_filename;
        __src.curpos   = -2;

        do_cmd();
    } while(1);
}

/* extend the buffer by CMD_BUF_SIZE+1 bytes */
int ext_cmdbuf(size_t howmuch)
{
    uint16_t newsz = cmdbuf_size + howmuch;
    char *newbuf = (char *)realloc(cmdbuf, newsz);
    if(!newbuf) return 0;
    cmdbuf       = newbuf;
    cmdbuf_size  = newsz;
    return 1;
}

/* read command line into buffer */
char *read_cmd()
{
    cmdbuf_index = 0;
    cmdbuf_end   = 0;
    if(!cmdbuf)
    {
        CMD_BUF_SIZE = sysconf(_SC_LINE_MAX);
        if(CMD_BUF_SIZE <= 0) CMD_BUF_SIZE = DEFAULT_LINE_MAX;
        cmdbuf = (char *)malloc(CMD_BUF_SIZE);
        if(!cmdbuf)
        {
            exit_gracefully(EXIT_FAILURE, "FATAL ERROR: Insufficient memory for command buffer");
        }
        cmdbuf_size  = CMD_BUF_SIZE;
    }        
    cmdbuf[0] = '\0';
    update_row_col();
    if(get_terminal_row() == 0 || get_terminal_col() == 0) update_row_col();
    start_row = get_terminal_row();
    start_col = get_terminal_col();
    int c;
    int tabs = 0;
    char *p;
    
    /* get the number of consecutive EOFs to force exit. default is 10 (bash) or 1 (tcsh) */
    struct symtab_entry_s *entry = get_symtab_entry("IGNOREEOF");
    int eofs = 0, max_eofs = 0;
    if(entry)
    {
        if(entry->val)
        {
            char *strend = NULL;
            max_eofs = strtol(entry->val, &strend, 10);
            if(strend == entry->val) max_eofs = MAX_EOFS;
        }
        else max_eofs = MAX_EOFS;
    }
    
    /* tcsh has an 'inputmode' variable that determins the input mode (insert/overwrite) */
    char *inputm = get_shell_varp("INSERTMODE", "insert");
    if(inputm)
    {
        if(strcmp(inputm, "overwrite") == 0)
             insert = 1;
        else insert = 0;
    }
    
    while(1)
    {
        /***********************
         * get next key stroke
         ***********************/
        c = get_next_key();
        if(c == EOF_KEY)
        {
            if(cmdbuf_index == 0)
            {
                if(++eofs >= max_eofs) return NULL;
                else continue;
            }
            cmdbuf[cmdbuf_index] = '\0';
            return cmdbuf;
        }
        if(c == 0) continue;

        if(c != '\t') tabs = 0;
        else          tabs++;
        eofs = 0;

        switch(c)
        {
            default:
                /*********************************** 
                 * some special keys
                 ***********************************/
                if(c == ERASE_KEY)          /* default ^H */
                {
                    do_backspace(1);
                }
                else if(c == KILL_KEY)      /* default ^U */
                {
                    do_kill_key();
                }
                else if(c == INTR_KEY)      /* default ^C */
                {
                    print_ctrl_key(INTR_KEY);
                    kill_input();
                }
                else if(c == VLNEXT_KEY)
                {
                    c = get_next_key();
                    do_insert(c);
                }
                else if((c >= ' ' && c <= '~'))
                {
                    do_insert(c);
                }
                break;
                
            case UP_KEY:
                do_up_key(1);
                break;
                
            case DOWN_KEY:
                do_down_key(1);
                break;
                
            case LEFT_KEY:
                do_left_key(1);
                break;

            case RIGHT_KEY:
                do_right_key(1);
                break;

            case HOME_KEY:
                do_home_key();
                break;

            case END_KEY:
                do_end_key();
                break;

            case '\b':
                do_backspace(1);
                break;

            case DEL_KEY:
                do_del_key(1);
                break;
                
            case INS_KEY:
                insert = !insert;
                break;

            /*********************************** 
             * tab
             ***********************************/
            case '\t':
                if(tabs == 1)       /* first time, ring a bell */
                {
                    //beep();
                }
                else
                {
                    /* this procedure will do command auto-completion */
                    do_tab(cmdbuf, &cmdbuf_index, &cmdbuf_end);
                }
                break;

            case CTRLW_KEY:
                if(cmdbuf_index == 0) continue;
                int z = cmdbuf_index-1;
                while(z >= 0)
                {
                    if(cmdbuf[z] == ' ' || ispunct(cmdbuf[z]))
                    {
                        z++; break;
                    }
                    if(z == 0) break;
                    z--;
                }
                if(z == cmdbuf_index) continue;
                update_row_col();
                size_t old_row = terminal_row, old_col = terminal_col;
                move_cur(start_row, start_col);
                printf("%*s", cmdbuf_end, " ");
                char *p1 = cmdbuf+z;
                char *p2 = cmdbuf+cmdbuf_index;
                while((*p1++ = *p2++)) ;
                int  diff = cmdbuf_index-z;
                cmdbuf_index -= diff;
                cmdbuf_end   -= diff;
                move_cur(start_row, start_col);
                printf("%s", cmdbuf);
                do
                {
                    if(old_col == 1) { old_col = VGA_WIDTH; old_row--; }
                    else old_col--;
                } while(--diff);
                move_cur(old_row  , old_col  );
                break;

            case '\e':
                /*
                 * NOTE: this case must be the case immediately preceding the 
                 *       newline/carriage return case, otherwise it won't work
                 *       when the user presses Enter to exit the vi-editing mode.
                 */
                if(option_set('y')) c = vi_cmode();
                if(c != '\n' && c != '\r') break;
                /*
                 * NOTE: fall throught to the next case.
                 */
                
            /*********************************** 
             * newline char, check for commands
             ***********************************/
            case '\n':
            case '\r':
                printf("\r\n");
                /* perform history expansion on the line */
                if(in_heredoc < 0 && option_set('H') && (p = hist_expand(quotes)))
                {
                    if(p == INVALID_HIST_EXPAND)
                    {
                        /* failed hist expands are reloaded for editing (bash) */
                        if(!optionx_set(OPTION_HIST_RE_EDIT))
                        {
                            if(incomplete_cmd) free(incomplete_cmd);
                            incomplete_cmd = NULL;
                            cmdbuf_index = 0;
                            cmdbuf_end   = 0;
                            cmdbuf[0] = '\0';
                            print_prompt();
                            update_row_col();
                            start_col = get_terminal_col();
                            start_row = get_terminal_row();
                            continue;
                        }
                    }
                    else if(p)
                    {
                        /* do not pass expanded line to the shell yet (bash) */
                        if(optionx_set(OPTION_HIST_VERIFY))
                        {
                            clear_cmd(0);
                            strcpy(cmdbuf, p);
                            free_malloced_str(p);
                            move_cur(start_row, start_col);
                            output_cmd();
                            cmdbuf_end = strlen(cmdbuf);
                            cmdbuf_index = cmdbuf_end;
                            break;
                        }
                        else
                        {
                            strcpy(cmdbuf, p);
                            free(p);
                            output_cmd();
                            printf("\r\n");
                            cmdbuf_end = strlen(cmdbuf);
                            if(cmdbuf[cmdbuf_end-1] == '\n')
                            {
                                cmdbuf_end--;
                            }
                            cmdbuf_index = cmdbuf_end;
                        }
                    }
                }
                
                cmdbuf[cmdbuf_end  ] = '\n';
                cmdbuf[cmdbuf_end+1] = '\0';
                if(is_incomplete_cmd(incomplete_cmd ? 0 : 1))
                {
                    print_prompt2();
                    char *tmp = (char *)0;
                    size_t sz = strlen(cmdbuf)+1;
                    if(incomplete_cmd) sz += strlen(incomplete_cmd);
                    tmp = (char *)malloc(sz);
                    if(!tmp) goto _process;
                    if(incomplete_cmd)
                    {
                        strcpy(tmp, incomplete_cmd);
                        free(incomplete_cmd);
                    }
                    else tmp[0] = '\0';
                    incomplete_cmd = tmp;
                    strcat(incomplete_cmd, cmdbuf);
                    cmdbuf_end = 0;
                    cmdbuf[cmdbuf_end] = '\0';
                    cmdbuf_index = cmdbuf_end;
                    update_row_col();
                    start_col = get_terminal_col();
                    start_row = get_terminal_row();
                    continue;
                }
_process:
                cmdbuf_end   = glue_cmd_pieces();
                in_heredoc   = -1;
                if(!heredocs) return cmdbuf;
                while(heredocs--)
                {
                    if(heredoc_mark[heredocs])
                    {
                        free(heredoc_mark[heredocs]);
                        heredoc_mark[heredocs] = 0;
                    }
                }
                return cmdbuf;
                
        } /* end switch */
    } /* end while */
}


char *has_keyword(char **__cmd, char *word)
{
    char *p;
    char *cmd = *__cmd;
    if(!(p = strstr(cmd, word)))
    {
        if(incomplete_cmd)
        {
            cmd = cmdbuf;
            if(!(p = strstr(cmd, word))) return 0;
        }
        else return 0;
    }
    /* check it's really a keyword */
    if(p != cmd)
    {
        int len = strlen(word);
        if(p[len] && !isspace(p[len])) return 0;
    }
    else
    {
        if(!isspace(p[-1])) return 0;
    }
    *__cmd = cmd;
    return p;
}


/* after user presses ENTER, check if he entered a full command or not */
int is_incomplete_cmd(int first_time)
{
    char *cmd = cmdbuf;
    size_t cmd_len = strlen(cmd);
    size_t i = 0;
    if(in_heredoc >= 0)
    {
        /* if no here-document mark is present, just wait for EOF by the user */
        if(!heredoc_mark[in_heredoc]) return 1;
        cmd[cmd_len-1] = '\0';
        char *nl = strrchr(cmd, '\n');
        cmd[cmd_len-1] = '\n';
        if(!nl)
        {
            /* nothing on the line to compare */
            if(cmd_len == 1) return 1;
            i = 0;
        }
        else i =  nl - cmd + 1;
        /* heredoc delimiter match */
        if(strcmp(cmd+i, heredoc_mark[in_heredoc]) == 0)
        {
            /* move on to the next heredoc, if any */
            if(++in_heredoc < heredocs) return 1;
            in_heredoc = -1;
            return 0;
        }
        else return 1;
    }
    if(first_time)
    {
        open_cb = close_cb = 0;
        open_rb = close_rb = 0;
        quotes  = 0;
        __heredocs = 0;
    }
    if(cmd_len >= 2)
    {
        if(cmd[cmd_len-2] == '\\' && cmd[cmd_len-1] == '\n')
        {
            return 1;
        }
    }
    do
    {
        switch(cmd[i])
        {
            case '{':
                open_cb++ ; break;
                
            case '(':
                open_rb++ ; break;
                
            case '}':
                close_cb++; break;
                
            case ')':
                close_rb++; break;
                
            case '\'':
            case '"':
            case '`':
                if((i != 0) && (cmd[i-1] == '\\')) continue;
                if(quotes)
                {
                    if(cmd[i] == quotes) quotes = 0;
                }
                else
                {
                    quotes = cmd[i];
                }
                break;

            case '<':                
                if(cmd[i+1] == '<')
                {
                    i += 2;
                    /* <<< introduces here-string (non-POSIX extension) */
                    if(i < cmd_len && cmd[i] == '<') break;
                    if(in_heredoc == -1) in_heredoc = 0;
                    /* here-document redirection operators are '<<' and '<<-', according to POSIX */
                    if(i < cmd_len && cmd[i] == '-')
                    {
                        i++;
                    }
                    if(cmd[i] != '\0' && !isspace(cmd[i]))
                    {
                        if(!heredoc_mark[__heredocs])
                        {
                            size_t j = i;
                            while(cmd[j] && !isspace(cmd[j]) && cmd[j] != ';' && cmd[j] != '&')
                                j++;
                            heredoc_mark[__heredocs] = (char *)malloc(j-i+1);
                            if(!heredoc_mark[__heredocs])
                            {
                                return 0;     /* TODO: output a decent error message and bail out */
                            }
                            strncpy(heredoc_mark[__heredocs], cmd+i, j-i);
                            heredoc_mark[__heredocs][j-i  ] = '\n';
                            heredoc_mark[__heredocs][j-i+1] = '\0';
                        }
                    }
                    heredocs++;
                    __heredocs++;
                }
                break;
        }
    } while(++i < cmd_len);

    if(quotes) return 1;
    if(open_cb && (open_cb != close_cb)) return 1;
    if(open_rb && (open_rb != close_rb)) return 1;
    if(in_heredoc >= 0) return 1;
    
    /* quick and dirty check for some important constructs (for, until, while loops, ...) */
    cmd = incomplete_cmd ? incomplete_cmd : cmdbuf;
    char *p;
    if((p = has_keyword(&cmd, "for")) || (p = has_keyword(&cmd, "while")) || (p = has_keyword(&cmd, "until")))
    {
        cmd = p;
        if(!(p = has_keyword(&cmd, "done"))) return 1;
    }
    if((p = has_keyword(&cmd, "if")))
    {
        cmd = p;
        if(!(p = has_keyword(&cmd, "fi"))) return 1;
    }
    if((p = has_keyword(&cmd, "case")))
    {
        cmd = p;
        if(!(p = has_keyword(&cmd, "esac"))) return 1;
    }
    return 0;
}

size_t glue_cmd_pieces()
{
    size_t cmdbuf_len     = strlen(cmdbuf);
    if(!incomplete_cmd) return cmdbuf_len;
    size_t incomplete_len = strlen(incomplete_cmd);
    char tmp[cmdbuf_len+1];
    strcpy(tmp, cmdbuf);
    if((cmdbuf_len+incomplete_len) >= cmdbuf_size)
    {
        /* TODO: not enough memory. we should react better to this error */
        if(!ext_cmdbuf(strlen(incomplete_cmd)+1)) return 0;
    }
    strcpy(cmdbuf, incomplete_cmd);
    strcat(cmdbuf, tmp);
    free(incomplete_cmd);
    incomplete_cmd = NULL;
    return cmdbuf_len+incomplete_len;
}
