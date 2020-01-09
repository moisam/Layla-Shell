/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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

/* flags for fork_command() */
#define FORK_COMMAND_DONICE             (1 << 0)
#define FORK_COMMAND_IGNORE_HUP         (1 << 1)

/* flags for word_expand() */
#define EXPAND_STRIP_QUOTES             (1 << 0)
#define EXPAND_STRIP_SPACES             (1 << 1)

/* invalid value for $OPTARG */
#define INVALID_OPTARG                  ((char *)-1)

/* default and maximum history entries */
#define  default_HISTSIZE               512
#define  MAX_CMD_HISTORY                512

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

/* flags for the job_s flags field */
#define JOB_FLAG_FORGROUND              (1 << 0)
#define JOB_FLAG_DISOWNED               (1 << 1)
#define JOB_FLAG_NOTIFIED               (1 << 2)
#define JOB_FLAG_NOTIFY                 (1 << 3)    /* set by the notify builtin to notify individual jobs */

/* flags for the builtin_s flags field */
#define BUILTIN_PRINT_VOPTION           (1 << 0)
#define BUILTIN_PRINT_HOPTION           (1 << 1)
#define BUILTIN_ENABLED                 (1 << 2)

/* values of the add_spaces parameter of wordlist_to_str() */
#define WORDLIST_ADD_SPACES             1
#define WORDLIST_NO_SPACES              0

// #define WORD_EXPANSION_FIELD_SPLIT      1
// #define WORD_EXPANSION_NO_FIELD_SPLIT   0
/* word expansion flags for the word_expand() function */
#define FLAG_PATHNAME_EXPAND            (1 << 0)
#define FLAG_REMOVE_QUOTES              (1 << 1)
#define FLAG_FIELD_SPLITTING            (1 << 2)

/*
 * flags for the 'force_export_all' parameter of the do_export_vars()
 * and do_export_table() functions.
 */

/* export only the variables/functions marked for export */
#define EXPORT_VARS_EXPORTED_ONLY       (0)

/* export everything (used only by subshells) */
#define EXPORT_VARS_FORCE_ALL           (1)

/* POSIX says non-interactive shell should exit on syntax errors */
#define EXIT_IF_NONINTERACTIVE()                    \
do {                                                \
        if(!interactive_shell)                        \
            exit_gracefully(EXIT_FAILURE, NULL);    \
} while(0);

/* macro to check if a flag is set */
#define flag_set(flags, which)      (((flags) & (which)) == (which))

/* macro to check if a job is running */
#define NOT_RUNNING(status)         (WIFEXITED((status)) || WIFSIGNALED((status)) || WIFSTOPPED((status)))

/* process group id of the command line interpreter */
extern pid_t tty_pid;


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

/* special shell variables (used in vars.c) */
struct var_s
{
    char *name;
    char *val ;
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

/* struct for builtin utilities */
struct builtin_s
{
    char   *name;         /* utility name */
    /*
     * helps us when comparing builtin function names,
     * so that we don't need to call strlen() over and over.
     */
    size_t  namelen;      /* length of utility name */
    char   *explanation;  /* what does the utility do */
    void   *func;         /* function to call to execute the utility */
    /*
     * fields used by the help utility to display
     * information about each builtin utility.
     */
    int     synopsis_name_count;  /* how many times does the utility name
                                 * appear in the synopsis
                                 */
    char   *synopsis;             /* utility usage */
    char   *help;                 /* longer help message */
    unsigned char flags;
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


/***********************************************
 * function declarations (sorted by source file).
 ***********************************************/
 
/* prompt.c */
extern  char prompt[];
char   *evaluate_prompt(char *PS);
void    do_print_prompt(char *which);
void    print_prompt(void);
void    print_prompt2(void);
void    print_prompt3(void);
void    print_prompt4(void);

/* popen.c */
FILE   *popenr(char *cmd);
void    init_subshell(void);
void    set_shlvl_var(void);

/* initsh.c */
extern  int  startup_finished;
void    initsh(char **argv, int init_tty);
void    init_login(void);
void    init_rc(void);
int     parse_shell_args(int argc, char **argv, struct source_s *src);

/* cmdline.c */
extern  int       terminal_row, terminal_col;
extern  int       VGA_WIDTH, VGA_HEIGHT;
extern  char     *cmdbuf      ;           /* ptr to buffer */
extern  uint16_t  cmdbuf_index;           /* whrere to add incoming key */
extern  uint16_t  cmdbuf_end  ;           /* index of last entered key */
extern  uint16_t  cmdbuf_size ;           /* actual malloc'd size */
extern  timer_t   timerid     ;
void    cmdline(void);
int     ext_cmdbuf(size_t howmuch);
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
int     echoon(int fd);
int     echooff(int fd);
void    term_canon(int on);
int     get_screen_size(void);
void    move_cur(int row, int col);
void    clear_screen(void);
void    set_terminal_color(int FG, int BG);
void    update_row_col(void);
int     get_terminal_row(void);
int     get_terminal_col(void);

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

/* helpfunc.c */
extern  char *COMMAND_DEFAULT_PATH;
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

/* wordexp.c */
int     is_restrict_var(char *name);
size_t  find_closing_quote(char *data, int in_double_quotes, int sq_nesting);
size_t  find_closing_brace(char *data, int in_double_quotes);
void    delete_char_at(char *str, size_t index);
char   *substitute_str(char *s1, char *s2, size_t start, size_t end);
char   *get_all_vars(char *prefix);
char   *pos_params_expand(char *tmp, int in_double_quotes);

struct  word_s *word_expand(char *orig_word, int flags);
struct  word_s *word_expand_one_word(char *orig_word, int do_field_splitting);
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

/* tab.c */
int     do_tab(char *cmdbuf, uint16_t *cmdbuf_index, uint16_t *cmdbuf_end);

/* args.c */
extern  char **shell_argv;
extern  int    shell_argc;
extern  char *__optarg   ;
extern  char  __opterr   ;
extern  int     argi     ;
int     parse_args(int __argc, char **__argv, char *__ops, int *__argi, int errexit);

/* params.c */
extern  int exit_status;
extern  int subshell_level;

void    init_shell_vars(char *pw_name, gid_t gid, char *fullpath);
int     is_pos_param(char *var_name);
int     is_special_param(char *var_name);
char   *get_all_pos_params_str(char which, int quoted);
char   *get_pos_params_str(char which, int quoted, int offset, int count);
int     pos_param_count(void);
char  **get_pos_paramsp(void);
void    set_pos_paramsp(char **p);
struct  symtab_entry_s *get_pos_param(int i);
void    set_exit_status(int status);
void    set_internal_exit_status(int status);
void    reset_pos_params(void);

/* main.c */
extern  int signal_received;
extern  int read_stdin;
extern  int interactive_shell;

int     parse_and_execute(struct source_s *src);
int     read_file(char *filename, struct source_s *src);
int     set_signal_handler(int signum, void (handler)(int));
void    SIGCHLD_handler(int signum);
void    SIGINT_handler(int signum);

/* functab.c */
#include "symtab/symtab.h"
extern  struct symtab_s *func_table;
void    init_functab(void);
struct  symtab_entry_s *get_func(char *name);
struct  symtab_entry_s *add_func(char *name);
void    unset_func(char *name);
void    purge_exported_funcs(void);

/* vars.c */
void    init_rand(void);
char   *get_special_var(char *name);
int     set_special_var(char *name, char *val);

extern  int special_var_count;
extern  struct var_s special_vars[];

/* builtins/history.c */
extern  struct histent_s cmd_history[];
extern  int    cmd_history_index;
extern  int    cmd_history_end;
extern  int    hist_file_count;
extern  int    HISTSIZE;
extern  char   hist_file[];
char   *get_last_cmd_history(void);
void    clear_history(int start, int end);
char   *save_to_history(char *cmd_buf);
void    flush_history(void);
void    init_history(void);
void    remove_newest(void);
int     history_builtin(int argc, char **argv);

/* builtins/hist_expand.c */
char   *hist_expand(int quotes);

/* alphalist.c */
void    init_alpha_list(struct alpha_list_s *list);
void    free_alpha_list(struct alpha_list_s *list);
void    print_alpha_list(struct alpha_list_s *list);
void    add_to_alpha_list(struct alpha_list_s *list, char *str);
char   *alpha_list_make_str(const char *fmt, ...);

/******************************************/
/* BUILTIN UTILITIES                      */
/******************************************/

/* builtins/builtins.c */
extern  int    regular_builtin_count;
extern  struct builtin_s regular_builtins[];
extern  int    special_builtin_count;
extern  struct builtin_s special_builtins[];

int     builtin_builtin(int argc, char **argv);
int     is_function(char *cmd);
int     is_builtin(char *cmd);
int     is_enabled_builtin(char *cmd);
int     is_special_builtin(char *cmd);
int     is_regular_builtin(char *cmd);

/* builtins/help.c */
extern  char shell_ver[];
void    print_help(char *invokation_name, int index, int isreg, int flags);
int     help_builtin(int argc, char **argv);

/******************************************/
/* A- Regular Builtins                    */
/******************************************/

/* builtins/alias.c */
extern  struct alias_s aliases[MAX_ALIASES];
int     alias_builtin(int argc, char **argv);
void    init_aliases(void);
int     valid_alias_name(char *name);
void    run_alias_cmd(char *alias);
void    unset_all_aliases(void);

/* builtins/bg.c */
int     bg_builtin(int argc, char **argv);

/* builtins/cd.c */
extern  char *cwd;
int     cd_builtin(int argc, char **argv);
char   *get_home(void);

/* builtins/command.c */
int     command_builtin(int argc, char **argv);
int     search_and_exec(struct source_s *src, int cargc, char **cargv, char *PATH, int flags);

/* builtins/false.c */
int     false_builtin(int argc, char **argv);

/* builtins/fc.c */
int     fc_builtin(int argc, char **argv);

/* builtins/fg.c */
int     fg_builtin(int argc, char **argv);

/* builtins/getopts.c */
int     getopts_builtin(int argc, char **argv);

/* jobs.c */
struct  job_s *get_job_by_jobid(int n);
struct  job_s *get_job_by_any_pid(pid_t pid);
struct  job_s *add_job(pid_t pgid, pid_t pids[], int pid_count, char *commandstr, int is_bg);
pid_t  *get_malloced_pids(pid_t pids[], int pid_count);
int     kill_job(struct job_s *j);
void    check_on_children(void);
void    notice_termination(pid_t pid, int status);
int     set_cur_job(struct job_s *job);
int     jobs_builtin(int argc, char **argv);
int     get_jobid(char *jobid_str);
int     get_total_jobs(void);
void    set_job_exit_status(struct job_s *job, pid_t pid, int status);
void    set_pid_exit_status(struct job_s *job, pid_t pid, int status);
int     get_pid_exit_status(struct job_s *job, pid_t pid);
int     pending_jobs(void);
void    kill_all_jobs(int signum, int flag);
int     rip_dead(pid_t pid);

/* builtins/kill.c */
int     kill_builtin(int argc, char **argv);

/* builtins/newgrp.c */
int     newgrp_builtin(int argc, char **argv);

/* builtins/pwd.c */
int     pwd_builtin(int argc, char **argv);

/* builtins/read.c */
int     read_builtin(int argc, char **argv);
int     ready_to_read(int fd);

/* builtins/true.c */
int     true_builtin(int argc, char **argv);

/* builtins/type.c */
int     type_builtin(int argc, char **argv);

/* builtins/umask.c */
int     umask_builtin(int argc, char **argv);

/* builtins/ulimit.c */
int     ulimit_builtin(int argc, char **argv);

/* builtins/unalias.c */
int     unalias_builtin(int argc, char **argv);
void    unalias_all(void);

/* builtins/wait.c */
int     wait_builtin(int argc, char **argv);

/* builtins/disown.c */
int     disown_builtin(int argc, char **argv);

/* builtins/nice.c */
int     nice_builtin(int argc, char **argv);

/* builtins/hup.c */
int     hup_builtin(int argc, char **argv);

/* builtins/notify.c */
int     notify_builtin(int argc, char **argv);

/* builtins/caller.c */
int     caller_builtin(int argc, char **argv);
struct  callframe_s *callframe_new(char *funcname, char *srcfile, int lineno);
struct  callframe_s *get_cur_callframe(void);
int     callframe_push(struct callframe_s *cf);
struct  callframe_s *callframe_pop(void);
void    callframe_popf(void);
int     get_callframe_count(void);

/* builtins/dirstack.c */

int     init_dirstack(void);
void    save_dirstack(char *__path);
int     load_dirstackp(char *val);
int     load_dirstack(char *__path);
struct  dirstack_ent_s *get_dirstack_entryn(int n, struct dirstack_ent_s **__prev);
struct  dirstack_ent_s *get_dirstack_entry(char *nstr, struct dirstack_ent_s **__prev);
void    purge_dirstack(int flags);
char   *purge_dirstackp(void);
int     dirs_builtin(int argc, char **argv);
int     pushd_builtin(int argc, char **argv);
int     popd_builtin(int argc, char **argv);

extern  int read_dirsfile;
extern  int stack_count;

/**********************************
 * some builtin utility extensions
 **********************************/
int     dump_builtin(int argc, char **argv);
int     ver_builtin(int argc, char **argv);
//int    halt(void);
int     help_builtin(int argc, char **argv);
//int    reboot(void);
//int    print_system_date(void);
int     mailcheck_builtin(int argc, char **argv);
int     check_for_mail(void);
int     test_builtin(int argc, char **argv);
int     echo_builtin(int argc, char **argv);
int     glob_builtin(int argc, char **argv);
int     printenv_builtin(int argc, char **argv);
int     setenv_builtin(int argc, char **argv);
int     unsetenv_builtin(int argc, char **argv);
int     let_builtin(int argc, char **argv);
int     whence_builtin(int argc, char **argv);
int     enable_builtin(int argc, char **argv);
int     bugreport_builtin(int argc, char **argv);
int     stop_builtin(int argc, char **argv);
int     unlimit_builtin(int argc, char **argv);
void    do_echo(int v, int argc, char **argv, int flags);

/******************************************/
/* B- Special Builtins                    */
/******************************************/

/* builtins/colon.c */
int     colon_builtin(int argc, char **argv);

/* builtins/coproc.c */
int     coproc_builtin(int argc, char **argv, struct io_file_s *io_files);

/* builtins/dot.c */
int     dot_builtin(int argc, char **argv);

/* builtins/eval.c */
int     eval_builtin(int argc, char **argv);

/* builtins/exec.c */
int     exec_builtin(int argc, char **argv);

/* builtins/exit.c */
int     exit_builtin(int argc, char **argv);
void    exit_gracefully(int stat, char *errmsg);
extern  int tried_exit;

/* builtins/export.c */
void    print_quoted_val(char *val);
void    purge_quoted(char *prefix, char *name, char *val);
int     export_builtin(int argc, char **argv);
int     is_list_terminator(char *c);
void    do_export_vars(int force_export_all);
void    do_export_table(struct symtab_s *symtab, int force_export_all);

/* builtins/hash.c */
extern  struct hashtab_s *utility_hashtable;
void    init_utility_hashtable(void);
int     hash_utility(char *utility, char *path);
void    unhash_utility(char *utility);
char   *get_hashed_path(char *utility);
int     hash_builtin(int argc, char **argv);

/* builtins/readonly.c */
int     readonly_builtin(int argc, char **argv);

/* builtins/return.c */
int     return_builtin(int argc, char **argv);

/* builtins/set.c */
int     option_set(char which);
void    reset_non_posix_options(void);
void    set_option(char option, int set);
int     do_options(char *ops, char *extra);
int     set_builtin(int argc, char **argv);
int     do_set(char *name_buf, char *val_buf, int set_global, int set_flags, int unset_flags);
void    symtab_save_options(void);
char    short_option(char *long_option);
int     is_short_option(char which);

/* builtins/shift.c */
int     shift_builtin(int argc, char **argv);

/* builtins/source.c */
int     source_builtin(int argc, char **argv);

/* builtins/times.c */
void    start_clock(void);
int     times_builtin(int argc, char *argv[]);

/* builtins/time.c */
#include "parser/node.h"
int     time_builtin(struct source_s *src, struct node_s *cmd);
double  get_cur_time(void);

/* builtins/trap.c */
extern int executing_trap;
void   init_traps(void);
void   reset_nonignored_traps(void);
int    trap_builtin(int argc, char **argv);
void   trap_handler(int signum);
struct trap_item_s *save_trap(char *name);
void   restore_trap(char *name, struct trap_item_s *saved);
struct trap_item_s *get_trap_item(char *trap);
void   block_traps(void);
void   unblock_traps(void);
void   do_pending_traps(void);

/* builtins/unset.c */
int    unset_builtin(int argc, char **argv);

/* builtins/local.c */
int    local_builtin(int argc, char **argv);

/* builtins/declare.c */
int    declare_builtin(int argc, char **argv);
int    do_declare_var(char *arg, int set_global, int set_flags, int unset_flags, int UTILITY);
void   purge_vars(int flags);

/* builtins/logout.c */
int    logout_builtin(int argc, char **argv);

/* builtins/memusage.c */
int    memusage_builtin(int argc, char **argv);

/* builtins/suspend.c */
int    suspend_builtin(int argc, char **argv);

/* builtins/repeat.c */
int    repeat_builtin(int argc, char **argv);

/*
 * builtin utility index (for use when accessing
 * the struct builtin_s arrays).
 */
enum regular_builtin_index_e
{
    REGULAR_BUILTIN_TEST  = 0,          /* POSIX */
    REGULAR_BUILTIN_TEST2    ,          /* POSIX */
    REGULAR_BUILTIN_ALIAS    ,          /* POSIX */
    REGULAR_BUILTIN_BG       ,          /* POSIX */
    REGULAR_BUILTIN_BUGREPORT,          /* non-POSIX extension */
    REGULAR_BUILTIN_BUILTIN  ,          /* non-POSIX extension */
    REGULAR_BUILTIN_CALLER   ,          /* non-POSIX extension */
    REGULAR_BUILTIN_CD       ,          /* POSIX */
    REGULAR_BUILTIN_COMMAND  ,          /* POSIX */
    REGULAR_BUILTIN_COPROC   ,          /* non-POSIX extension */
    REGULAR_BUILTIN_DECLARE  ,          /* non-POSIX extension */
    REGULAR_BUILTIN_DIRS     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_DISOWN   ,          /* non-POSIX extension */
    REGULAR_BUILTIN_DUMP     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_ECHO     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_ENABLE   ,          /* non-POSIX extension */
    REGULAR_BUILTIN_FALSE    ,          /* POSIX */
    REGULAR_BUILTIN_FC       ,          /* POSIX */
    REGULAR_BUILTIN_FG       ,          /* POSIX */
    REGULAR_BUILTIN_GETOPTS  ,          /* POSIX */
    REGULAR_BUILTIN_GLOB     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_HASH     ,          /* POSIX */
    REGULAR_BUILTIN_HELP     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_HISTORY  ,          /* non-POSIX extension */
    REGULAR_BUILTIN_HUP      ,          /* non-POSIX extension */
    REGULAR_BUILTIN_JOBS     ,          /* POSIX */
    REGULAR_BUILTIN_KILL     ,          /* POSIX */
    REGULAR_BUILTIN_LET      ,          /* non-POSIX extension */
    REGULAR_BUILTIN_MAIL     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_MEMUSAGE ,          /* non-POSIX extension */
    REGULAR_BUILTIN_NEWGRP   ,          /* POSIX */
    REGULAR_BUILTIN_NICE     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_NOHUP    ,          /* non-POSIX extension */
    REGULAR_BUILTIN_NOTIFY   ,          /* non-POSIX extension */
    REGULAR_BUILTIN_POPD     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_PRINTENV ,          /* non-POSIX extension */
    REGULAR_BUILTIN_PUSHD    ,          /* non-POSIX extension */
    REGULAR_BUILTIN_PWD      ,          /* POSIX */
    REGULAR_BUILTIN_READ     ,          /* POSIX */
    REGULAR_BUILTIN_SETENV   ,          /* non-POSIX extension */
    REGULAR_BUILTIN_STOP     ,          /* non-POSIX extension */
    REGULAR_BUILTIN_TEST3    ,          /* POSIX */
    REGULAR_BUILTIN_TRUE     ,          /* POSIX */
    REGULAR_BUILTIN_TYPE     ,          /* POSIX */
    REGULAR_BUILTIN_ULIMIT   ,          /* POSIX */
    REGULAR_BUILTIN_UMASK    ,          /* POSIX */
    REGULAR_BUILTIN_UNALIAS  ,          /* POSIX */
    REGULAR_BUILTIN_UNLIMIT  ,          /* non-POSIX extension */
    REGULAR_BUILTIN_UNSETENV ,          /* non-POSIX extension */
    REGULAR_BUILTIN_VER      ,          /* non-POSIX extension */
    REGULAR_BUILTIN_WAIT     ,          /* POSIX */
    REGULAR_BUILTIN_WHENCE   ,          /* non-POSIX extension */
};

enum special_builtin_index_e
{
    SPECIAL_BUILTIN_BREAK    ,          /* POSIX */
    SPECIAL_BUILTIN_COLON    ,          /* POSIX */
    SPECIAL_BUILTIN_CONTINUE ,          /* POSIX */
    SPECIAL_BUILTIN_DOT      ,          /* POSIX */
    SPECIAL_BUILTIN_EVAL     ,          /* POSIX */
    SPECIAL_BUILTIN_EXEC     ,          /* POSIX */
    SPECIAL_BUILTIN_EXIT     ,          /* POSIX */
    SPECIAL_BUILTIN_EXPORT   ,          /* POSIX */
    SPECIAL_BUILTIN_LOCAL    ,          /* non-POSIX extension */
    SPECIAL_BUILTIN_LOGOUT   ,          /* non-POSIX extension */
    SPECIAL_BUILTIN_READONLY ,          /* POSIX */
    SPECIAL_BUILTIN_REPEAT   ,          /* non-POSIX extension */
    SPECIAL_BUILTIN_RETURN   ,          /* POSIX */
    SPECIAL_BUILTIN_SET      ,          /* POSIX */
    SPECIAL_BUILTIN_SETX     ,          /* non-POSIX extension */
    SPECIAL_BUILTIN_SHIFT    ,          /* POSIX */
    SPECIAL_BUILTIN_SOURCE   ,          /* POSIX */
    SPECIAL_BUILTIN_SUSPEND  ,          /* non-POSIX extension */
    SPECIAL_BUILTIN_TIMES    ,          /* POSIX */
    //SPECIAL_BUILTIN_TIME     ,          /* POSIX */
    SPECIAL_BUILTIN_TRAP     ,          /* POSIX */
    SPECIAL_BUILTIN_UNSET    ,          /* POSIX */
};

#endif
