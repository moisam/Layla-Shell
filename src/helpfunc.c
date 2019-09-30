/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: helpfunc.c
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

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <linux/kd.h>
#include "cmd.h"
#include "backend/backend.h"
#include "debug.h"
#include "kbdevent.h"

/********************************************************************
 * 
 * This file contains helper functions for other parts of the shell.
 * 
 ********************************************************************/

/****************************************
 * 1 - Terminal control/status functions
 ****************************************/

/* declared in kbdevent2.c */
extern struct termios tty_attr_old;
extern struct termios tty_attr;


/*
 * turn the terminal canonical mode on or off.
 */
void term_canon(int on)
{
    if(!isatty(0))
    {
        return;
    }
    if(on)
    {
        tcsetattr(0, TCSANOW, &tty_attr_old);
    }
    else
    {
        tcsetattr(0, TCSANOW, &tty_attr);
    }
}

/* special struct termios for turning echo on/off */
struct termios echo_tty_attr;


/*
 * turn echo on for the given file descriptor.
 * 
 * returns 1 on success, 0 on failure.
 */
int echoon(int fd)
{
    /* check fd is a terminal */
    if(!isatty(fd))
    {
        return 0;
    }
    /* get the terminal attributes */
    if(tcgetattr(fd, &echo_tty_attr) == -1)
    {
        return 0;
    }
    /* turn echo on */
    echo_tty_attr.c_lflag |= ECHO;
    /* set the new terminal attributes */
    if((tcsetattr(fd, TCSAFLUSH, &echo_tty_attr) == -1))
    {
        return 0;
    }
    return 1;
}


/*
 * turn echo off for the given file descriptor.
 * 
 * returns 1 on success, 0 on failure.
 */
int echooff(int fd)
{
    /* check fd is a terminal */
    if(!isatty(fd))
    {
        return 0;
    }
    /* get the terminal attributes */
    if(tcgetattr(fd, &echo_tty_attr) == -1)
    {
        return 0;
    }
    /* turn echo off */
    echo_tty_attr.c_lflag &= ~ECHO;
    /* set the new terminal attributes */
    if((tcsetattr(fd, TCSAFLUSH, &echo_tty_attr) == -1))
    {
        return 0;
    }
    return 1;
}


/*
 * get the screen size and save the width and height in the $COLUMNS and $LINES
 * shell variables, respectively.
 * 
 * returns 1 if the screen size if obtained and saved, 0 otherwise.
 */
int get_screen_size()
{
    struct winsize w;
    /* find the size of the terminal window */
    int fd = isatty(0) ? 0 : isatty(2) ? 2 : -1;
    if(fd == -1)
    {
        return 0;
    }
    int res = ioctl(fd, TIOCGWINSZ, &w);
    if(res != 0)
    {
        return 0;
    }
    VGA_HEIGHT = w.ws_row;
    VGA_WIDTH  = w.ws_col;
    /* update the value of terminal columns in environ and in the symbol table */
    char buf[32];
    struct symtab_entry_s *e = get_symtab_entry("COLUMNS");
    if(!e)
    {
        e = add_to_symtab("COLUMNS");
    }
    if(e && sprintf(buf, "%d", VGA_WIDTH))
    {
        symtab_entry_setval(e, buf);
    }
    /* update the value of terminal rows in environ and in the symbol table */
    e = get_symtab_entry("LINES");
    if(!e)
    {
        e = add_to_symtab("LINES");
    }
    if(e && sprintf(buf, "%d", VGA_HEIGHT))
    {
        symtab_entry_setval(e, buf);
    }
    return 1;
}


/*
 * move the cursor to the given row (line) and column.. both values are 1-based,
 * counting from the top-left corner of the screen.
 */
void move_cur(int row, int col)
{
    fprintf(stdout, "\e[%d;%dH", row, col);
    fflush(stdout);
}


/*
 * clear the screen.
 */
void clear_screen()
{
    fprintf(stdout, "\e[2J");
    fprintf(stdout, "\e[0m");
    fprintf(stdout, "\e[3J\e[1;1H");
}


/*
 * set the text foreground and background color.
 */
void set_terminal_color(int FG, int BG)
{
    /*control sequence to set screen color */
    fprintf(stdout, "\x1b[%d;%dm", FG, BG);
}


/*
 * get the cursor position (current row and column), which are 1-based numbers,
 * counting from the top-left corner of the screen.
 */
void update_row_col()
{
    /*
     * clear the terminal device's EOF flag.. this would have been set, for example,
     * if we used the read builtin to read from the terminal and the user pressed
     * CTRL-D to indicate end of input.. we can't read the cursor position without
     * clearing that flag.
     */
    if(feof(stdin))
    {
        clearerr(stdin);
    }
    /* 
     * we will temporarily block SIGCHLD so it won't disturb us between our cursor 
     * position request and the terminal's response.
     */
    sigset_t intmask;
    sigemptyset(&intmask);
    sigaddset(&intmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &intmask, NULL);
    /* request terminal cursor position report (CPR) */
    fprintf(stdout, "\x1b[6n");
    /* read the result. it will be reported as:
     *     ESC [ y; x R
     */
    terminal_col = 0;
    terminal_row = 0;
    int c;
    int *in = &terminal_row;
    int delim = ';';
    term_canon(0);
    while((c = getchar()) != EOF)
    {
        if(c == 27 )
        {
            continue;
        }
        if(c == '[')
        {
            continue;
        }
        *in = c-'0';
        while((c = getchar()) != delim)
        {
            *in = ((*in) * 10) + (c-'0');
        }
        if(delim == ';')
        {
            in = &terminal_col;
            delim = 'R';
        }
        else
        {
            break;
        }
    }
    /* get the trailing 'R' if its still there */
    while(c != EOF && c != 'R')
    {
        c = getchar();
    }
    debug ("row=%d, col=%d\n", terminal_row, terminal_col);
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);
}


/*
 * return the current row at which the cursor is positioned.. rows are 1-based,
 * counting from the top of the screen.
 */
int get_terminal_row()
{
    return terminal_row;
}


/*
 * move the cursor to the given row.. rows are 1-based, counting from the
 * top of the screen.
 * 
 * returns the new cursor row.
 */
int set_terminal_row(int row)
{
    int diff = row-terminal_row;
    if(diff < 0)
    {
        fprintf(stdout, "\x1b[%dA", -diff);
    }
    else
    {
        fprintf(stdout, "\x1b[%dB",  diff);
    }
    return row;
}


/*
 * return the current column at which the cursor is positioned.. columns are 1-based,
 * counting from the left side of the screen.
 */
int get_terminal_col()
{
    return terminal_col;
}


/*
 * move the cursor to the given column.. columns are 1-based, counting from the
 * left side of the screen.
 * 
 * returns the new cursor column.
 */
int set_terminal_col(int col)
{
    fprintf(stdout, "\x1b[%dG", col);
    return col;
}


/*
 * produce a beeping sound.
 * 
 * returns 1.
 */
int beep()
{
    /* in tcsh, special alias beepcmd is run when the shell wants to ring the bell */
    run_alias_cmd("beepcmd");
    putchar('\a');
    return 1;
}


/****************************************
 * 2 - String manipulation functions
 ****************************************/

/*
 * convert string str letters to uppercase.
 * 
 * returns 1 on success, 0 if a NULL str pointer is given.
 */
int strupper(char *str)
{
    if(!str)
    {
        return 0;
    }
    while(*str)
    {
        *str = toupper(*str);
        str++;
    }
    return 1;
}


/*
 * convert string str letters to lowercase.
 * 
 * returns 1 on success, 0 if a NULL str pointer is given.
 */
int strlower(char *str)
{
    if(!str)
    {
        return 0;
    }
    while(*str)
    {
        *str = tolower(*str);
        str++;
    }
    return 1;
}


const char *_itoa_digits = "0123456789";

/*
 * quick converstion from an integer to a string format.
 */
void _itoa(char *str, int num)
{
    size_t res = 1, i;
    uintmax_t copy = num;
    while(10 <= copy)
    {
        copy /= 10, res++;
    }
    str[res] = '\0';
    for(i = res; i != 0; i--)
    {
        str[i-1] = _itoa_digits[num % 10];
        num /= 10;
    }
}


/*
 * append character chr to string str at position pos (zero based).
 */
inline void strcat_c(char *str, int pos, char chr)
{
    str[pos++] = chr;
    str[pos  ] = '\0';
}


/*
 * remove all occurrences of '\\n' from the string in buf.
 */
size_t remove_escaped_newlines(char *buf)
{
    size_t len = strlen(buf);
    if(!len)
    {
        return 0;
    }
    size_t i = 0;
    while(i < len)
    {
        if(buf[i] == '\\' && buf[i+1] == '\n')
        {
            delete_char_at(buf, i);
        }
        else
        {
            i++;
        }
    }
    return len;
}

/*
 * search string for any one of the passed characters.
 * 
 * returns a char pointer to the first occurence of any of the characters,
 * NULL if none found.
 */
char *strchr_any(char *string, char *chars)
{
    if(!string || !chars)
    {
        return NULL;
    }
    char *s = string;
    while(*s)
    {
        char *c = chars;
        while(*c)
        {
            if(*s == *c)
            {
                return s;
            }
            c++;
        }
        s++;
    }
    return NULL;
}

/*
 * returns 1 if the two strings are the same, 0 otherwise.
 */
int is_same_str(char *s1, char *s2)
{
    if((strlen(s1) == strlen(s2)) && strcmp(s1, s2) == 0)
    {
        return 1;
    }
    return 0;
}

/*
 * return the passed string value, quoted in a format that can
 * be used for reinput to the shell.
 */
char *quote_val(char *val, int add_quotes)
{
    char *res = NULL;
    size_t len;
    /* empty string */
    if(!val || !*val)
    {
        len = add_quotes ? 3 : 1;
        res = malloc(len);
        if(!res)
        {
            return NULL;
        }
        strcpy(res, add_quotes ? "\"\"" : "");
        return res;
    }
    /* count the number of quotes needed */
    len = 0;
    char *v = val, *p;
    while(*v)
    {
        switch(*v)
        {
            case '\\':
            case  '`':
            case  '$':
            case  '"':
                len++;
                break;
        }
        v++;
    }
    len += strlen(val);
    /* add two for the opening and closing quotes (optional) */
    if(add_quotes)
    {
        len += 2;
    }
    /* alloc memory for quoted string */
    res = malloc(len+1);
    if(!res)
    {
        return NULL;
    }
    p = res;
    /* add opening quote (optional) */
    if(add_quotes)
    {
        *p++ = '"';
    }
    /* copy quoted val */
    v = val;
    while(*v)
    {
        switch(*v)
        {
            case '\\':
            case  '`':
            case  '$':
            case  '"':
                /* add '\' for quoting */
                *p++ = '\\';
                /* copy char */
                *p++ = *v++;
                break;

            default:
                /* copy next char */
                *p++ = *v++;
                break;
        }
    }
    /* add closing quote (optional) */
    if(add_quotes)
    {
        *p++ = '"';
    }
    *p = '\0';
    return res;
}

/*
 * convert a string array to a single string with members
 * separated by spaces.. the last member must be NULL, or else
 * we will loop until we SIGSEGV! a non-zero dofree argument
 * causes the passed list to be freed (if the caller is too 
 * lazy to free its own memory).
 */
char *list_to_str(char **list, int dofree)
{
    if(!list)
    {
        return NULL;
    }
    int i;
    int len = 0;
    int count = 0;
    char *p, *p2;
    /* get the count */
    for(i = 0; list[i]; i++)
    {
        ;
    }
    count = i;
    int lens[count];
    /* get total length and each item's length */
    for(i = 0; i < count; i++)
    {
        /* add 1 for the separating space char */
        lens[i] = strlen(list[i])+1;
        len += lens[i];
    }
    p = malloc(len+1);
    if(!p)
    {
        return NULL;
    }
    *p = 0;
    p2 = p;
    /* now copy the items */
    for(i = 0; i < count; i++)
    {
        sprintf(p, "%s ", list[i]);
        p += lens[i];
    }
    p[-1]= '\0';
    /* free the original list */
    if(dofree)
    {
        for(i = 0; i < count; i++)
        {
            free(list[i]);
        }
        free(list);
    }
    return p2;
}


/*
 * get the system-defined maximum line length.. if no value is defined by the
 * system, use our own default value.
 */
int get_linemax()
{
    int line_max;
#ifdef LINE_MAX
    line_max = LINE_MAX;
#else
    line_max = sysconf(_SC_LINE_MAX);
    if(line_max <= 0)
    {
        line_max = DEFAULT_LINE_MAX;
    }
#endif
    return line_max;
}


/*
 * alloc memory for, or extend the host (or user) names buffer if needed..
 * in the first call, the buffer is initialized to 32 entries.. subsequent
 * calls result in the buffer size doubling, so that it becomes 64, 128, ...
 * count is the number of used entries in the buffer, while len is the number
 * of alloc'd entries (size of buffer divided by sizeof(char **)).
 * 
 * returns 1 if the buffer is alloc'd/extended, 0 otherwise.
 */
int check_buffer_bounds(int *count, int *len, char ***buf)
{
    if(*count >= *len)
    {
        if(!(*buf))
        {
            /* first call. alloc memory for the buffer */
            *buf = malloc(32*sizeof(char **));
            if(!(*buf))
            {
                return 0;
            }
            *len = 32;
        }
        else
        {
            /* subsequent calls. extend the buffer */
            int newlen = (*len) << 1;
            char **hn2 = realloc(*buf, newlen*sizeof(char **));
            if(!hn2)
            {
                return 0;
            }
            *buf = hn2;
            *len = newlen;
        }
    }
    return 1;
}


/****************************************
 * 3 - Miscellaneous functions
 ****************************************/

/*
 * return 1 if the current user is root, 0 otherwise.
 */
int isroot()
{
    static uid_t uid = -1;
    static char  gotuid = 0;
    if(!gotuid)
    {
        uid = geteuid();
        gotuid = 1;
    }
    return (uid == 0);
}


/*
 * search the path for the given file.. if use_path is NULL, we use the value
 * of $PATH, otherwise we use the value of use_path as the search path.. if
 * exe_only is non-zero, we search for executable files, otherwise we search
 * for any file with the given name in the path.
 *
 * returns the absolute path of the first matching file, NULL if no match is found.
 */
char *search_path(char *file, char *use_path, int exe_only)
{
    /* bash extension for ignored executable files */
    char *EXECIGNORE = get_shell_varp("EXECIGNORE", NULL);
    /* use the given path or, if null, use $PATH */
    char *PATH = use_path ? use_path : getenv("PATH");
    char *p    = PATH;
    char *p2;
    
    while(p && *p)
    {
        p2 = p;
        /* get the end of the next entry */
        while(*p2 && *p2 != ':')
        {
            p2++;
        }
        int  plen = p2-p;
        if(!plen)
        {
            plen = 1;
        }
        /* copy the next entry */
        int  alen = strlen(file);
        char path[plen+1+alen+1];
        strncpy(path, p, p2-p);
        path[p2-p] = '\0';
        if(p2[-1] != '/')
        {
            strcat(path, "/");
        }
        /* and concat the file name */
        strcat(path, file);
        /* check if the file exists */
        struct stat st;
        if(stat(path, &st) == 0)
        {
            /* not a regular file */
            if(!S_ISREG(st.st_mode))
            {
                errno = ENOENT;
                /* check the next path entry */
                p = p2;
                if(*p2 == ':')
                {
                    p++;
                }
                continue;
            }
            /* requested exe files only */
            if(exe_only)
            {
                if(access(path, X_OK) != 0)
                {
                    errno = ENOEXEC;
                    /* check the next path entry */
                    p = p2;
                    if(*p2 == ':')
                    {
                        p++;
                    }
                    continue;
                }
            }
            /* check its not one of the files we should ignore */
            if(EXECIGNORE)
            {
                if(!match_ignore(EXECIGNORE, path))
                {
                    return get_malloced_str(path);
                }
            }
            else
            {
                return get_malloced_str(path);
            }
        }
        else    /* file not found */
        {
            /* check the next path entry */
            p = p2;
            if(*p2 == ':')
            {
                p++;
            }
        }
    }

    errno = ENOEXEC;
    return 0;
}


/*
 * return 1 if the given cmd name is a defined function, 0 otherwise.
 */
int is_function(char *cmd)
{
    return get_func(cmd) ? 1 : 0;
}


/*
 * return 1 if the given cmd name is a builtin utility, 0 otherwise.
 */
int is_builtin(char *cmd)
{
    return is_special_builtin(cmd) ? 1 : is_regular_builtin(cmd);
}


/*
 * return 1 if the given cmd name is an enabled special builtin utility, -1 if it
 * is an enabled regular builtin utility, 0 otherwise.
 */
int is_enabled_builtin(char *cmd)
{
    if(!cmd)
    {
        return 0;
    }
    int     j;
    for(j = 0; j < special_builtin_count; j++)
    {
        if(strcmp(special_builtins[j].name, cmd) == 0 && 
           flag_set(special_builtins[j].flags, BUILTIN_ENABLED))
        {
            return 1;
        }
    }
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(strcmp(regular_builtins[j].name, cmd) == 0 &&
           flag_set(regular_builtins[j].flags, BUILTIN_ENABLED))
        {
            return -1;
        }
    }
    return 0;
}


/*
 * return 1 if the given cmd name is a special builtin utility, 0 otherwise.
 */
int is_special_builtin(char *cmd)
{
    if(!cmd)
    {
        return 0;
    }
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < special_builtin_count; j++)
    {
        if(special_builtins[j].namelen != cmdlen)
        {
            continue;
        }
        if(strcmp(special_builtins[j].name, cmd) == 0)
        {
            return 1;
        }
    }
    return 0;
}


/*
 * return 1 if the given cmd name is a regular builtin utility, 0 otherwise.
 */
int is_regular_builtin(char *cmd)
{
    if(!cmd)
    {
        return 0;
    }
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(regular_builtins[j].namelen != cmdlen)
        {
            continue;
        }
        if(strcmp(regular_builtins[j].name, cmd) == 0)
        {
            return 1;
        }
    }
    return 0;
}


/*
 * fork a new child process to execute a command, passing it argc and argv..
 * flagarg is an optional argument needed by flags.. if flags are empty (i.e. zero),
 * flagarg should also be zero.. use_path tells us if we should use $PATH when
 * searching for the command (if use_path is NULL).. flags are set by some builtin
 * utilities, such as nice and nohup.. the UTILITY parameter is the name of the builtin
 * utility that called us (we use it in printing error messages).
 *
 * returns the exit status of the child process after executing the command.
 */
int fork_command(int argc, char **argv, char *use_path, char *UTILITY, int flags, int flagarg)
{
    pid_t child_pid;
    if((child_pid = fork_child()) == 0)    /* child process */
    {
        if(option_set('m'))
        {
            setpgid(0, 0);
            tcsetpgrp(0, child_pid);
        }
        reset_nonignored_traps();

        /* request to change the command's nice value (by the nice builtin) */
        if(flag_set(flags, FORK_COMMAND_DONICE))
        {
            if(setpriority(PRIO_PROCESS, 0, flagarg) == -1)
            {
                fprintf(stderr, "%s: failed to set nice value to %d: %s\n", UTILITY, flagarg, strerror(errno));
            }
        }
        /* request to ignore SIGHUP (by the nohup builtin) */
        if(flag_set(flags, FORK_COMMAND_IGNORE_HUP))
        {
            /* tcsh ignores the HUP signal here */
            if(signal(SIGHUP, SIG_IGN) == SIG_ERR)
            {
                fprintf(stderr, "%s: failed to ignore SIGHUP: %s\n", UTILITY, strerror(errno));
            }
            /*
             * ... and GNU coreutils nohup modifies the standard streams if they are
             * connected to a terminal.
             */
            if(isatty(0))
            {
                close(0);
                open("/dev/null", O_RDONLY);
            }
            if(isatty(1))
            {
                close(1);
                /* try to open a file in CWD. if failed, try to open it in $HOME */
                if(open("nohup.out", O_APPEND) == -1)
                {
                    char *home = get_shell_varp("HOME", ".");
                    char path[strlen(home)+11];
                    sprintf(path, "%s/nohup.out", home);
                    if(open(path, O_APPEND) == -1)
                    {
                        /* nothing. open the NULL device */
                        open("/dev/null", O_WRONLY);
                    }
                }
            }
            /* redirect stderr to stdout */
            if(isatty(2))
            {
                close(2);
                dup2(1, 2);
            }
        }
        /* export variables and execute the command */
        do_export_vars();
        do_exec_cmd(argc, argv, use_path, NULL);
        /* NOTE: we should NEVER come back here, unless there is error of course!! */
        fprintf(stderr, "%s: failed to exec '%s': %s\n", UTILITY, argv[0], strerror(errno));
        if(errno == ENOEXEC)
        {
            exit(EXIT_ERROR_NOEXEC);
        }
        if(errno == ENOENT)
        {
            exit(EXIT_ERROR_NOENT);
        }
        exit(EXIT_FAILURE);
    }
    /* ... and parent countinues over here ...    */

    /* NOTE: we re-set the process group id here (and above in the child process) to make
     *       sure it gets set whether the parent or child runs first (i.e. avoid race condition).
     */
    if(option_set('m'))
    {
        setpgid(child_pid, 0);
        /* tell the terminal who's the foreground pgid now */
        tcsetpgrp(0, child_pid);
    }
    //pid_t resid;
    int   status;
    //resid = waitpid(child_pid, &status, WAIT_FLAG);
    waitpid(child_pid, &status, WAIT_FLAG);
    if(WIFSTOPPED(status) && option_set('m'))
    {
        struct job *job = add_job(child_pid, (pid_t[]){child_pid}, 1, argv[0], 0);
        set_cur_job(job);
        notice_termination(child_pid, status);
    }
    else
    {
        set_exit_status(status, 1);
        struct job *job = get_job_by_any_pid(child_pid);
        if(job)
        {
            set_pid_exit_status(job, child_pid, status);
            set_job_exit_status(job, child_pid, status);
        }
    }
    /* reset the terminal's foreground pgid */
    if(option_set('m'))
    {
        tcsetpgrp(0, tty_pid);
    }
    return status;
}


/*
 * return 1 if path exists and is a regular file, 0 otherwise.
 */
int file_exists(char *path)
{
    struct stat st;
    if(stat(path, &st) == 0)
    {
        if(S_ISREG(st.st_mode))
        {
            return 1;
        }
    }
    return 0;
}


/*
 * return the full path to a temporary filename template, to be passed to 
 * mkstemp() or mkdtemp().. as both functions modify the string we pass
 * them, we get an malloc'd string, instead of using a string from our
 * string buffer.. it is the caller's responsibility to free that string.
 */
char *get_tmp_filename()
{
    char *tmpdir = get_shell_varp("TMPDIR", "/tmp");
    int   len = strlen(tmpdir);
    char  buf[len+18];
    sprintf(buf, "%s%clsh/", tmpdir, '/');
    /* try to mkdir our temp directory, so that all our tmp files reside under /tmp/lsh. */
    if(mkdir(buf, 0700) == 0)
    {
        strcat(buf, "lsh.tmpXXXXXX");
    }
    /* if we failed, just return a normal tmp file under /tmp */
    else
    {
        sprintf(buf, "%s%clsh.tmpXXXXXX", tmpdir, '/');
    }
    return __get_malloced_str(buf);
}
