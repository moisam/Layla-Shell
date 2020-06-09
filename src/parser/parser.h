/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: parser.h
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

#ifndef PARSER_H
#define PARSER_H

#include "../scanner/scanner.h"
#include "../symtab/symtab.h"

/* constants used by parse_file_redirect() and parse_heredoc_redirect() */
#define IO_FILE_LESS            1       /* '<' input redirection operator */
#define IO_FILE_LESSAND         2       /* '<&' input redirection operator */
#define IO_FILE_LESSGREAT       3       /* '<>' input/output redirection operator */
#define IO_FILE_CLOBBER         4       /* '>|' output redirection operator */
#define IO_FILE_GREAT           5       /* '>' output redirection operator */
#define IO_FILE_GREATAND        6       /* '>&' output redirection operator */
#define IO_FILE_AND_GREAT_GREAT 7       /* '&>>' append redirection operator */
#define IO_FILE_DGREAT          8       /* '>>' append redirection operator */
#define IO_HERE_EXPAND          9       /* expandable heredoc (without quoted heredoc word) */
#define IO_HERE_NOEXPAND        10      /* nonexpandable heredoc (with quoted heredoc word) */
#define IO_HERE_STRIP_EXPAND    11      /* strip heredoc tabs, but no expansion */
#define IO_HERE_STRIP_NOEXPAND  12      /* strip heredoc tabs, with expansion */
#define IO_HERE_STR             13      /* here-string */

/* skip optional newlines */
#define skip_newline_tokens()                                   \
while(tok->type == TOKEN_NEWLINE)                               \
{                                                               \
    tok = tokenize(tok->src);                                   \
}

/*
 *  skip optional newlines and update the source struct's wstart pointer
 *  (this one is important for compound commands, such as loops, that want
 *  to know where the current command line starts in the input source).
 */
#define skip_newline_tokens2()                                  \
while(tok->type == TOKEN_NEWLINE)                               \
{                                                               \
    tok->src->wstart = tok->src->curpos;                        \
    tok = tokenize(tok->src);                                   \
}                                                               \

/* parser functions */
struct node_s *parse_list(struct token_s *tok);
struct node_s *parse_and_or(struct token_s *tok);
struct node_s *parse_pipeline(struct token_s *tok);
struct node_s *parse_term(struct token_s *tok, enum token_type_e stop_at);
struct node_s *parse_compound_list(struct token_s *tok, enum token_type_e stop_at);
struct node_s *parse_subshell(struct token_s *tok);
struct node_s *get_wordlist(struct token_s *tok);
struct node_s *parse_do_group(struct token_s *tok);
struct node_s *parse_for_loop2(struct token_s *tok);
struct node_s *parse_for_loop(struct token_s *tok);
struct node_s *parse_select_loop(struct token_s *tok);
struct node_s *parse_case_item(struct token_s *tok);
struct node_s *parse_case_clause(struct token_s *tok);
struct node_s *parse_if_clause(struct token_s *tok);
struct node_s *parse_while_loop(struct token_s *tok);
struct node_s *parse_until_loop(struct token_s *tok);
struct node_s *parse_brace_group(struct token_s *tok);
struct node_s *parse_compound_command(struct token_s *tok);
struct node_s *parse_file_redirect(struct token_s *tok);
struct node_s *parse_heredoc_redirect(struct token_s *tok);
struct node_s *parse_io_redirect(struct token_s *tok);
int            is_redirect_op(char *str);
struct node_s *parse_redirect_list(struct token_s *tok);
struct node_s *parse_function_body(struct token_s *tok);
struct node_s *parse_function_definition(struct token_s *tok, int using_keyword);
struct node_s *parse_simple_command(struct token_s *tok);
struct node_s *parse_command(struct token_s *tok);
struct node_s *parse_translation_unit();
char          *get_alias_val(char *cmd);
struct node_s *io_file_node(int fd, char type, char *namestr, int lineno);
int            is_name(char *str);

/* functions for working with here-documents */
// struct word_s *get_heredoc(struct source_s *src, int strip, int *expand);
char          *get_heredoc(char *start, char *end, int strip);
char          *herestr_end(char *cmd);
// char          *heredoc_end(char *orig_cmd, int *expand, char **__delim, char **start, char last_char);
char          *heredoc_end(char *start, char *delim, char last_char);
char          *last_heredoc_end(char *start, int heredoc_count, char **heredoc_delims, char last_char);
int            heredoc_delim(char *orig_cmd, int *expand, char **__delim, char **__delim_end);
int            extract_heredocs(struct source_s *src, struct node_s *cmd, int heredoc_count);
int            next_cmd_word(char **start, char **end, int do_braces);

/* flag to indicate a parsing error */
extern int     parser_err;

#endif
