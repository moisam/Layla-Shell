/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
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

/* constants used by parse_io_file() and parse_io_here() */
#define IO_FILE_LESS            1
#define IO_FILE_LESSAND         2
#define IO_FILE_LESSGREAT       3
#define IO_FILE_CLOBBER         4
#define IO_FILE_GREAT           5
#define IO_FILE_GREATAND        6
#define IO_FILE_AND_GREAT_GREAT 7
#define IO_FILE_DGREAT          8
// #define IO_HERE_DLESS       8
// #define IO_HERE_DLESSDASH   9
#define IO_HERE_EXPAND          9
#define IO_HERE_NOEXPAND        10


struct node_s *parse_complete_command(struct token_s *tok);
struct node_s *parse_list(struct token_s *tok);
struct node_s *parse_and_or(struct token_s *tok);
struct node_s *parse_pipeline(struct token_s *tok);
struct node_s *parse_pipe_sequence(struct token_s *tok);
void           parse_separator(struct token_s *tok);
struct node_s *parse_term(struct token_s *tok, enum token_type stop_at);
struct node_s *parse_compound_list(struct token_s *tok, enum token_type stop_at);
struct node_s *parse_subshell(struct token_s *tok);
struct node_s *get_wordlist(struct token_s *tok);
struct node_s *parse_do_group(struct token_s *tok);
struct node_s *parse_for_clause2(struct token_s *tok);
struct node_s *parse_for_clause(struct token_s *tok);
struct node_s *parse_case_item(struct token_s *tok);
struct node_s *parse_case_clause(struct token_s *tok);
struct node_s *parse_if_clause(struct token_s *tok);
struct node_s *parse_while_clause(struct token_s *tok);
struct node_s *parse_until_clause(struct token_s *tok);
struct node_s *parse_brace_group(struct token_s *tok);
struct node_s *parse_compound_command(struct token_s *tok);
struct node_s *parse_io_file(struct token_s *tok);
struct node_s *parse_io_here(struct token_s *tok);
struct node_s *parse_io_redirect(struct token_s *tok);
struct node_s *parse_redirect_list(struct token_s *tok);
struct node_s *parse_function_body(struct token_s *tok);
struct node_s *parse_function_definition(struct token_s *tok, int using_keyword);
struct node_s *parse_simple_command(struct token_s *tok);
struct node_s *parse_command(struct token_s *tok);
struct node_s *parse_translation_unit();
char          *__parse_alias(char *cmd);
struct node_s *io_file_node(int fd, char type, char *namestr, int lineno);

extern struct symtab_entry_s *current_func;

#endif
