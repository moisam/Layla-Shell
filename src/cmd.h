/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: cmd.h
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

#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <sys/types.h>
#include "scanner/source.h"

/**************************
 * some macro definitions.
 **************************/

/* the name of this shell */
#define SHELL_NAME      "lsh"

/* simple wrappers for printing error messages */
#define PRINT_ERROR(...)                \
do                                      \
{                                       \
    fprintf(stderr, __VA_ARGS__);       \
} while(0)

#define READONLY_ASSIGN_ERROR(utility, name, type)      \
    PRINT_ERROR("%s: cannot set `%s`: readonly %s\n",   \
                utility, name, type)

#define SOURCE_NAME         get_shell_varp("0", SHELL_NAME)

/* I/O file redirection file open modes (for io_file_s->open_mode field) */
#define MODE_WRITE                      (O_RDWR | O_CREAT | O_TRUNC )
#define MODE_APPEND                     (O_RDWR | O_CREAT | O_APPEND)
#define MODE_READ                       (O_RDONLY)

/* I/O file redirection extra flags (for io_file_s->extra_flags field) */
#define NOCLOBBER_FLAG                  (1 << 0)    /* no-clobber (force creating a new file) */
#define CLOOPEN_FLAG                    (1 << 1)    /* close-on-open (used when duplicating fds) */

/* default file creation mask */
#define FILE_MASK                       (S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR)

/* default dir creation mask */
#define DIR_MASK                        (S_IRWXU | S_IRWXG | S_IRWXO)

/* some jobs-related constants */
#define MAX_PROCESS_PER_JOB             32
#define MAX_JOBS                        255
#define MAX_TOKENS                      255

/* max length of the $ENV file name */
#define MAX_ENV_NAME_LEN                31

/* default values for the maximum line and path lengths */
#define DEFAULT_LINE_MAX                4096
#define DEFAULT_PATH_MAX                4096

/* POSIX's error exit codes */
#define EXIT_ERROR_NOENT                127
#define EXIT_ERROR_NOEXEC               126

/* maximum number of aliases supported by this shell */
#define MAX_ALIASES                     256

/* maximum nested here-documents */
#define MAX_NESTED_HEREDOCS             64

/* flags for fork_command() */
#define FORK_COMMAND_DONICE             (1 << 0)
#define FORK_COMMAND_IGNORE_HUP         (1 << 1)

/* flags for word_expand() */
#define EXPAND_STRIP_QUOTES             (1 << 0)
#define EXPAND_STRIP_SPACES             (1 << 1)

/* invalid value for $OPTARG */
#define INVALID_OPTARG                  ((char *)-1)

/* default and maximum history entries */
#define default_HISTSIZE                512
#define MAX_CMD_HISTORY                 4096
#define INIT_CMD_HISTORY_SIZE           2048

/* value to indicate failure of the history expansion function */
#define INVALID_HIST_EXPAND             ((char *)-1)

/* terminal text colors */
#define COL_WHITE                       37
#define COL_GREEN                       32
#define COL_RED                         31
#define COL_BGBLACK                     40
#define COL_DEFAULT                     0

/* flags for the search_and_exec() function */
/* fork new processes to execute external commands */
#define SEARCH_AND_EXEC_DOFORK          (1 << 0)
/* do function search */
#define SEARCH_AND_EXEC_DOFUNC          (1 << 1)
// #define SEARCH_AND_EXEC_MERGE_GLOBAL    (1 << 2)

/* some marcos used by the directory stack utilities */
#define FLAG_DIRSTACK_SEPARATE_LINES    (1 << 0)    /* print each entry on a separate line */
#define FLAG_DIRSTACK_FULL_PATHS        (1 << 1)    /* don't contract the home dir to ~ */
#define FLAG_DIRSTACK_PRINT_INDEX       (1 << 2)    /* print the entry's index */
#define FLAG_DIRSTACK_WRAP_ENTRIES      (1 << 3)    /* wrap entries before they reach screen's edge -- tcsh */
#define DIRSTACK_FILE                       "~/.lshdirs"

/* flags for the do_echo() function */
#define FLAG_ECHO_ALLOW_ESCAPED         (1 << 0)
#define FLAG_ECHO_NULL_TERM             (1 << 1)
#define FLAG_ECHO_PRINT_NL              (1 << 2)

/* some macros used by the trap() function */
#define ACTION_DEFAULT                  1
#define ACTION_EXECUTE                  2
#define ACTION_IGNORE                   3
#define TRAP_COUNT                      36      /* 31 signals + EXIT + ERR + CHLD + DEBUG + RETURN */

/* some special trap numbers (similar to a signal's number) */
#define ERR_TRAP_NUM                    32
#define CHLD_TRAP_NUM                   33
#define DEBUG_TRAP_NUM                  34
#define RETURN_TRAP_NUM                 35

/* flags for the job_s structure's flags field */
/* job is running in the foreground */
#define JOB_FLAG_FORGROUND              (1 << 0)
/* job is disowned */
#define JOB_FLAG_DISOWNED               (1 << 1)
/* job status has been notified to the user */
#define JOB_FLAG_NOTIFIED               (1 << 2)
/* set by the notify builtin to notify individual jobs */
#define JOB_FLAG_NOTIFY                 (1 << 3)
/* job started with job control */
#define JOB_FLAG_JOB_CONTROL            (1 << 4)

/* flags for the builtin_s flags field */
#define BUILTIN_PRINT_VOPTION           (1 << 0)
#define BUILTIN_PRINT_HOPTION           (1 << 1)
#define BUILTIN_ENABLED                 (1 << 2)
#define BUILTIN_SPECIAL_BUILTIN         (1 << 3)

/* flags for the struct word_s flags field */
/* the word had quotes before we called remove_quotes() on it */
#define FLAG_WORD_HAD_QUOTES            (1 << 0)
#define FLAG_WORD_HAD_DOUBLE_QUOTES     (1 << 1)

/* values of the add_spaces parameter of wordlist_to_str() */
#define WORDLIST_ADD_SPACES             1
#define WORDLIST_NO_SPACES              0

/* word expansion flags for the word_expand() function */
#define FLAG_PATHNAME_EXPAND            (1 << 0)
#define FLAG_REMOVE_QUOTES              (1 << 1)
#define FLAG_FIELD_SPLITTING            (1 << 2)
#define FLAG_STRIP_VAR_ASSIGN           (1 << 3)
#define FLAG_EXPAND_VAR_ASSIGN          (1 << 4)

/* flags for do_set() */
#define SET_FLAG_GLOBAL                 (1 << 0)
#define SET_FLAG_APPEND                 (1 << 1)
#define SET_FLAG_FORCE_NEW              (1 << 2)

/* flags for parse_args() */
/* print an error message when we find an unknown option */
#define FLAG_ARGS_PRINTERR              (1 << 0)
/* exit the non-interactive shell if we find an unknown option */
#define FLAG_ARGS_ERREXIT               (1 << 1)

/* flags for hist_expand() */
#define FLAG_HISTEXPAND_DO_BACKUP       (1 << 0)

/*
 * flags for the 'force_export_all' parameter of the do_export_vars()
 * and do_export_table() functions.
 */

/* export only the variables/functions marked for export */
#define EXPORT_VARS_EXPORTED_ONLY       (0)

/* export everything (used only by subshells) */
#define EXPORT_VARS_FORCE_ALL           (1)

#include "builtins/builtins.h"

/* POSIX says non-interactive shell should exit on syntax errors */
#define EXIT_IF_NONINTERACTIVE()                                    \
do {                                                                \
        if(!interactive_shell)                                      \
        {                                                           \
            /* try to exit (this will execute any EXIT traps) */    \
            do_builtin_internal(exit_builtin, 2,                    \
                                (char *[]){ "exit", "1", NULL });   \
            /* if exit_builtin() failed, force exit */              \
            exit_gracefully(EXIT_FAILURE, NULL);                    \
        }                                                           \
} while(0);

/* macro to check if a flag is set */
#define flag_set(flags, which)      (((flags) & (which)) == (which))

/* macro to check if a job is running */
#define NOT_RUNNING(status)         (WIFEXITED((status)) || WIFSIGNALED((status)) || WIFSTOPPED((status)))

/* pgid of the shell */
extern pid_t shell_pid;

/* foreground pgid of the terminal at shell startup */
extern pid_t orig_tty_pgid;


/************************************
 * definitions of shell structures.
 ************************************/

struct alias_s
{
    char *name;         /* alias name */
    char *val;          /* aliased value */
};

/* job structure */
struct job_s
{
    int     job_num;            /* job number */
    int     proc_count;         /* number of processes in the job */
    int     pgid;               /* job's process group id */
    int     status;             /* current job status */
    char   *commandstr;         /* job's command string */
    pid_t  *pids;               /* list of process ids */
    int    *exit_codes;         /* process exit status codes */
    int     child_exits;        /* how many children did exit */
    long    child_exitbits;     /* bitfield to indicate which children exited */
    int     flags;              /* flags (see the macros above) */
    struct  termios *tty_attr;  /* terminal state when job is suspended */
};
// extern char job_run_status[MAX_JOBS];

/* tokens per input string */
struct word_s
{
    char  *data;
    int    len;
    int    flags;
    struct word_s *next;
};

/* struct for history list entries */
struct histent_s
{
    char   *cmd;        /* history command */
    time_t  time;       /* time when it was entered */
};

/* an alphabetically-sorted list of strings */
struct alpha_list_s
{
    int count;
    int len;
    char **items;
};

/* struct to represent an I/O redirection */
struct io_file_s
{
    char *path;
    int   fileno;
    int   duplicates;
    int   open_mode;
    int   extra_flags;
};

/* struct to represent callframes */
struct callframe_s
{
    char  *funcname;
    char  *srcfile ;
    int    lineno  ;
    struct callframe_s *prev;
};

/* struct for directory stack entries */
struct dirstack_ent_s
{
    char *path;
    struct dirstack_ent_s *next, *prev;
};

/* struct to represent traps */
struct trap_item_s
{
    /* action to do when trap occurs (ignore, default, ...) */
    int   action    ;
    /* command to execute when the trap occurs (if action not ignore/default) */
    char *action_str;
};


/************************************
 * definitions of shell variables.
 ************************************/

extern  struct    alias_s aliases[MAX_ALIASES];     /* alias.c */
extern  char      prompt[];                         /* prompt.c */
extern  int       startup_finished;                 /* initsh.c */
extern  size_t    terminal_row, terminal_col;       /* cmdline.c */
extern  size_t    VGA_WIDTH, VGA_HEIGHT;
extern  char     *cmdbuf;                 /* ptr to buffer */
extern  size_t    cmdbuf_index;           /* whrere to add incoming key */
extern  size_t    cmdbuf_end;             /* index of last entered key */
extern  size_t    cmdbuf_size;            /* actual malloc'd size */
extern  timer_t   timerid;
extern  char     *COMMAND_DEFAULT_PATH;                 /* helpfunc.c */
extern  char    **shell_argv;                           /* args.c */
extern  int       shell_argc;
extern  char     *internal_optarg;
extern  char      internal_opterr;
extern  int       internal_argi;
// extern  char    **internal_argv;
extern  int       internal_argsub;
extern  int       exit_status;                          /* params.c */
extern  int       executing_subshell;
extern  int       shell_level;
extern  int       read_stdin;                           /* main.c */
extern  int       interactive_shell;
extern  int       restricted_shell;
extern  int       signal_received;                      /* sig.c */
extern  int       tried_exit;                           /* builtins/exit.c */
#include "symtab/symtab.h"
extern  struct    symtab_s *func_table;                 /* functab.c */
extern  int       special_var_count;                    /* vars.c */
extern  char     *special_var_names[];
// extern  struct    histent_s cmd_history[];              /* builtins/history.c */
extern  struct    histent_s *cmd_history;               /* builtins/history.c */
extern  int       cmd_history_index;
extern  int       cmd_history_end;
extern  int       hist_file_count;
extern  int       hist_cmds_this_session;
extern  int       HISTSIZE;
extern  char      default_hist_filename[];
extern  int       executing_trap;                       /* builtins/trap.c */
extern  char     *cwd;                                  /* builtins/cd.c */


/***********************************************
 * function declarations (sorted by source file).
 ***********************************************/
 
/* prompt.c */
char   *evaluate_prompt(char *PS);
void    do_print_prompt(char *which);
void    print_prompt(void);
void    print_prompt2(void);
void    print_prompt3(void);
void    print_prompt4(void);

/* popen.c */
FILE   *popenr(char *cmd);
void    init_subshell(void);
void    inc_shlvl_var(int amount);

/* initsh.c */
void    initsh(char **argv);
void    init_tty(void);
void    init_login(void);
void    init_rc(void);
int     parse_shell_args(int argc, char **argv, struct source_s *src);

/* cmdline.c */
void    cmdline(void);
void    init_cmdbuf(void);
int     ext_cmdbuf(char **cmdbuf, size_t *size, size_t howmuch);
char   *read_cmd(void);
int     is_incomplete_cmd(int first_time);
size_t  glue_cmd_pieces(void);

/* strbuf.c */
void    init_str_hashtable(void);
char   *__get_malloced_str(char *str);
char   *get_malloced_str(char *str);
char   *get_malloced_strl(char *str, int start, int length);
void    free_malloced_str(char *str);

/* terminal.c */
void    term_canon(int on);
int     get_screen_size(void);
void    move_cur(int row, int col);
void    clear_screen(void);
void    set_terminal_color(int FG, int BG);
void    update_row_col(void);
size_t  get_terminal_row(void);
size_t  get_terminal_col(void);
int     cur_tty_fd(void);
struct  termios *save_tty_attr(void);
int     set_tty_attr(int tty, struct termios *attr);

/* strings.c */
int     strupper(char *str);
int     strlower(char *str);
void    strcat_c(char *str, int pos, char chr);
int     is_same_str(char *s1, char *s2);
char   *strchr_any(char *string, char *chars);
char   *list_to_str(char **list);
char   *quote_val(char *val, int add_quotes, int escape_sq);
int     get_linemax(void);
int     check_buffer_bounds(int *count, int *len, char ***names);
char   *next_colon_entry(char **colon_list);
char   *next_path_entry(char **colon_list, char *filename, int use_dot);
#include <sys/time.h>
int     get_secs_usecs(char *str, struct timeval *res);
char   *alloc_string_buf(int size);
int     may_extend_string_buf(char **buf, char **buf_end, char **buf_ptr, int *buf_size);

/* helpfunc.c */
int     beep(void);
char   *get_default_path(void);
char   *search_path(char *file, char *use_path, int exe_only);
int     fork_command(int argc, char **argv, char *use_path, char *UTILITY,
                     int flags, int flagarg);
int     isroot(void);
int     file_exists(char *path);
char   *get_tmp_filename(void);
char   *get_shell_varp(char *name, char *def_val);
int     get_shell_vari(char *name, int def_val);
long    get_shell_varl(char *name, int def_val);
void    set_shell_varp(char *name, char *val);
void    set_shell_vari(char *name, int val);

/* wordexp.c */
int     is_restrict_var(char *name);
size_t  find_closing_quote(char *data, int in_double_quotes, int sq_nesting);
size_t  find_closing_brace(char *data, int in_double_quotes);
void    delete_char_at(char *str, size_t index);
char   *substitute_str(char *s1, char *s2, size_t start, size_t end);
char   *get_all_vars(char *prefix);
char   *pos_params_expand(char *tmp, int in_double_quotes);

struct  word_s *word_expand(char *orig_word, int flags);
struct  word_s *word_expand_one_word(char *orig_word, int flags);
char   *word_expand_to_str(char *word);
char   *wordlist_to_str(struct word_s *word, int add_spaces);
void    free_all_words(struct word_s *first);
struct  word_s *make_word(char *word);
void    skip_IFS_whitespace(char **__str, char *__IFS);
int     is_IFS_char(char c, char *IFS);

char   *tilde_expand(char *s);
char   *command_substitute(char *__cmd);
char   *ansic_expand(char *str);
char   *var_expand(char *__var_name);
struct  word_s *pathnames_expand(struct word_s *words);
void    remove_quotes(struct word_s *wordlist);
struct  word_s *field_split(char *str);

/* shunt.c */
char   *arithm_expand(char *__expr);
int     get_ndigit(char c, int base, int *result);

/* braceexp.c */
char  **brace_expand(char *str, size_t *count);
int     is_num(char *str);

/* tab.c */
int     do_tab(char *cmdbuf, size_t *cmdbuf_index, size_t *cmdbuf_end);

/* args.c */
int     parse_args(int __argc, char **__argv, char *__ops, int *__argi, int flags);

/* params.c */
void    init_shell_vars(char *pw_name, gid_t gid, char *fullpath);
int     is_pos_param(char *var_name);
int     is_special_param(char *var_name);
char   *get_all_pos_params_str(char which, int quoted);
char   *get_pos_params_str(char which, int quoted, int offset, int count);
int     pos_param_count(void);
struct  symtab_entry_s *get_pos_param(int i);
void    set_exit_status(int status);
void    set_internal_exit_status(int status);
void    reset_pos_params(void);
void    set_local_pos_params(int count, char **params);

/* main.c */
int     parse_and_execute(struct source_s *src);
int     read_file(char *filename, struct source_s *src);

/* builtins/exit.c */
void    exit_gracefully(int stat, char *errmsg);

/* functab.c */
void    init_functab(void);
struct  symtab_entry_s *get_func(char *name);
struct  symtab_entry_s *add_func(char *name);
int     unset_func(char *name);
void    print_func_attribs(unsigned int flag);

/* vars.c */
void    init_rand(void);
char   *get_special_var(char *name, char *old_val);
int     set_special_var(char *name, char *val);
void    set_underscore_val(char *val, int set_env);
void    save_OPTIND(void);
void    reset_OPTIND(void);

/* builtins/history.c */
char   *get_last_cmd_history(void);
void    clear_history(int start, int end);
char   *save_to_history(char *cmd_buf);
void    flush_history(void);
void    init_history(void);
void    remove_newest(void);
void    load_history_list(void);

/* builtins/hist_expand.c */
char   *hist_expand(int quotes, int flags);
int     nodetree_hist_expand(struct node_s *cmd);

/* alphalist.c */
void    init_alpha_list(struct alpha_list_s *list);
void    free_alpha_list(struct alpha_list_s *list);
void    print_alpha_list(struct alpha_list_s *list);
void    add_to_alpha_list(struct alpha_list_s *list, char *str);
char   *alpha_list_make_str(const char *fmt, ...);

/* jobs.c */
struct  job_s *get_job_by_jobid(int n);
struct  job_s *get_job_by_any_pid(pid_t pid);
struct  job_s *add_job(struct job_s *new_job);
struct  job_s *new_job(char *commandstr, int is_bg);
pid_t  *get_malloced_pids(pid_t pids[], int pid_count);
int    *get_malloced_exit_codes(int pid_count);
void    free_job(struct job_s *job, int free_struct);
int     remove_job(struct job_s *job);
void    check_on_children(void);
void    notice_termination(pid_t pid, int status, int add_to_deadlist);
int     set_cur_job(struct job_s *job);
int     get_jobid(char *jobid_str);
int     get_total_jobs(void);
void    set_job_exit_status(struct job_s *job, pid_t pid, int status);
void    set_pid_exit_status(struct job_s *job, pid_t pid, int status);
int     get_pid_exit_status(struct job_s *job, pid_t pid);
int     pending_jobs(void);
void    kill_all_jobs(int signum, int flag);
int     rip_dead(pid_t pid);
void    add_pid_to_job(struct job_s *job, pid_t pid);
void    print_status_message(struct job_s *job, pid_t pid, int status, int output_pid, FILE *out);

/* builtins/set.c */
int     option_set(char which);
void    set_option(char option, int set);
int     do_options(char *ops, char *extra);
struct  symtab_entry_s *do_set(char *name_buf, char *val_buf, int set_flags, int unset_flags, int flags);
void    symtab_save_options(void);
char    short_option(char *long_option);
int     is_short_option(char which);
void    do_privileged(int on);
void    do_posix(int on);

/* builtins/trap.c */
void    init_traps(void);
void    reset_nonignored_traps(void);
void    trap_handler(int signum);
struct  trap_item_s *save_trap(char *name);
void    restore_trap(char *name, struct trap_item_s *saved);
struct  trap_item_s *get_trap_item(char *trap);
void    block_traps(void);
void    unblock_traps(void);
void    do_pending_traps(void);

/* builtins/command.c */
int     search_and_exec(struct source_s *src, int cargc, char **cargv, char *PATH, int flags);

/* builtins/cd.c */
char   *get_home(int no_fail);


#endif
