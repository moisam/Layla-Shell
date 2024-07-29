/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "include/cmd.h"
#include "include/vi.h"
#include "include/kbdevent.h"
#include "include/utf.h"
#include "scanner/scanner.h"
#include "scanner/source.h"
#include "parser/parser.h"
#include "parser/node.h"
#include "backend/backend.h"
#include "builtins/builtins.h"
#include "builtins/setx.h"
#include "include/debug.h"


/* we will save incomplete commands here until being processed */
char     *incomplete_cmd = NULL;

/* our main command line buffer */
char     *cmdbuf         = NULL;        /* ptr to buffer */
size_t    cmdbuf_index   = 0;           /* whrere to add incoming key */
size_t    cmdbuf_end     = 0;           /* index of last entered key */
size_t    cmdbuf_size    = 0;           /* actual malloc'd size` */
size_t    CMD_BUF_SIZE   = 0;           /* size to alloc and extend the buffer by */

/* flag to tell us if we are inside a here-document */
int       in_heredoc     = -1;

/* assume heredocs for every possible file descriptor */
char     *heredoc_mark [FOPEN_MAX] = { 0, };

/* how many heredocs are expected */
int       heredocs       = 0;

/* screen size and cursor position */
size_t    terminal_row   = 0;
size_t    terminal_col   = 0;
size_t    VGA_WIDTH      = 80;
size_t    VGA_HEIGHT     = 25;
size_t    start_row      = 0;
size_t    start_col      = 0;

/* flag to indicate if INS was pressed */
int       insert         = 0;

/* 
 * flag to indicate if a SIGALRM signal was received as a result of the user
 * setting the $TPERIOD shell variable (which works similar to tcsh's tperiod).
 */
int       do_periodic    = 0;
timer_t   timerid        = 0;          /* id of the timer used to send SIGALRM */

/* max EOFs before we exit */
#define MAX_EOFS        10

/* variables used by is_incomplete_cmd() function */
static int open_cb    = 0, close_cb = 0;   /* count curly braces */
static int open_rb    = 0, close_rb = 0;   /* count round brackets */
static int quotes     = 0;                 /* count quotes */
static int __heredocs = 0;                 /* count heredocs */


/*
 * Kill input by emptying the command buffer, printing a newline followed by
 * the first prompt, and updating the cursor position.
 */
void kill_input(void)
{
    cmdbuf_index = 0;
    cmdbuf_end   = 0;
    fprintf(stderr, "\n");
    print_prompt();
    cmdbuf[0]    = '\0';
    update_row_col();
    start_row    = get_terminal_row();
    start_col    = get_terminal_col();

    if(incomplete_cmd)
    {
        free(incomplete_cmd);
        incomplete_cmd = NULL;
    }

    while(heredocs > 0)
    {
        heredocs--;
        if(heredoc_mark[heredocs])
        {
            free(heredoc_mark[heredocs]);
            heredoc_mark[heredocs] = 0;
        }
    }
}

/*
 * Main interactive shell REPL (Read-Execute-Print-Loop).
 */
void cmdline(void)
{
    char *cmd;
    //int  stat = EXIT_SUCCESS;

    /* clear the screen */
    if(optionx_set(OPTION_CLEAR_SCREEN))
    {
        clear_screen();
    }

    /* print welcome message */
    fprintf(stdout, "\n\nWelcome to Layla shell\n\n");

    /* clear heredoc marks */
    int i;
    for(i = 0; i < FOPEN_MAX; i++)
    {
        heredoc_mark[i] = NULL;
    }


    /* prepare our source struct */
    struct source_s src;
    src.buffer   = NULL;
    src.bufsize  = 0;
    src.srctype  = SOURCE_STDIN;
    src.srcname  = NULL;
    src.curpos   = INIT_SRC_POS;

    /* REPL loop */
    do
    {
        cmd_history_index = cmd_history_end;

        /* check on child processes before printing the next $PS1 */
        if(option_set('m'))
        {
            check_on_children();
        }

        /* check for mail */
        if(check_for_mail())
        {
            do_builtin_internal(mailcheck_builtin, 2, (char *[]){"mail", "-q", NULL});
        }

        /* in tcsh, special alias periodic is run every tperiod minutes */
        if(do_periodic)
        {
            run_alias_cmd("periodic");
            do_periodic = 0;
        }

        /* in tcsh, special alias precmd is run before printing the next command prompt */
        run_alias_cmd("precmd");

        /* print the first prompt $PS1 */
        print_prompt();

        /* check if we're ready to read from the terminal */
        int tty = cur_tty_fd();
        struct timeval timeout = { 0, 0 };
        if(get_secs_usecs(get_shell_varp("TMOUT", NULL), &timeout) && 
           !ready_to_read(tty, &timeout))
        {
            break;
        }

        /*
         * turn canonical mode off (might have been turned on by the last command
         * we executed).
         */
        term_canon(0);

        /* read the next command line */
        cmd = read_cmd();

        /* no input (EOF) */
        if(!cmd)
        {
            /* accept EOF only if 'ignoreeof' option is not set */
            if(option_set('o'))
            {
                /* tcsh outputs a message in this case */
                fprintf(stderr, "Use \"exit\" to leave\n");
                clearerr(stdin);
                continue;
            }

            /* try to exit (this will execute any EXIT traps) */
            do_builtin_internal(exit_builtin, 1, (char *[]){ "exit", NULL });
            
            /* if we return from exit_builtin(), it means we have pending jobs */
            continue;
        }

        /* we've got an empty line */
        if(cmd[0] == '\0' || strcmp(cmd, "\n") == 0)
        {
            continue;
        }

        /* fill in the source struct */
        src.buffer   = cmd;
        src.bufsize  = cmdbuf_end;
        src.curpos   = INIT_SRC_POS;

        /* and execute the command line */
        parse_and_execute(&src);
    } while(1);
}


/*
 * Initialize the command buffer for first use.
 */
void init_cmdbuf(void)
{
    cmdbuf_index = 0;
    cmdbuf_end   = 0;
    
    /* first call. init buffer */
    if(!cmdbuf)
    {
        CMD_BUF_SIZE = sysconf(_SC_LINE_MAX);
        if(CMD_BUF_SIZE <= 0)
        {
            CMD_BUF_SIZE = DEFAULT_LINE_MAX;
        }
        
        cmdbuf = malloc(CMD_BUF_SIZE);
        if(!cmdbuf)
        {
            exit_gracefully(EXIT_FAILURE, "FATAL ERROR: Insufficient memory for command buffer");
        }
        
        cmdbuf_size  = CMD_BUF_SIZE;
    }
    
    /* empty the buffer */
    cmdbuf[0] = '\0';
}


/*
 * Extend the command buffer by the given amount of bytes.
 *
 * Returns 1 if the buffer is successfully extended, 0 otherwise.
 */
int ext_cmdbuf(char **cmdbuf, size_t *size, size_t howmuch)
{
    size_t newsz = (*size) + howmuch;
    char *newbuf = realloc(cmdbuf, newsz);
    
    if(!newbuf)
    {
        return 0;
    }
    
    (*cmdbuf) = newbuf;
    (*size)   = newsz;

    return 1;
}


/*
 * Read the next command line into our command buffer.
 *
 * Returns a pointer to the buffer if the command is read successfully,
 * NULL if there's no input or EOF is reached.
 */
char *read_cmd(void)
{
    char *p;
    int c;
    int tty = cur_tty_fd();
    
    init_cmdbuf();

    /* update cursor position */
    update_row_col();
    
    start_row = get_terminal_row();
    start_col = get_terminal_col();

    /*
     * Get the number of consecutive EOFs to force exit. Default is 10 (bash)
     * or 1 (tcsh). We obtain the value manually, instead of calling 
     * get_shell_vari(), because if the variable is defined, but its value is
     * invalid, we'll use our default MAX_EOFS value. If the variable isn't
     * defined, we'll use 0.
     */
    struct symtab_entry_s *entry = get_symtab_entry("IGNOREEOF");
    int eofs = 0, max_eofs = 0;
    if(entry)
    {
        if(entry->val)
        {
            char *strend = NULL;
            max_eofs = strtol(entry->val, &strend, 10);
            if(max_eofs < 0 || *strend)
            {
                max_eofs = MAX_EOFS;
            }
        }
        else
        {
            max_eofs = MAX_EOFS;
        }
    }
    
    /* tcsh has an 'inputmode' variable that determins the input mode (insert/overwrite) */
    char *inputm = get_shell_varp("INSERTMODE", "insert");
    if(inputm)
    {
        if(strcmp(inputm, "overwrite") == 0)
        {
            insert = 1;
        }
        else
        {
            insert = 0;
        }
    }
    
    /* loop to read user input */
    while(1)
    {
        /*
         * Check if we received a signal for which we have a trap set.
         * If so, clear the input buffer and reprint the prompt string.
         */
        if(signal_received)
        {
            kill_input();
            signal_received = 0;
            continue;
        }
        
        /*
         * This flag tells us whether we've received an EOF (CTRL-D) with a
         * non-empty buffer, in which case we'll consider the command line complete,
         * without actually calling is_incomplete_cmd() to do the check.
         */
        int force_eof = 0;

        /***********************
         * get next key stroke
         ***********************/
        c = get_next_key(tty);

        /* EOF key */
        if(c == EOF_KEY)
        {
            /* if we received CTRL-D with cmd in buffer, finish the command */
            if(incomplete_cmd)
            {
                c = '\n';
                /* indicate we shouldn't call is_incomplete_cmd() on this input */
                force_eof = 1;
            }
            else
            {
                /* if the buffer is empty, count the number of EOFs we received */
                if(cmdbuf_index == 0)
                {
                    /* if EOFs reached the threshold, return NULL */
                    if(++eofs >= max_eofs)
                    {
                        return NULL;
                    }
                }
                /* otherwise, beep in error (bash) */
                beep();
                continue;
            }
        }

        /* invalid key */
        if(c == 0)
        {
            continue;
        }
        
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
                else if(c == VLNEXT_KEY)
                {
                    c = get_next_key(tty);
                    do_insert(c);
                }
                else if(is_utf8(c))
                {
                    do_insert(c);
                    int i = trailing_utf8_bytes[(unsigned int)(unsigned char)c];
                    while(i--)
                    {
                        do_insert(get_next_key(tty));
                    }
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
                //update_row_col();
                if(CTRL_MASK && cmdbuf_index)
                {
                    int i = cmdbuf_index;
                    if(isspace(cmdbuf[i-1]))
                    {
                        i--;
                    }

                    if(isspace(cmdbuf[i]) || cmdbuf[i] == '\0')
                    {
                        while(i &&  isspace(cmdbuf[i]))
                        {
                            i--;    /* skip trailing spaces */
                        }
                        
                        while(i && !isspace(cmdbuf[i]))
                        {
                            i--;    /* skip prev word */
                        }
                        
                        if(isspace(cmdbuf[i]))
                        {
                            i++;
                        }
                    }
                    else
                    {
                        while(i && !isspace(cmdbuf[i]))
                        {
                            i--;    /* skip prev word */
                        }
                        
                        while(i &&  isspace(cmdbuf[i]))
                        {
                            i--;    /* skip trailing spaces */
                        }
                        
                        if(isspace(cmdbuf[i]))
                        {
                            i++;
                        }
                    }
                    do_left_key(cmdbuf_index-i);
                }
                else
                {
                    do_left_key(1);
                }
                break;

            case RIGHT_KEY:
                //update_row_col();
                if(CTRL_MASK && cmdbuf_index < cmdbuf_end)
                {
                    size_t i = cmdbuf_index;
                    if(isspace(cmdbuf[i]))
                    {
                        while(i < cmdbuf_end &&  isspace(cmdbuf[i]))
                        {
                            i++;    /* skip leading spaces */
                        }
                        
                        while(i < cmdbuf_end && !isspace(cmdbuf[i]))
                        {
                            i++;    /* skip next word */
                        }
                    }
                    else
                    {
                        while(i < cmdbuf_end && !isspace(cmdbuf[i]))
                        {
                            i++;    /* skip next word */
                        }
                    }
                    do_right_key(i-cmdbuf_index);
                }
                else
                {
                    do_right_key(1);
                }
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
                /* this procedure will do command auto-completion */
                do_tab(cmdbuf, &cmdbuf_index, &cmdbuf_end);
                break;

            case CTRLW_KEY:
                if(cmdbuf_index == 0)
                {
                    continue;
                }
                
                size_t z = cmdbuf_index-1;
                while(z)
                {
                    if(cmdbuf[z] == ' ' || ispunct(cmdbuf[z]))
                    {
                        z++;
                        break;
                    }
                    
                    if(z == 0)
                    {
                        break;
                    }
                    z--;
                }
                
                if(z == cmdbuf_index)
                {
                    continue;
                }
                update_row_col();
                
                size_t old_row = terminal_row, old_col = terminal_col;
                move_cur(start_row, start_col);
                printf("%*s", (int)cmdbuf_end, " ");
                char *p1 = cmdbuf+z;
                char *p2 = cmdbuf+cmdbuf_index;
                while((*p1++ = *p2++))
                {
                    ;
                }
                
                int  diff = cmdbuf_index-z;
                cmdbuf_index -= diff;
                cmdbuf_end   -= diff;
                move_cur(start_row, start_col);
                printf("%s", cmdbuf);
                
                do
                {
                    if(old_col == 1)
                    {
                        old_col = VGA_WIDTH;
                        old_row--;
                    }
                    else
                    {
                        old_col--;
                    }
                } while(--diff);
                move_cur(old_row  , old_col);
                break;

            case '\e':
                /*
                 * NOTE: This case must be the case immediately preceding the 
                 *       newline/carriage return case, otherwise it won't work
                 *       when the user presses Enter to exit the vi-editing mode.
                 */
                if(option_set('y'))
                {
                    c = vi_cmode();
                }
                
                if(c != '\n' && c != '\r')
                {
                    break;
                }
                
                /* NOTE: fall through to the next case */
                __attribute__((fallthrough));
                
            /*********************************** 
             * newline char, check for commands
             ***********************************/
            case '\n':
            case '\r':
                printf("\n");
                /* perform history expansion on the line */
                if(in_heredoc < 0 && option_set('H') &&
                    (p = hist_expand(quotes, FLAG_HISTEXPAND_DO_BACKUP)))
                {
                    if(p == INVALID_HIST_EXPAND)
                    {
                        /* failed hist expands are reloaded for editing (bash) */
                        if(!optionx_set(OPTION_HIST_RE_EDIT))
                        {
                            if(incomplete_cmd)
                            {
                                free(incomplete_cmd);
                            }
                            
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
                            free_malloced_str(p);
                            output_cmd();
                            printf("\n");
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
                /* did we receive CTRL-D on this line? */
                if(force_eof)
                {
                    c = 0;
                }
                else
                {
                    c = is_incomplete_cmd(incomplete_cmd ? 0 : 1);
                }

                /* further action depends on whether the command is complete or not */
                if(c < 0)           /* error parsing command line */
                {
                    kill_input();
                    break;
                }
                else if(c > 0)      /* incomplete command in the buffer */
                {
                    print_prompt2();
                    char *tmp = NULL;
                    size_t sz = strlen(cmdbuf)+1;

                    if(incomplete_cmd)
                    {
                        sz += strlen(incomplete_cmd);
                    }

                    tmp = malloc(sz);
                    if(tmp)
                    {
                        if(incomplete_cmd)
                        {
                            strcpy(tmp, incomplete_cmd);
                            free(incomplete_cmd);
                        }
                        else
                        {
                            tmp[0] = '\0';
                        }
                        
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
                }

                cmdbuf_end = glue_cmd_pieces();
                in_heredoc = -1;
                if(!heredocs)
                {
                    return cmdbuf;
                }
                
                while(heredocs > 0)
                {
                    heredocs--;
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


/*
 * After user presses ENTER, check if he entered a full command or not.
 *
 * Returns 1 if the command is incomplete, 0 if it is complete.
 */
int is_incomplete_cmd(int first_time)
{
    char   *cmd = cmdbuf;
    size_t  cmd_len = strlen(cmd);
    size_t  i = 0, j;

    /*
     * if we are inside a here-document, check if the last entered line matches
     * the here-document marker word, which signals the end of the here-document.
     */
    if(in_heredoc >= 0)
    {
        /* if no here-document mark is present, just wait for EOF by the user */
        cmd[cmd_len-1] = '\0';
        char *nl = strrchr(cmd, '\n');
        cmd[cmd_len-1] = '\n';
        if(!nl)
        {
            /* nothing on the line to compare to */
            if(cmd_len == 1)
            {
                return 1;
            }
            i = 0;
        }
        else
        {
            i =  nl - cmd + 1;
        }
        /* heredoc delimiter match */
        if(strcmp(cmd+i, heredoc_mark[in_heredoc]) == 0)
        {
            /* move on to the next heredoc, if any */
            if(++in_heredoc < heredocs)
            {
                return 1;
            }
            in_heredoc = -1;
            return 0;
        }
        else
        {
            return 1;
        }
    }

    if(first_time)
    {
        open_cb    = 0;
        close_cb   = 0;
        open_rb    = 0;
        close_rb   = 0;
        quotes     = 0;
        __heredocs = 0;
    }

    if(cmd_len >= 2)
    {
        if(cmd[cmd_len-2] == '\\' && cmd[cmd_len-1] == '\n')
        {
            /* check if the '\' is escaped itself */
            char *p2 = cmd+cmd_len-2;
            char *p1 = p2;
            while(p1 >= cmd && *p1 == '\\')
            {
                p1--;
            }
            /*
             * Check how many consecutive backslashes we have. If we have
             * an even number, the backslashes escape each other and the
             * newline is not escaped, so we return the input line.
             * If we have an odd number, the last backslash escapes the
             * newline character and so we continue reading input.
             */
            if((p2-p1) % 2)
            {
                return 1;
            }
        }
    }

    int endme = 0;
    
    do
    {
        switch(cmd[i])
        {
            case '{':
                if(quotes == '\'')
                {
                    break;
                }
                
                open_cb++;
                j = find_closing_brace(cmd+i, (quotes == '"') ? 1 : 0);
                if(j == 0)  /* closing brace not found */
                {
                    endme = 1;
                }
                else
                {
                    close_cb++;
                }
                break;
                
            case '(':
                if(quotes == '\'')
                {
                    break;
                }
                
                open_rb++ ;
                j = find_closing_brace(cmd+i, (quotes == '"') ? 1 : 0);
                if(j == 0)  /* closing brace not found */
                {
                    endme = 1;
                }
                else
                {
                    close_rb++;
                }
                break;
                
            case '\'':
                /* single quotes has no effect inside double quotes */
                if(quotes == '"')
                {
                    break;
                }
                /* end of a partially started single-quoted string */
                else if(quotes == '\'')
                {
                    quotes = 0;
                    break;
                }
                
                while(++i < cmd_len)
                {
                    if(cmd[i] == '\'')
                    {
                        break;
                    }
                }
                
                /* closing quote not found */
                if(cmd[i] != '\'')
                {
                    endme = 1;
                    quotes = '\'';
                }
                break;
                
            case '\\':
                /* backslash has no effect inside single quotes */
                if(quotes != '\'')
                {
                    i++;
                }
                break;
                
            case '"':
            case '`':
                /* double and back quotes have no effect inside single quotes */
                if(quotes == '\'')
                {
                    break;
                }
                /* end of a partially started quoted string */
                else if(quotes == cmd[i])
                {
                    quotes = 0;
                    break;
                }

                j = find_closing_quote(cmd+i, (quotes == '"') ? 1 : 0, 0);
                if(j == 0)  /* closing quote not found */
                {
                    endme = 1;
                }
                else
                {
                    quotes = cmd[i];
                }
                break;

            case '<':
                if(quotes)
                {
                    break;
                }

                if(cmd[i+1] == '<')
                {
                    i += 2;
                    /* <<< introduces here-strings (non-POSIX extension) */
                    if(i < cmd_len && cmd[i] == '<')
                    {
                        break;
                    }
                    
                    if(in_heredoc == -1)
                    {
                        in_heredoc = 0;
                    }
                    
                    /* here-document redirection operators are '<<' and '<<-', according to POSIX */
                    if(i < cmd_len && cmd[i] == '-')
                    {
                        i++;
                    }
                    
                    /*
                     * Strictly speaking, POSIX says the heredoc word should come directly
                     * after the '<<' or '<<-' operator, with no intervening spaces.
                     * As most users will eventually enter a space between the word and
                     * the operator for readability, we'll accept this behavior in here.
                     */
                    while(isspace(cmd[i]))
                    {
                        i++;
                    }
                    
                    if(cmd[i] == '\0')
                    {
                        PRINT_ERROR(SHELL_NAME, "missing heredoc delimiter word after << or <<-");
                        return -1;
                    }
                    
                    if(!heredoc_mark[__heredocs])
                    {
                        size_t j = i;
                        while(cmd[j] && !isspace(cmd[j]) && cmd[j] != ';' && cmd[j] != '&')
                        {
                            j++;
                        }
                        heredoc_mark[__heredocs] = malloc(j-i+1);
                        
                        if(!heredoc_mark[__heredocs])
                        {
                            INSUFFICIENT_MEMORY_ERROR(SHELL_NAME, "save heredoc delimiter word");
                            return -1;
                        }
                        
                        strncpy(heredoc_mark[__heredocs], cmd+i, j-i);
                        heredoc_mark[__heredocs][j-i  ] = '\n';
                        heredoc_mark[__heredocs][j-i+1] = '\0';
                    }
                    heredocs++;
                    __heredocs++;
                }
                break;
        }
        
        if(endme)
        {
            break;
        }
    } while(++i < cmd_len);

    if(quotes)
    {
        return 1;
    }
    if(open_cb && (open_cb != close_cb))
    {
        return 1;
    }
    if(open_rb && (open_rb != close_rb))
    {
        return 1;
    }
    if(in_heredoc >= 0)
    {
        return 1;
    }
    
    /* quick and dirty check for some important constructs (for, until, while loops, ...) */
    cmd = incomplete_cmd ? incomplete_cmd : cmdbuf;
    /* count the number of keywords we have on the line */
    int dos = 0, ifs = 0, cases = 0;
    int dones = 0, fis = 0, esacs = 0;
    int loops = 0;
    /* pointer to one of the above indices */
    int *index = NULL;
    /* flag to tell us if a keyword starts after a separator operator */
    int op = 1;
    /* pointer to the end of a keyword in input */
    char *iend = NULL;
    char *p, *p2;

check:
    p = cmd;
    while(*p)
    {
        switch(*p)
        {
            case ' ':
            case '\t':
                p++;
                break;

            case '\r':
            case '\n':
            case '&':
            case ';':
            case '|':
                /* remember we have a separator operator, after which we can have a keyword */
                op = 1;
                p++;
                break;

            case '"':
            case '\'':
            case '`':
                /* skip quoted strings */
                p2 = p+1;
                while(*p2 && *p2 != *p)
                {
                    p2++;
                }
                /* skip the closing quote */
                if(*p2 == *p)
                {
                    p2++;
                }
                op = 0;
                p = p2;
                break;

            default:
                /* check for keywords */
                if(isalpha(*p) && op)
                {
                    if(strncmp(p, "done", 4) == 0)
                    {
                        index = &dones;
                        iend = p+4;
                    }
                    else if(strncmp(p, "do", 2) == 0)
                    {
                        index = &dos;
                        iend = p+2;
                    }
                    else if(strncmp(p, "if", 2) == 0)
                    {
                        index = &ifs;
                        iend = p+2;
                    }
                    else if(strncmp(p, "fi", 2) == 0)
                    {
                        index = &fis;
                        iend = p+2;
                    }
                    else if(strncmp(p, "case", 4) == 0)
                    {
                        index = &cases;
                        iend = p+4;
                    }
                    else if(strncmp(p, "esac", 4) == 0)
                    {
                        index = &esacs;
                        iend = p+4;
                    }
                    else if(strncmp(p, "for", 3) == 0)
                    {
                        index = &loops;
                        iend = p+3;
                    }
                    else if(strncmp(p, "while", 5) == 0 || strncmp(p, "until", 5) == 0)
                    {
                        index = &loops;
                        iend = p+5;
                    }
                    else
                    {
                        index = NULL;
                    }
                }
                else
                {
                    index = NULL;
                }
                
                /*
                 *  check if we've got a keyword (must be preceded by a separator
                 *  operator or start of line).
                 */
                if(index)
                {
                    if(op)
                    {
                        /* make sure it is followed by a separator operator */
                        if(!*iend || isspace(*iend) || *iend == '&' || *iend == ';' || *iend == '|')
                        {
                            (*index) += 1;
                        }
                    }
                    p = iend;
                }
                else
                {
                    p++;
                }
                op = 0;
                break;
        }
    }
    /* check the rest of the line */
    if(cmd == incomplete_cmd)
    {
        cmd = cmdbuf;
        goto check;
    }
    /* check we don't have a for, while or until without a do-done group */
    if(loops && (!dos || !dones))
    {
        return 1;
    }
    /* check we have a balanced number of keywords (which can be zero) */
    if((dos != dones) || (ifs != fis) || (cases != esacs))
    {
        return 1;
    }
    return 0;
}


/*
 * Form the complete command line by amalgamating all lines together.
 *
 * Returns the total length of the command lines.
 */
size_t glue_cmd_pieces(void)
{
    size_t cmdbuf_len = strlen(cmdbuf);
    if(!incomplete_cmd)
    {
        return cmdbuf_len;
    }
    size_t incomplete_len = strlen(incomplete_cmd);
    char tmp[cmdbuf_len+1];
    strcpy(tmp, cmdbuf);
    if((cmdbuf_len+incomplete_len) >= cmdbuf_size)
    {
        /* TODO: not enough memory. we should react better to this error */
        if(!ext_cmdbuf(&cmdbuf, &cmdbuf_size, incomplete_len+1))
        {
            return 0;
        }
    }
    strcpy(cmdbuf, incomplete_cmd);
    strcat(cmdbuf, tmp);
    free(incomplete_cmd);
    incomplete_cmd = NULL;
    return cmdbuf_len+incomplete_len;
}
