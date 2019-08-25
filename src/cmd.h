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

#ifndef CMD_H
#define CMD_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <sys/types.h>
#include "scanner/source.h"

/* constant declarations */
static const char TAB = '\t';
static const char CR  = '\r';
static const char NL  = '\n';
static inline char is_space(char c)
{
    if(c == ' ' || /* c == '\n' || */ c == '\t') return 1;
    return 0;
}

#define SHELL_NAME      "lsh"

/* process group id of the command line interpreter */
extern pid_t tty_pid;

// #define WAIT_FLAG       (WEXITED|WSTOPPED|WCONTINUED|WUNTRACED /* |WNOHANG */)
#define WAIT_FLAG       (WUNTRACED)

#define W_FLAG      (O_RDWR | O_CREAT | O_TRUNC )
#define A_FLAG      (O_RDWR | O_CREAT | O_APPEND)
#define R_FLAG      (O_RDONLY)
#define C_FLAG      (W_FLAG | O_NOCTTY) /* use O_NOCTTY as our "no-clobber" flag */
#define CLOOPEN     (-1)
#define FILE_MASK   (S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR)
#define DIR_MASK    (S_IRWXU | S_IRWXG | S_IRWXO)


#define MAX_PROCESS_PER_JOB     10
#define MAX_JOBS                255
#define MAX_TOKENS              255

#define MAX_ENV_NAME_LEN        31

#define DEFAULT_LINE_MAX        2048
#define DEFAULT_PATH_MAX        4096

/* POSIX's error exit codes */
#define EXIT_ERROR_NOENT        127
#define EXIT_ERROR_NOEXEC       126


struct _stream
{
    struct cmd_token *path;
#define _ATTRIB_WRITE   1
#define _ATTRIB_APPEND  2
#define _ATTRIB_READ    3
#define _ATTRIB_PIPE    4
#define _ATTRIB_HEREDOC 5
    char attributes;
    int  flags;
};

/* job structure */
struct job
{
    int     job_num;
    int     proc_count;
    int     pgid;
    int     status;
    char   *commandstr;
    pid_t  *pids;
    int    *exit_codes;
    int     child_exits;
    long    child_exitbits;
#define JOB_FLAG_FORGROUND      (1 << 0)
#define JOB_FLAG_DISOWNED       (1 << 1)
#define JOB_FLAG_NOTIFIED       (1 << 2)
#define JOB_FLAG_NOTIFY         (1 << 3)    /* set by the notify builtin to notify individual jobs */
    int     flags;
    struct  termios *tty_attr;              /* terminal state when job is suspended */
};
// extern char job_run_status[MAX_JOBS];
#define flag_set(flags, which)      (((flags) & (which)) == (which))
#define NOT_RUNNING(status)         (WIFEXITED((status)) || WIFSIGNALED((status)) || WIFSTOPPED((status)))

/* tokens per input string */
struct cmd_token
{
    char *data;
    int len;
#define PLAIN_TOKEN                 0
#define SINGLY_QUOTED_TOKEN         1
#define DOUBLY_QUOTED_TOKEN         2
#define BACKTICKED_TOKEN            3
#define COMMAND_SUBSTITUTE_TOKEN    4
#define PARAMETER_EXPANSION_TOKEN   5
#define ARITHMETIC_EXPANSION_TOKEN  6
#define OPERATOR_TOKEN              7
#define HEREDOC_TOKEN_EXP           8   /* expandable heredoc */
#define HEREDOC_TOKEN_NOEXP         9   /* non-expandable heredoc */
    char token_type;
    struct cmd_token *next;
};

/* now for the "automatic" internal shell variables like $? and $1 ... */
struct shell_var_s
{
    char name[MAX_ENV_NAME_LEN+1];
    char str_value[12];
    char *large_str_value;
};


/* prompt.c */
extern  char prompt[];
char   *__evaluate_prompt(char *PS);
void    evaluate_prompt();
void    print_prompt();
void    print_prompt2();
void    print_prompt3();
void    print_prompt4();

/* popen.c */
FILE   *popenr(char *cmd);

/* initsh.c */
extern  int  null_environ_index;
extern  int  startup_finished;
void    initsh(int argc, char **argv, int init_tty);
void    init_login();
void    init_rc();
char    parse_options(int argc, char **argv);

/* cmdline.c */
extern  int       terminal_row, terminal_col;
extern  int       VGA_WIDTH, VGA_HEIGHT;
extern  char     *cmdbuf      ;           /* ptr to buffer */
extern  uint16_t  cmdbuf_index;           /* whrere to add incoming key */
extern  uint16_t  cmdbuf_end  ;           /* index of last entered key */
extern  uint16_t  cmdbuf_size ;           /* actual malloc'd size */
// #define          CMD_BUF_SIZE	 ((LINE_MAX)-1)
extern  timer_t   timerid     ;
void    cmdline();
int     ext_cmdbuf();
char   *read_cmd();
int     is_incomplete_cmd();
size_t  glue_cmd_pieces();

/* strbuf.c */
void    init_str_hashtable();
char   *__get_malloced_str(char *str);
char   *get_malloced_str(char *str);
void    free_malloced_str(char *str);

/* helpfunc.c */
int     beep();
int     strupper(char *str);
int     strlower(char *str);
void    _itoa(char *str, int num);
void    strcat_c(char *str, int pos, char chr);
int     get_screen_size();
void    move_cur(int row, int col);
void    clear_screen();
void    set_terminal_color(int FG, int BG);
void    update_row_col();
int     get_terminal_row();
int     get_terminal_col();
int     set_terminal_col(int col);
int     set_terminal_row(int row);
size_t  remove_escaped_newlines(char *buf);
void    term_canon(int on);
char   *get_malloced_strl(char *str, int start, int length);
int     is_same_str(char *s1, char *s2);
struct  cmd_token *make_cmd_token(char *word);
char   *search_path(char *file, char *use_path, int exe_only);
int     is_function(char *cmd);
int     is_builtin(char *cmd);
int     is_enabled_builtin(char *cmd);
int     is_special_builtin(char *cmd);
int     is_regular_builtin(char *cmd);
void    save_signals();
void    reset_signals();
int     fork_command(int argc, char **argv, char *use_path, char *UTILITY, int flags, int flagarg);
int     isroot();
char   *strchr_any(char *string, char *chars);
char   *list_to_str(char **list, int dofree);
char   *quote_val(char *val);
int     echoon(int fd);
int     echooff(int fd);
int     file_exists(char *path);
int     get_linemax();
char   *get_tmp_filename();
char   *get_shell_varp(char *name, char *def_val);
int     get_shell_vari(char *name, int def_val);
long    get_shell_varl(char *name, int def_val);
void    set_shell_varp(char *name, char *val);

/* flags for fork_command() */
#define FORK_COMMAND_DONICE     (1 << 0)
#define FORK_COMMAND_IGNORE_HUP (1 << 1)

/* cmd_args.c */
int     is_reserved_word(char *str);
int     is_restrict_var(char *name);
void    free_all_tokens(struct cmd_token *first);
size_t  __get_next_token(char *str, struct cmd_token *t);
// int     tokenize(char *str, struct cmd_token *tok);
void    __do_alias(struct cmd_token *tok);
void    do_alias_substitution(struct cmd_token *tok);
char   *__do_command(char *cmd, int backquoted);
char   *__do_arithmetic(char *__expr);
char   *__do_var(char *__var_name, struct cmd_token **tokens);
char   *__substitute(char *s, char *val, size_t start, size_t end);
void    __substitute_var(struct cmd_token *tok, char *val, size_t start, size_t end);
size_t  find_closing_quote(char *data, int sq_nesting);
size_t  find_closing_brace(char *data);
char   *tilde_expand(char *s, size_t *_i, int in_var_assign);
// int     __word_expand(struct cmd_token **_head, struct cmd_token ***_tail,
//                       size_t *_i, size_t *_j, size_t *_len,
//                       char cmd, char in_double_quotes);
void    delete_char_at(char *str, size_t index);
void    purge_tokens(struct cmd_token *tok);
struct  cmd_token *word_expand(struct cmd_token *head, struct cmd_token **tail, int in_heredoc, int strip_quotes);
struct  cmd_token *__make_fields(char *str);
struct  cmd_token *make_head_tail_tokens(struct cmd_token *tok, struct cmd_token *fld, 
                                        size_t len, size_t i, size_t j);
char   *word_expand_to_str(char *word);
char   *get_all_vars(char *prefix);
char    get_xdigit(char c);

/* braceexp.c */
char  **brace_expand(char *str, int *count);

/* tab.c */
int do_tab(char *cmdbuf, uint16_t *cmdbuf_index, uint16_t *cmdbuf_end);

/* args.c */
extern char **shell_argv;
extern int    shell_argc;
extern char *__optarg   ;
extern char  __opterr   ;
extern int     argi     ;
#define INVALID_OPTARG      ((char *)-1)
//char **make_argv_no_argc(struct cmd_token *tokens);
char **make_argv(int argc, struct cmd_token *tokens);
char **make_argv2(int argc, char **argv);
int    parse_args(int __argc, char **__argv, char *__ops, int *__argi, int errexit);

/* params.c */
extern char **pos_params;
extern int    pos_params_count;
// extern char  *null_param;
extern int    exit_status;
// extern struct cmd_token null_param_token;
extern int    subshell_level;

void   init_shell_vars(char *pw_name, gid_t gid, char *fullpath);
int    is_pos_param(char *var_name);
int    is_special_param(char *var_name);
struct cmd_token *get_all_pos_params(char which, int quoted);
struct cmd_token *get_pos_params(char which, int quoted, int offset, int count);
int    pos_param_count();
char **get_pos_paramsp();
void   set_pos_paramsp(char **p);
struct symtab_entry_s *get_pos_param(int i);

/* main.c */
// extern FILE  *STDIN ;
// extern FILE  *STDOUT;
// extern FILE  *STDERR;
extern struct source_s  __src;
extern struct source_s *src;
extern int    SIGINT_received;
int    do_cmd(/* int new_symtab */);
int    read_file(char *filename, struct source_s *src);

/* functab.c */
#include "symtab/symtab.h"
extern struct symtab_s *func_table;
void   init_functab();
struct symtab_entry_s *get_func(char *name);
struct symtab_entry_s *add_func(char *name);
void   unset_func(char *name);

/* vars.c */
struct var_s
{
    char *name;
    char *val ;
};

void   init_rand();
char  *get_special_var(char *name);
int    set_special_var(char *name, char *val);

extern int special_var_count;
extern struct var_s special_vars[];


/* builtins/history.c */
#define  default_HISTSIZE       512
#define  MAX_CMD_HISTORY        512
struct histent_s
{
    char   *cmd;
    time_t  time;
};
// extern   char *cmd_history[];
extern   struct histent_s cmd_history[];
extern   int   cmd_history_index;
extern   int   cmd_history_end;
extern   int   hist_file_count;
extern   int   HISTSIZE;
// #define  CLEAR_ALL	        (-1)
extern   char hist_file[];
// int      get_history_count();
// FILE    *get_history_file();
// uint32_t get_cmd_history(int index);
char    *get_last_cmd_history();
void     clear_history(int start, int end);
// void     add_to_history_file(char *cmd_buf, size_t len);
char    *save_to_history(char *cmd_buf);
void     flush_history();
void     init_history();
void     remove_newest();
int      history(int argc, char **argv);

/* builtins/hist_expand.c */
#define INVALID_HIST_EXPAND     ((char *)-1)

char    *hist_expand(int quotes);


#define COL_WHITE       37
#define COL_GREEN       32
#define COL_RED         31
#define COL_BGBLACK     40
#define COL_DEFAULT     0

/******************************************/
/* BUILTIN UTILITIES                      */
/******************************************/

struct builtin_s
{
    char *name;
    /*
     * helps us when comparing builtin function names,
     * so that we don't need to call strlen() over and over.
     */
    int   namelen;
    char *explanation;
    void *func;
    /*
     * fields used by the help utility to display
     * information about each builtin utility.
     */
    int   synopsis_name_count;
    char *synopsis;
    char *help;
#define BUILTIN_PRINT_VOPTION   (1 << 0)
#define BUILTIN_PRINT_HOPTION   (1 << 1)
#define BUILTIN_ENABLED         (1 << 2)
    unsigned char flags;
};

/* builtins/builtins.c */
extern int      regular_builtin_count;
// extern char    *regular_builtin_explanation[];
// extern char    *regular_builtin_name[];
// extern void    *regular_builtin_func[];
extern struct builtin_s regular_builtins[];
extern int      special_builtin_count;
// extern char    *special_builtin_explanation[];
// extern char    *special_builtin_name[];
// extern void    *special_builtin_func[];
extern struct builtin_s special_builtins[];
int    builtin(int argc, char *argv[]);

/* builtins/help.c */
extern char shell_ver[];
// void   print_help(char *utility, char *invokation_name);
void   print_help(char *invokation_name, int index, int isreg, int flags);

/******************************************/
/* A- Regular Builtins                    */
/******************************************/

/* builtins/alias.c */
#define MAX_ALIASES         256
struct alias_s
{
    char *name;
    char *val;
};
extern struct alias_s __aliases[MAX_ALIASES];
extern char   *null_alias;
int    alias(int argc, char *argv[]);
void   init_aliases();
int    valid_alias_name(char *name);
void   run_alias_cmd(char *alias);

/* builtins/bg.c */
int    bg(int argc, char **argv);

/* builtins/cd.c */
extern char *cwd;
int    cd(int argc, char *argv[]);
char  *get_home();

/* builtins/command.c */
int    command(int argc, char *argv[]);
int    search_and_exec(int cargc, char **cargv, char *PATH, int flags);

/* flags for the search_and_exec() function */
#define SEARCH_AND_EXEC_DOFORK  (1 << 0)        /* fork for external commands */
#define SEARCH_AND_EXEC_DOFUNC  (1 << 1)        /* do function search */

/* builtins/false.c */
int    false(int argc, char **argv);

/* builtins/fc.c */
int    fc(int argc, char **argv);

/* builtins/fg.c */
int    fg(int argc, char **argv);

/* builtins/getopts.c */
int    getopts(int argc, char **argv);

/* jobs.c */
struct job *get_job_by_pid(pid_t pgid);
struct job *get_job_by_jobid(int n);
struct job *get_job_by_any_pid(pid_t pid);
//struct job *add_job(pid_t pgid, char *commandstr);
struct job *add_job(pid_t pgid, pid_t pids[], int pid_count, char *commandstr, int is_bg);
pid_t *get_malloced_pids(pid_t pids[], int pid_count);
int    kill_job(struct job *j);
int    start_job(struct job *myjob);
struct cmd_token *get_heredoc(char *_cmd, int strip);
void   check_on_children();
void   notice_termination(pid_t pid, int status);
int    set_cur_job(struct job *job);
int    jobs(int argc, char **argv);
int    get_jobid(char *jobid_str);
int    get_total_jobs();
void   set_exit_status(int status, int domacros);
void   set_job_exit_status(struct job *job, pid_t pid, int status);
void   set_pid_exit_status(struct job *job, pid_t pid, int status);
int    pending_jobs();
void   kill_all_jobs(int signum, int flag);

/* builtins/kill.c */
int    __kill(int argc, char *argv[]);

/* builtins/newgrp.c */
int    newgrp(int argc, char *argv[]);

/* builtins/pwd.c */
int    pwd(int argc, char **argv);

/* builtins/read.c */
int    __read(int argc, char **argv);
int    ready_to_read(int fd);

/* builtins/true.c */
int    true(int argc, char **argv);

/* builtins/type.c */
int    type(int argc, char **argv);

/* builtins/umask.c */
void   init_umask();
int    __umask(int argc, char **argv);

/* builtins/ulimit.c */
int    __ulimit(int argc, char **argv);

/* builtins/unalias.c */
int    unalias(int argc, char **argv);
void   unalias_all();

/* builtins/wait.c */
int    __wait(int argc, char **argv);

/* builtins/disown.c */
extern int disown_all;
int    disown(int argc, char **argv);

/* builtins/nice.c */
int    __nice(int argc, char **argv);

/* builtins/hup.c */
int    hup(int argc, char **argv);

/* builtins/notify.c */
int    notify(int argc, char **argv);

/* builtins/caller.c */
struct callframe_s
{
    char  *funcname;
    char  *srcfile ;
    int    lineno  ;
    struct callframe_s *prev;
};

int    caller(int argc, char **argv);
struct callframe_s *callframe_new(char *funcname, char *srcfile, int lineno);
int    callframe_push(struct callframe_s *cf);
struct callframe_s *callframe_pop();
void   callframe_popf();
int    get_callframe_count();

/* builtins/dirstack.c */
#define FLAG_DIRSTACK_SEPARATE_LINES        (1 << 0)    /* print each entry on a separate line */
#define FLAG_DIRSTACK_FULL_PATHS            (1 << 1)    /* don't contract the home dir to ~ */
#define FLAG_DIRSTACK_PRINT_INDEX           (1 << 2)    /* print the entry's index */
#define FLAG_DIRSTACK_WRAP_ENTRIES          (1 << 3)    /* wrap entries before they reach screen's edge -- tcsh */

#define DIRSTACK_FILE                       "~/.lshdirs"

struct dirstack_ent_s
{
    char *path;
    struct dirstack_ent_s *next, *prev;
};

int    init_dirstack();
void   save_dirstack(char *__path);
int    load_dirstackp(char *val);
int    load_dirstack(char *__path);
struct dirstack_ent_s *get_dirstack_entryn(int n, struct dirstack_ent_s **__prev);
struct dirstack_ent_s *get_dirstack_entry(char *nstr, struct dirstack_ent_s **__prev);
void   purge_dirstack(int flags);
char  *purge_dirstackp();
int    dirs(int argc, char **argv);
int    pushd(int argc, char **argv);
int    popd(int argc, char **argv);

extern int read_dirsfile;
extern int stack_count;

/**********************************
 * some builtin utility extensions
 **********************************/
int    dump();
int    ver();
//int    halt();
int    help(int argc, char **argv);
//int    reboot();
//int    print_system_date();
int    mail(int argc, char **argv);
int    check_for_mail();
int    test(int argc, char **argv);
int    echo(int argc, char **argv);
int    __glob(int argc, char **argv);
int    printenv(int argc, char **argv);
int    __setenv(int argc, char **argv);
int    __unsetenv(int argc, char **argv);
int    let(int argc, char **argv);
int    whence(int argc, char **argv);
int    enable(int argc, char **argv);
int    bugreport(int argc, char **argv);
int    stop(int argc, char **argv);
int    unlimit(int argc, char **argv);

/* flags for the do_echo() function */
#define FLAG_ECHO_ALLOW_ESCAPED     (1 << 0)
#define FLAG_ECHO_NULL_TERM         (1 << 1)
void   do_echo(int v, int argc, char **argv, int flags);

/******************************************/
/* B- Special Builtins                    */
/******************************************/

/* builtins/colon.c */
int    colon();

/* builtins/dot.c */
extern char *dot_filename;
int    dot(int argc, char **argv);

/* builtins/eval.c */
int    eval(int argc, char **argv);

/* builtins/exec.c */
int    exec(int argc, char **argv);

/* builtins/exit.c */
int    __exit(int argc, char **argv);
void   exit_gracefully(int stat, char *errmsg);
extern int tried_exit;

/* builtins/export.c */
int    is_capital(char c);
int    is_legitimate(char c);
int    is_shell_var(char *var);
void   purge_quoted_val(char *val);
void   purge_quoted(char *prefix, char *name, char *val);
int    export(int argc, char *argv[]);
int    set(int argc, char *argv[]);
int    is_list_terminator(char *c);
void   do_export_vars();

/* builtins/hash.c */
extern struct hashtab_s *utility_hashes;
void   init_utility_hashtable();
int    hash_utility(char *utility, char *path);
void   unhash_utility(char *utility);
char  *get_hashed_path(char *utility);
int    hash(int argc, char **argv);

/* builtins/readonly.c */
int    readonly(int argc, char **argv);

/* builtins/return.c */
extern int return_set;
int    __return(int argc, char **argv);

/* builtins/set.c */
int    option_set(char which);
void   reset_options();
void   set_option(char option, int set);
int    do_options(char *ops, char *extra);
int    set(int argc, char *argv[]);
int    __set(char *name_buf, char *val_buf, int set_global, int set_flags, int unset_flags);
void   symtab_save_options();
char   short_option(char *long_option);
int    is_short_option(char which);

/* builtins/shift.c */
int    shift(int argc, char *argv[]);

/* builtins/source.c */
int    source(int argc, char **argv);

/* builtins/times.c */
void   start_clock(void);
int    __times(int argc, char *argv[]);

/* builtins/time.c */
/*
 * NOTE: we removed time from this builtins list because it is now recognized as a reserved word.
 */
//int    __time(int argc, char *argv[]);
#include "parser/node.h"
int    __time(struct node_s *cmd);
double get_cur_time();

/* builtins/trap.c */
struct trap_item_s
{
    int   action    ;
    char *action_str;
};

#define ACTION_DEFAULT  1
#define ACTION_EXECUTE  2
#define ACTION_IGNORE   3

#define TRAP_COUNT      36      /* 31 signals + EXIT + ERR + CHLD + DEBUG + RETURN */

extern int executing_trap;
void   init_traps();
void   reset_nonignored_traps();
int    trap(int argc, char **argv);
void   trap_handler(int signum);
void   save_traps();
void   restore_traps();
struct trap_item_s *save_trap(char *name);
void   restore_trap(char *name, struct trap_item_s *saved);
struct trap_item_s *get_trap_item(char *trap);
void   block_traps();
void   unblock_traps();

/* some special trap numbers (similar to a signal's number) */
#define ERR_TRAP_NUM    32
#define CHLD_TRAP_NUM   33
#define DEBUG_TRAP_NUM  34
#define RETURN_TRAP_NUM 35

/* builtins/unset.c */
int    unset(int argc, char *argv[]);

/* builtins/local.c */
int    local(int argc, char **argv);

/* builtins/declare.c */
int    declare(int argc, char **argv);
int    do_declare_var(char *arg, int set_global, int set_flags, int unset_flags, int UTILITY);
void   purge_vars(int flags);

/* builtins/logout.c */
int    logout(int argc, char **argv);

/* builtins/memusage.c */
int    memusage(int argc, char **argv);

/* builtins/suspend.c */
int    suspend(int argc, char **argv);

/* builtins/repeat.c */
int    repeat(int argc, char **argv);

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


/* POSIX says non-interactive shell should exit on syntax errors */
#define EXIT_IF_NONINTERACTIVE()                    \
do {                                                \
        if(!option_set('i'))                        \
            exit_gracefully(EXIT_FAILURE, NULL);    \
} while(0);

#endif
