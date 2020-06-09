/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: builtins.h
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

#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdlib.h>
#include <stddef.h>         /* size_t */
#include "../symtab/symtab.h"

/* struct for builtin utilities */
struct builtin_s
{
    char   *name;         /* utility name */
    char   *explanation;  /* what does the utility do */
    void   *func;         /* function to call to execute the utility */
    /*
     * fields used by the help utility to display
     * information about each builtin utility.
     */
    char   *synopsis;             /* utility usage */
    char   *help;                 /* longer help message */
    unsigned char flags;
};

/* flags for the print_command_type() function */
#define TYPE_FLAG_PRINT_PATH        (1 << 0)
#define TYPE_FLAG_PRINT_WORD        (1 << 1)
#define TYPE_FLAG_PRINT_FUNCS       (1 << 2)
#define TYPE_FLAG_PRINT_BUILTINS    (1 << 3)
#define TYPE_FLAG_PRINT_ALL         (1 << 4)
#define TYPE_FLAG_PRINT_HASHED      (1 << 5)
#define TYPE_FLAG_PATH_ONLY         (1 << 6)

/* flags for the cd_flags argument of the do_cd() function */
#define DO_CD_WITH_POPTION          (1 << 0)
#define DO_CD_PUSH_DIRSTACK         (1 << 1)

/* error printing for the unset builtin utility */
#define UNSET_PRINT_ERROR(arg, msg) \
    PRINT_ERROR("unset: cannot unset `%s`: %s\n", arg, msg);


/* alias.c */
void    init_aliases(void);
int     valid_alias_name(char *name);
void    run_alias_cmd(char *alias);
void    unset_all_aliases(void);
int     alias_list_index(char *alias);

/* builtins.c */
extern  struct builtin_s shell_builtins[];

int     is_function(char *cmd);
struct  builtin_s *is_builtin(char *cmd);
struct  builtin_s *is_enabled_builtin(char *cmd);
struct  builtin_s *is_special_builtin(char *cmd);
struct  builtin_s *is_regular_builtin(char *cmd);
int     do_builtin(int argc, char **argv, int special_utility);
int     do_builtin_internal(int (*builtin)(int, char **), int argc, char **argv);
void    disable_nonposix_builtins(void);

/* caller.c */
struct  callframe_s *callframe_new(char *funcname, char *srcfile, int lineno);
struct  callframe_s *get_cur_callframe(void);
int     callframe_push(struct callframe_s *cf);
struct  callframe_s *callframe_pop(void);
void    callframe_popf(void);
int     get_callframe_count(void);

/* cd.c */
int     do_cd(int v, int argc, char **argv, int print_dirstack, int dirstack_flags, int cd_flags);

/* coproc.c */
void    coproc_close_fds(void);

/* declare.c */
int     do_declare(int argc, char **argv, int global);
int     purge_vars(char **args, char *utility, int funcs, int flags);

/* dirstack.c */
extern  int read_dirsfile;
extern  int stack_count;

int     init_dirstack(void);
void    save_dirstack(char *__path);
int     load_dirstackp(char *val);
int     load_dirstack(char *__path);
struct  dirstack_ent_s *get_dirstack_entryn(int n, struct dirstack_ent_s **__prev);
struct  dirstack_ent_s *get_dirstack_entry(char *nstr, struct dirstack_ent_s **__prev, int *n);
void    purge_dirstack(int flags);
char   *purge_dirstackp(void);
int     push_cwd(char *utility);

/* echo.c */
void    do_echo(int v, int argc, char **argv, int flags);

/* export.c */
int     is_list_terminator(char *c);
void    do_export_vars(int force_export_all);
void    do_export_table(struct symtab_s *symtab, int force_export_all);
void    print_var_attribs(unsigned int attr, char *var_perfix, char *func_prefix);
int     process_var_attribs(char **args, int unexport, int funcs, int flag);

/* hash.c */
extern  struct hashtab_s *utility_hashtable;
void    init_utility_hashtable(void);
int     hash_utility(char *utility, char *path);
void    unhash_utility(char *utility);
char   *get_hashed_path(char *utility);

/* help.c */
extern  char shell_ver[];

void    print_help(char *invokation_name, struct builtin_s *utility, int flags);

/* mailcheck.c */
int     check_for_mail(void);

/* read.c */
#include <sys/time.h>
int     ready_to_read(int fd, struct timeval *timeout);

/* times.c */
void    start_clock(void);

/* time.c */
double  get_cur_time(void);

/* type.c */
int     print_command_type(char *cmd, char *who, char *PATH, int flags);

/* unalias.c */
void    unalias_all(void);


/******************************************/
/* BUILTIN UTILITIES                      */
/******************************************/
int     alias_builtin(int argc, char **argv);
int     bg_builtin(int argc, char **argv);
int     bugreport_builtin(int argc, char **argv);
int     builtin_builtin(int argc, char **argv);
int     caller_builtin(int argc, char **argv);
int     cd_builtin(int argc, char **argv);
int     colon_builtin(int argc, char **argv);
int     command_builtin(int argc, char **argv);
#include "../cmd.h"             /* struct io_file_s */
//int     coproc_builtin(int argc, char **argv, struct io_file_s *io_files);
int     coproc_builtin(struct source_s *src, struct node_s *cmd, struct node_s *coproc_name);
int     declare_builtin(int argc, char **argv);
int     disown_builtin(int argc, char **argv);
int     dirs_builtin(int argc, char **argv);
int     dot_builtin(int argc, char **argv);
int     dump_builtin(int argc, char **argv);
int     echo_builtin(int argc, char **argv);
int     enable_builtin(int argc, char **argv);
int     eval_builtin(int argc, char **argv);
int     exec_builtin(int argc, char **argv);
int     exit_builtin(int argc, char **argv);
int     export_builtin(int argc, char **argv);
int     false_builtin(int argc, char **argv);
int     fc_builtin(int argc, char **argv);
int     fg_builtin(int argc, char **argv);
int     getopts_builtin(int argc, char **argv);
int     glob_builtin(int argc, char **argv);
// int    halt(void);
int     hash_builtin(int argc, char **argv);
int     help_builtin(int argc, char **argv);
int     history_builtin(int argc, char **argv);
int     hup_builtin(int argc, char **argv);
int     jobs_builtin(int argc, char **argv);
int     kill_builtin(int argc, char **argv);
int     let_builtin(int argc, char **argv);
int     local_builtin(int argc, char **argv);
int     logout_builtin(int argc, char **argv);
int     mailcheck_builtin(int argc, char **argv);
int     memusage_builtin(int argc, char **argv);
int     newgrp_builtin(int argc, char **argv);
int     nice_builtin(int argc, char **argv);
int     notify_builtin(int argc, char **argv);
// int    print_system_date(void);
int     pushd_builtin(int argc, char **argv);
int     popd_builtin(int argc, char **argv);
int     printenv_builtin(int argc, char **argv);
int     pwd_builtin(int argc, char **argv);
int     read_builtin(int argc, char **argv);
// int    reboot(void);
int     readonly_builtin(int argc, char **argv);
int     repeat_builtin(int argc, char **argv);
int     return_builtin(int argc, char **argv);
int     set_builtin(int argc, char **argv);
int     setenv_builtin(int argc, char **argv);
int     shift_builtin(int argc, char **argv);
int     source_builtin(int argc, char **argv);
int     stop_builtin(int argc, char **argv);
int     suspend_builtin(int argc, char **argv);
int     test_builtin(int argc, char **argv);
int     trap_builtin(int argc, char **argv);
int     true_builtin(int argc, char **argv);
int     type_builtin(int argc, char **argv);
int     umask_builtin(int argc, char **argv);
int     ulimit_builtin(int argc, char **argv);
int     unalias_builtin(int argc, char **argv);
int     unlimit_builtin(int argc, char **argv);
int     unset_builtin(int argc, char **argv);
int     unsetenv_builtin(int argc, char **argv);
int     ver_builtin(int argc, char **argv);
int     wait_builtin(int argc, char **argv);
int     whence_builtin(int argc, char **argv);
#include "../parser/node.h"
#include "../scanner/source.h"
int     time_builtin(struct source_s *src, struct node_s *cmd);
int     times_builtin(int argc, char *argv[]);


/*
 * In this file, we define the 'builtin' utility, which is used to display the list
 * of special and regular builtin utilities, as well as to run a builtin utility,
 * even if an external command or shell function exists with the same name.
 * We store special and regular builtins information in the following list. Each item
 * in the list contain the utility's name (and its length), a short description
 * on what the utility does, and the function we should call when the utility is invoked.
 * Additionally, and to assist the 'help' utility in printing useful help messages for
 * each utility, we store the synopsis (how to use or invoke the utility), and the number
 * of times the utility's name appears in the synopsis (so that 'help' can call printf()
 * with the right number of arguments). We then have the help message that 'help' prints
 * for the utility, and the utility's flags (is the utility enabled or not, does 'help'
 * print the default help message for the -v and -h options or not, ...).
 * 
 * See the definition of 'struct builtin_s' above.
 */

#define DOT_BUILTIN                 shell_builtins[ 0]
#define COLON_BUILTIN               shell_builtins[ 1]
#define TEST_BUILTIN                shell_builtins[ 2]
#define TEST2_BUILTIN               shell_builtins[ 3]
#define ALIAS_BUILTIN               shell_builtins[ 4]
#define BG_BUILTIN                  shell_builtins[ 5]
#define BREAK_BUILTIN               shell_builtins[ 6]
#define BUGREPORT_BUILTIN           shell_builtins[ 7]
#define BUILTIN_BUILTIN             shell_builtins[ 8]
#define CALLER_BUILTIN              shell_builtins[ 9]
#define CD_BUILTIN                  shell_builtins[10]
#define COMMAND_BUILTIN             shell_builtins[11]
#define CONTINUE_BUILTIN            shell_builtins[12]
#define DECLARE_BUILTIN             shell_builtins[13]
#define DIRS_BUILTIN                shell_builtins[14]
#define DISOWN_BUILTIN              shell_builtins[15]
#define DUMP_BUILTIN                shell_builtins[16]
#define ECHO_BUILTIN                shell_builtins[17]
#define ENABLE_BUILTIN              shell_builtins[18]
#define EVAL_BUILTIN                shell_builtins[19]
#define EXEC_BUILTIN                shell_builtins[20]
#define EXIT_BUILTIN                shell_builtins[21]
#define EXPORT_BUILTIN              shell_builtins[22]
#define FALSE_BUILTIN               shell_builtins[23]
#define FC_BUILTIN                  shell_builtins[24]
#define FG_BUILTIN                  shell_builtins[25]
#define GETOPTS_BUILTIN             shell_builtins[26]
#define GLOB_BUILTIN                shell_builtins[27]
#define HASH_BUILTIN                shell_builtins[28]
#define HELP_BUILTIN                shell_builtins[29]
#define HISTORY_BUILTIN             shell_builtins[30]
#define HUP_BUILTIN                 shell_builtins[31]
#define JOBS_BUILTIN                shell_builtins[32]
#define KILL_BUILTIN                shell_builtins[33]
#define LET_BUILTIN                 shell_builtins[34]
#define LOCAL_BUILTIN               shell_builtins[35]
#define LOGOUT_BUILTIN              shell_builtins[36]
#define MAILCHECK_BUILTIN           shell_builtins[37]
#define MEMUSAGE_BUILTIN            shell_builtins[38]
#define NEWGRP_BUILTIN              shell_builtins[39]
#define NICE_BUILTIN                shell_builtins[40]
#define NOHUP_BUILTIN               shell_builtins[41]
#define NOTIFY_BUILTIN              shell_builtins[42]
#define POPD_BUILTIN                shell_builtins[43]
#define PRINTENV_BUILTIN            shell_builtins[44]
#define PUSHD_BUILTIN               shell_builtins[45]
#define PWD_BUILTIN                 shell_builtins[46]
#define READ_BUILTIN                shell_builtins[47]
#define READONLY_BUILTIN            shell_builtins[48]
#define REPEAT_BUILTIN              shell_builtins[49]
#define RETURN_BUILTIN              shell_builtins[50]
#define SET_BUILTIN                 shell_builtins[51]
#define SETENV_BUILTIN              shell_builtins[52]
#define SETX_BUILTIN                shell_builtins[53]
#define SHIFT_BUILTIN               shell_builtins[54]
#define SHOPT_BUILTIN               shell_builtins[55]
#define SOURCE_BUILTIN              shell_builtins[56]
#define STOP_BUILTIN                shell_builtins[57]
#define SUSPEND_BUILTIN             shell_builtins[58]
#define TEST3_BUILTIN               shell_builtins[59]
#define TIMES_BUILTIN               shell_builtins[60]
#define TRAP_BUILTIN                shell_builtins[61]
#define TRUE_BUILTIN                shell_builtins[62]
#define TYPE_BUILTIN                shell_builtins[63]
#define TYPESET_BUILTIN             shell_builtins[64]
#define ULIMIT_BUILTIN              shell_builtins[65]
#define UMASK_BUILTIN               shell_builtins[66]
#define UNALIAS_BUILTIN             shell_builtins[67]
#define UNLIMIT_BUILTIN             shell_builtins[68]
#define UNSET_BUILTIN               shell_builtins[69]
#define UNSETENV_BUILTIN            shell_builtins[70]
#define VER_BUILTIN                 shell_builtins[71]
#define WAIT_BUILTIN                shell_builtins[72]
#define WHENCE_BUILTIN              shell_builtins[73]


#endif
