/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: main.c
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

/* macro definition needed to use setenv() */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <sys/wait.h>
#include "cmd.h"
#include "sig.h"
#include "debug.h"
#include "backend/backend.h"
#include "symtab/symtab.h"
#include "builtins/builtins.h"
#include "builtins/setx.h"

/* pgid of the shell */
pid_t  shell_pid = 0;

/* foreground pgid of the terminal at shell startup */
pid_t  orig_tty_pgid = 0;

/* flag to indicate if we are reading from stdin */
int    read_stdin = 0;

/* flag to indicate whether the shell is interactive or not */
int    interactive_shell = 0;

/* flag to indicate whether the shell is restricted or not */
int    restricted_shell = 0;

/* defined in initsh.c */
extern int   noprofile;        /* if set, do not load login scripts */
extern int   norc;             /* if set, do not load rc scripts */

/* defined below */
long read_pipe(FILE *f, char **str);


/*
 * main shell entry point.
 */
int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    /* init global symbol table */
    init_symtab();

    /* init utility names hashtable */
    init_utility_hashtable();

    /* init strings hashtable */
    init_str_hashtable();
    
    /* init the functions table */
    init_functab();
    
    /* init traps table */
    init_traps();
    
    /* init aliases */
    memset(aliases, 0, sizeof(aliases));

    shell_argc = argc;
    shell_argv = argv;
    shell_pid  = getpid();
    
    /* init shell variables from the environment */
    initsh(argv);

    /*
     * if we have a script file name or a command string passed to us, initsh() will
     * load it into the following source struct.
     */
    struct source_s src;
    memset(&src, 0, sizeof(struct source_s));

    /* parse the command line options, if any */
    int islogin = parse_shell_args(argc, argv, &src);
    set_option('L', islogin ? 1 : 0);
    
    /* save the options string before we read startup scripts */
    symtab_save_options();
    
    if(islogin)
    {
        set_optionx(OPTION_LOGIN_SHELL, 1);
        read_dirsfile = 1;
        /* so that we will automatically save the dirstack on logout */
        set_optionx(OPTION_SAVE_DIRS, 1);
    }
    
    /* 
     * $SHELLOPTS contains a colon-separated list of options to set.
     * bash uses this variable and sets the options before reading any
     * startup files if the shell is not restricted. as we've mangled our
     * $SHELLOPTS variable by parsing options in the inish.c file, we'll
     * use the SHELLOPTS environment variable, which should contain any options
     * specified by the user before calling us.
     */
    char *s = getenv("SHELLOPTS");
    if(s && !option_set('r'))
    {
        char *s2;
        while((s2 = next_colon_entry(&s)))
        {
            do_options("-o", s2);
            free(s2);
        }
        symtab_save_options();
    }

    /* not in privileged mode? reset ids (bash) */
    if(!option_set('p'))
    {
        uid_t euid = geteuid(), ruid = getuid();
        gid_t egid = getegid(), rgid = getgid();

        if(euid != ruid)
        {
            seteuid(ruid);
        }

        if(egid != rgid)
        {
            setegid(rgid);
        }
        
        /* bash doesn't read startup files in this case */
        noprofile = 1;
        norc      = 1;
    }
    
    /*
     * we check to see if this is a login shell. If so, read /etc/profile
     * and then ~/.profile.
     */
    if(islogin)
    {
        init_login();
    }

    /*
     * check for and execute $ENV file, if any (if not in privileged mode).
     * if not privileged, ksh also uses /etc/suid_profile instead of $ENV
     * (but we pass this one).
     */
    if(interactive_shell)
    {
        init_rc();
    }
    
    /*
     * the restricted mode '-r' is enabled below, after $ENV and .profile
     * scripts have been read and executed (above). this is in conformance
     * to ksh behaviour.
     */

    if(restricted_shell)
    {
        set_option('r', 1);
        set_optionx(OPTION_RESTRICTED_SHELL, 1);
        
        struct symtab_entry_s *entry = get_symtab_entry("PATH");
        if(entry)
        {
            entry->flags |= FLAG_READONLY;
        }
    }
    
    /* initialize the terminal */
    if(read_stdin)
    {
        init_tty();
    }

    /* 
     * save the signals we got from our parent, so that we can reset 
     * them in our children later on.
     */
    save_signals();
    
    /* set our own signal handlers */
    init_signals();
    
    /*
     * speed up the startup of subshells by omitting some features that are used
     * for interactive shells, like command history, aliases and dirstacks. in this
     * case, we only init these features if this is NOT a subshell.
     */
    if(interactive_shell)
    {
        /* init history */
        init_history();

        /* init the directory stack */
        init_dirstack();

        /* get us some useful alias definitions */
        init_aliases();
    }
    else
    {
        /* turn off options we don't need in a (non-interactive) subshell */
        set_optionx(OPTION_EXPAND_ALIASES, 0);
        set_optionx(OPTION_SAVE_HIST     , 0);
    }
    
    /* init our internal clock */
    start_clock();
    
    /* seed the random number generator */
    init_rand();
    
    /* only set $PPID if this is not a subshell */
    if(!executing_subshell)
    {
        pid_t ppid = getppid();
        char ppid_str[10];
        sprintf(ppid_str, "%u", ppid);
        setenv("PPID", ppid_str, 1);
        struct symtab_entry_s *entry = add_to_symtab("PPID");
        symtab_entry_setval(entry, ppid_str);
        entry->flags |= FLAG_READONLY;
    }

    /* set the exit status to 0 */
    exit_status = EXIT_SUCCESS;

    /* now we can enforce the restricted mode, as startup scripts are all read and executed */
    startup_finished = 1;
    
    /* main program loop */
    if(read_stdin)
    {
        cmdline();
    }
    else
    {
        parse_and_execute(&src);
    }

    /* the exit builtin will execute any EXIT traps */
    do_builtin_internal(exit_builtin, 1, (char *[]){ "exit", NULL });
}


#define SAVE_TO_HISTORY_AND_PRINT(cmd_node)     \
do {                                            \
    if(save_hist)                               \
    {                                           \
        save_to_history((cmd_node));            \
    }                                           \
    if(option_set('v'))                         \
    {                                           \
        fprintf(stderr, "%s\n", (cmd_node));    \
    }                                           \
} while(0)


/*
 * parse and execute the translation unit we have in the passed source_s struct.
 * 
 * returns 1.
 */
int parse_and_execute(struct source_s *src)
{
    /* prologue */
    struct token_s *old_current_token = dup_token(get_current_token());
    struct token_s *old_previous_token = dup_token(get_previous_token());

    /*
     * parse and execute the translation unit, one command at a time.
     */

    /* skip any leading whitespace chars */
    skip_white_spaces(src);

    /* save the start of this line */
    src->wstart = src->curpos;

    /* sanitize our indices for the next round */
    req_continue   = 0;
    req_break      = 0;
    cur_loop_level = 0;

    /*
     * the -n option means read commands but don't execute them.
     * only effective in non-interactive shells (POSIX says interactive shells
     * may safely ignore it). this option is good for checking a script for
     * syntax errors.
     */
    int noexec = (option_set('n') && !interactive_shell);

    /*
     * determine if we're going to save commands to the history list.
     * we save history commands when the shell is interactive and we're reading
     * from stdin.
     */
    int save_hist = interactive_shell && src->srctype == SOURCE_STDIN;

    int i = src->curpos;
    int res = 1;             /* the result of parsing/executing */
    char *p;
    struct token_s *tok = tokenize(src);

    /* skip any leading comments/newlines */
    while(tok->type != TOKEN_EOF)
    {
        /* skip comments and newlines */
        if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
        {
            i = src->curpos;
            /* save the start of this line */
            src->wstart = src->curpos;
            tok = tokenize(tok->src);
        }
        else
        {
            break;
        }
    }

    /* reached EOF or error getting next token */
    if(tok->type == TOKEN_EOF)
    {
        /* don't leave any hanging token structs */
        free_token(get_current_token());
        free_token(get_previous_token());
        /* restore token pointers */
        set_current_token(old_current_token);
        set_previous_token(old_previous_token);
        return 0;
    }

    /* first time ever? keep a track of the first char of the current command line */
    if(i < 0)
    {
        i = 0;
    }

    /* clear the parser's error flag */
    parser_err = 0;

    /* restore the terminal's canonical mode if needed */
    if(read_stdin)
    {
        term_canon(1);
    }

    /* loop parsing and executing commands */
    while(tok->type != TOKEN_EOF)
    {
        i = (src->curpos < 0) ? 0 : src->curpos;

        /* parse the next command */
        struct node_s *cmd = parse_list(tok);
        struct node_s *cmd2 = cmd;

        /* parser encountered an error */
        if(parser_err)
        {
            if(cmd)
            {
                free_node_tree(cmd);
            }
            res = 0;
            break;
        }

        /* input consisted of empty lines and/or comments with no commands */
        if(!cmd)
        {
            break;
        }

        if(!cmd->lineno)
        {
            cmd->lineno = src->curline;
        }

        /* add command to the history list and echo it (if -v option is set) */
        switch(cmd2->type)
        {
            case NODE_COPROC:
            case NODE_TIME:
                /* the real command is the first child of the 'time' node */
                if(cmd2->first_child)
                {
                    cmd2 = cmd2->first_child;
                    /* fall through to the next case */
                }
                else
                {
                    /* 'time' word with no timed command */
                    SAVE_TO_HISTORY_AND_PRINT("time");
                    break;
                }
                /* fall through to the next case */
                __attribute__((fallthrough));

            case NODE_COMMAND:
            case NODE_LIST:
                if(cmd2->val_type == VAL_STR && cmd2->val.str)
                {
                    SAVE_TO_HISTORY_AND_PRINT(cmd2->val.str);
                    break;
                }
                /* fall through to the next case */
                __attribute__((fallthrough));

            default:
                if(i >= src->curpos)
                {
                    break;
                }

                p = get_malloced_strl(src->buffer, i, src->curpos-i);
                if(p)
                {
                    SAVE_TO_HISTORY_AND_PRINT(p);
                    free_malloced_str(p);
                }
#if 0
                ;
                int j = src->curpos-(tok->text_len);
                while(src->buffer[j] == '\n')
                {
                    j--;
                }
                /* copy command line to buffer */
                char buf[j-i+1];
                int k = 0;
                do
                {
                    buf[k++] = src->buffer[i++];
                } while(i < j);
                buf[k] = '\0';
                SAVE_TO_HISTORY_AND_PRINT(buf);
#endif
                break;
        }
        
        /*
         * dump the Abstract Source Tree (AST) of this translation unit.
         * note that we are using an extended option '-d' which is not
         * defined by POSIX.
         */
        if(option_set('d'))
        {
            dump_node_tree(cmd, 1);
        }

        /* we've got a command. now are we going to execute it? */
        if(noexec)
        {
            free_node_tree(cmd);
            tok = get_current_token();
            continue;
        }

        /* now execute the command */
        if(!do_list(src, cmd, NULL))
        {
            /* failed to execute command. bail out if we're interactive */
            if(interactive_shell)
            {
                res = 0;
                free_node_tree(cmd);
                break;
            }
        }
        tok = get_current_token();
        

        /* free the nodetree */
        free_node_tree(cmd);
        fflush(stdout);
        fflush(stderr);

        /* we've got a return statement */
        if(return_set)
        {
            return_set = 0;
            /*
             * we should return from dot files AND functions. calling return outside any
             * function/script should cause the shell to exit.
             */
            if(src->srctype == SOURCE_STDIN)
            {
                exit_gracefully(exit_status, NULL);
            }
            res = 0;
            break;
        }

        /*
         * POSIX does not specify the -t (or onecmd) option, as it says it is
         * mainly used with here-documents. this flag causes the shell to read
         * and execute only one command before exiting.. it is not clear what
         * exactly constitutes 'one command'.. here, we just execute the first
         * node tree we've got and exit.
         */
        if(option_set('t'))
        {
            exit_gracefully(exit_status, NULL);
        }

        /*
         * prepare for parsing the next command.
         * skip optional newline and comment tokens.
         */
        while(tok->type != TOKEN_EOF)
        {
            if(tok->type == TOKEN_COMMENT || tok->type == TOKEN_NEWLINE)
            {
                tok = tokenize(tok->src);
            }
            else
            {
                break;
            }
        }

#if 0
        /* reached EOF or error getting next token */
        if(tok->type == TOKEN_EOF)
        {
            break;
        }

        /* keep a track of the first char of the current command line */
        i = src->curpos-(tok->text_len);
        while(src->buffer[i] == '\n')
        {
            i++;
        }
#endif

        /* save the start of this line */
        src->wstart = src->curpos-(tok->text_len);
    }

    /* don't leave any hanging token structs */
    free_token(get_current_token());
    free_token(get_previous_token());

    /* finished parsing and executing commands */
    fflush(stdout);
    fflush(stderr);

    /* reset the received signal flag */
    signal_received = 0;

    /* restore the terminal's non-canonical mode if needed */
    if(read_stdin)
    {
        term_canon(0);
        update_row_col();
    }

    /* epilogue */
    set_current_token(old_current_token);
    set_previous_token(old_previous_token);

    return res;
}


/*
 * read a file (presumably a script file) and initialize the
 * source_s struct so that we can parse and execute the file.
 * 
 * NOTE: this function doesn't handle big files.. we should extend
 *       it so it uses memmaped files or any other method for handling
 *       big files.
 * 
 * returns 1 if the file is loaded successfully, 0 otherwise.
 */
int read_file(char *filename, struct source_s *src)
{
    errno = 0;
    if(!filename)
    {
        return 0;
    }
    
    char *filename2 = word_expand_to_str(filename);
    if(!filename2)
    {
        return 0;
    }
    
    char *tmpbuf = NULL;
    FILE *f = NULL;
    long i;

    if(strchr(filename2, '/'))
    {
        /* pathname with '/', try opening it */
        f = fopen(filename2, "r");
    }
    else
    {
        /* pathname with no slashes, try to locate it */
        size_t len = strlen(filename2);

        /* try CWD */
        char tmp[len+3];
        strcpy(tmp, "./");
        strcat(tmp, filename2);
        f = fopen(tmp, "r");
        if(f)   /* file found */
        {
            goto read;
        }

        /* search using $PATH */
        char *path = search_path(filename2, NULL, 0);
        if(!path)   /* file not found */
        {
            free(filename2);
            return 0;
        }
        
        f = fopen(path, "r");
        free_malloced_str(path);
        if(f)
        {
            goto read;
        }
        
        /* failed to open the file */
        free(filename2);
        return 0;
    }
    
    /* failed to open the file */
    if(!f)
    {
        goto error;
    }
    
read:
    /* seek to the end of the file */
    if(fseek(f, 0, SEEK_END) != 0)
    {
        /*
         * handle the special files (e.g. pipes), where we can't seek.. we might
         * arrive here (reading from a pipe) if we're executing a command, which
         * was passed a redirected file using process substitution.
         */
        if(errno != ESPIPE)
        {
            goto error;
        }
        
        if(!(i = read_pipe(f, &tmpbuf)))
        {
            goto error;
        }
    }
    else
    {
        /* get the file length */
        if((i = ftell(f)) == -1)
        {
            goto error;
        }
        rewind(f);
        
        /* alloc buffer */
        tmpbuf = malloc(i+1);
        if(!tmpbuf)
        {
            goto error;
        }
        
        /* read the file */
        long j = fread(tmpbuf, 1, i, f);
        if(j != i)
        {
            goto error;
        }
        tmpbuf[i] = '\0';
    }

    fclose(f);
    src->buffer   = tmpbuf;
    src->bufsize  = i;
    src->srctype  = SOURCE_EXTERNAL_FILE;
    src->srcname  = get_malloced_str(filename2);
    free(filename2);
    src->curpos   = INIT_SRC_POS;
    return 1;
    
error:
    if(f)
    {
        fclose(f);
    }

    if(tmpbuf)
    {
        free(tmpbuf);
    }
    
    free(filename2);
    return 0;
}


/*
 * similar to read_file(), except it reads from a pipe.
 * 
 * returns the count of bytes read, and stores the read string in str.
 */
long read_pipe(FILE *f, char **str)
{
    int buf_size = 32;
    char *buf = alloc_string_buf(buf_size);
    long i = 0;
    
    if(!buf)
    {
        PRINT_ERROR("%s: failed to allocate buffer: %s\n", SHELL_NAME,
                    strerror(errno));
        return 0;
    }

    char *b = buf;
    char *buf_end = b+buf_size-1;

    while(!feof(f))
    {
        if(fread(b, 1, 1, f) != 1)
        {
            continue;
        }
        
        if(!may_extend_string_buf(&buf, &buf_end, &b, &buf_size))
        {
            PRINT_ERROR("%s: failed to allocate buffer: %s\n", SHELL_NAME,
                        strerror(errno));
            return 0;
        }
        
        b++;
        i++;
    }
    
    (*str) = buf;
    return i;
}
