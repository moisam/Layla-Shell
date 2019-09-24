/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: backend.h
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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <glob.h>
#include "../cmd.h"
#include "../parser/parser.h"
#include "../symtab/symtab.h"
#include "../parser/node.h"

struct io_file_s
{
    char *path;
    int   fileno;
    int   duplicates;
    int   flags;
};

int   do_exec_cmd(int argc, char **argv, char *use_path, int (*internal_cmd)(int, char **));
pid_t fork_child();

int   do_complete_command(struct node_s *node);
int   do_list(struct node_s *node, struct node_s *redirect_list);
int   do_and_or(struct node_s *node, struct node_s *redirect_list, int fg);
int   do_pipeline(struct node_s *node, struct node_s *redirect_list, int wait);
int   do_pipe_sequence(struct node_s *node, struct node_s *redirect_list, int wait);
void  do_separator(struct node_s *node);
int   do_term(struct node_s *node, struct node_s *redirect_list);
int   do_compound_list(struct node_s *node, struct node_s *redirect_list);
int   do_subshell(struct node_s *node, struct node_s *redirect_list);
// int  get_wordlist(struct node_s *node);
int   do_do_group(struct node_s *node, struct node_s *redirect_list);
int   do_for_clause2(struct node_s *node, struct node_s *redirect_list);
int   do_for_clause(struct node_s *node, struct node_s *redirect_list);
int   do_select_clause(struct node_s *node, struct node_s *redirect_list);
int   do_case_item(struct node_s *node, char *word, struct node_s *redirect_list);
int   do_case_clause(struct node_s *node, struct node_s *redirect_list);
int   do_if_clause(struct node_s *node, struct node_s *redirect_list);
int   do_while_clause(struct node_s *node, struct node_s *redirect_list);
int   do_until_clause(struct node_s *node, struct node_s *redirect_list);
int   do_brace_group(struct node_s *node, struct node_s *redirect_list);
int   do_compound_command(struct node_s *node, struct node_s *redirect_list);
int   do_function_body(struct node_s *node);
int   do_function_definition(int argc, char **argv);
int   do_simple_command(struct node_s *node, struct node_s *redirect_list, int fork);
int   do_command(struct node_s *node, struct node_s *redirect_list, int fork);
int   do_translation_unit(struct node_s *node);
char *__tok_to_str(struct word_s *tok);
char **__make_list(struct word_s *first_tok, int *token_count);
void  asynchronous_prologue();
void  inc_subshell_var();

int   do_special_builtin(int argc, char **argv);
int   do_regular_builtin(int argc, char **argv);

int   __break(int argc, char **argv);
int   __continue(int argc, char **argv);
// void SIGCHLD_handler(int signum);

/* pattern.c */
int   match_filename(char *pattern, char *str, int print_err, int ignore);
// int  __match_pattern(char *pattern, char *str);
char **filename_expand(char *dir, char *path, glob_t *matches);
int   match_prefix(char *pattern, char *str, int longest);
int   match_suffix(char *pattern, char *str, int longest);
int   has_regex_chars(char *p, size_t len);
int   match_ignore(char *pattern, char *filename);

void  save_std(int fd);
void  restore_std();

extern int do_restore_std;
/* redirect.c */
int   redirect_prep_node(struct node_s *child, struct io_file_s *io_files);
int   redirect_prep(struct node_s *node, struct io_file_s *io_files);
int   redirect_do(struct node_s *redirect_list);
void  redirect_restore();
char *redirect_proc(char op, char *cmdline);
int   do_io_file(struct node_s *node, struct io_file_s *io_file);
int   do_io_here(struct node_s *node, struct io_file_s *io_file);
int   do_io_redirect(struct node_s *node, struct io_file_s *io_file);
//int   do_redirect_list(struct node_s *node, struct io_file_s *io_file);

/* coprocess file descriptors */
extern int rfiledes[];
extern int wfiledes[];


#define ERR_TRAP_OR_EXIT()                          \
do {                                                \
    if(!res || exit_status)                         \
    {                                               \
        trap_handler(ERR_TRAP_NUM);                 \
        if(option_set('e'))                         \
            exit_gracefully(EXIT_FAILURE, NULL);    \
    }                                               \
} while(0)


#endif
