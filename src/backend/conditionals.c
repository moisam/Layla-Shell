/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: conditionals.c
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

/* macro definitions needed to use sig*() and setenv() */
#define _POSIX_C_SOURCE 200112L
/* for uslepp(), but also _POSIX_C_SOURCE shouldn't be >= 200809L */
#define _XOPEN_SOURCE   500

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include "backend.h"
#include "../error/error.h"
#include "../debug.h"
#include "../kbdevent.h"
#include "../builtins/setx.h"


/*
 * this function executes a case item:
 * 
 * case x in
 *  a|b) ...
 *  c|d|e) ...
 *  *) ...
 * esac
 * 
 * each of a|b, c|d|e and * is a case item. this function executes the case item by comparing
 * each pattern in the item (a and b for the first item; c, d and e for the second item). if one
 * of the patterns match, the function executes the compound list of commands asociated with
 * that item.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_case_item(struct node_s *node, char *word, struct node_s *redirect_list)
{
    /* 
     * the root node is a NODE_CASE_ITEM. we need to iterate its 
     * NODE_VAR children.
     */
    node = node->first_child;
    while(node && node->type == NODE_VAR)
    {
        char *pat_str = word_expand_to_str(node->val.str);
        if(pat_str)
        {
            if(match_filename(pat_str, word, 1, 1))
            {
                struct node_s *commands = node->next_sibling;
                while(commands && commands->type == NODE_VAR)
                {
                    commands = commands->next_sibling;
                }
                if(commands)
                {
                    int res = do_compound_list(commands, redirect_list);
                    ERR_TRAP_OR_EXIT();
                }
                free(pat_str);
                return 1;
            }
            free(pat_str);
        }
        node = node->next_sibling;
    }
    return 0;
}

/*
 * this function executes a case clause by trying to match and execute each case item,
 * in turn. if one of the patterns of a case item matched and we executed its compound
 * list of commands, further action depends on how the case item was terminated when the
 * user entered the command. a case item ending in ';&' means we should execute the 
 * compound list of the following case item, while ';;&' (bash) and ';|' (zsh) mean we
 * will search the other case items looking for matches. both of these are non-POSIX extensions.
 * the only case item terminator defined by POSIX is ';;', which we'll also handle here.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_case_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node || !node->first_child)
    {
        return 0;
    }
    struct node_s *word_node = node->first_child;
    struct node_s *item = word_node->next_sibling;

    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = word_node;
    while(local_redirects && local_redirects->type != NODE_IO_REDIRECT_LIST)
    {
        local_redirects = local_redirects->next_sibling;
    }
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }
    if(redirect_list)
    {
        if(!redirect_do(redirect_list))
        {
            return 0;
        }
    }

    /* get the word */
    char *word = word_expand_to_str(word_node->val.str);
    if(!word)
    {
        BACKEND_RAISE_ERROR(EMPTY_CASE_WORD, NULL, NULL);
        if(local_redirects)
        {
            redirect_restore();
        }
        /* POSIX says non-interactive shell should exit on syntax errors */
        EXIT_IF_NONINTERACTIVE();
        return 0;
    }
    
    while(item)
    {
        if(do_case_item(item, word, NULL /* redirect_list */))
        {
            /* check for case items ending in ';&' */
            if(item->val_type == VAL_CHR && item->val.chr == '&')
            {
                /* do the next item */
                item = item->next_sibling;
                if(!item || item->type != NODE_CASE_ITEM)
                {
                    goto fin;
                }
                item = item->first_child;
                if(!item || item->type != NODE_VAR)
                {
                    goto fin;
                }
                struct node_s *commands = item->next_sibling;
                while(commands && commands->type == NODE_VAR)
                {
                    commands = commands->next_sibling;
                }
                if(commands)
                {
                    int res = do_compound_list(commands, redirect_list);
                    ERR_TRAP_OR_EXIT();
                }
            }
            /* check for case items ending in ';;&' (or ';|') */
            else if(item->val_type == VAL_CHR && item->val.chr == ';')
            {
                /* try to match the next item */
                item = item->next_sibling;
                continue;
            }
            goto fin;
        }
        item = item->next_sibling;
    }
    set_exit_status(0, 0);
    
fin:
    free(word);
    if(local_redirects)
    {
        redirect_restore();
    }
    return 1;
}


/* 
 * execute an if clause (or conditional), which takes the form of:
 * 
 *    if test1; then commands1; elif test2; then commands2; else commands3; fi
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_if_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    struct node_s *clause = node->first_child;
    struct node_s *_then  = clause->next_sibling;
    struct node_s *_else  = NULL;
    if(_then) _else = _then->next_sibling;
    int res = 0;

    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = clause;
    while(local_redirects && local_redirects->type != NODE_IO_REDIRECT_LIST)
    {
        local_redirects = local_redirects->next_sibling;
    }
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }
    if(redirect_list)
    {
        if(!redirect_do(redirect_list))
        {
            return 0;
        }
    }

    if(!do_compound_list(clause, NULL /* redirect_list */))
    {
        if(local_redirects)
        {
            redirect_restore();
        }
        return 0;
    }
    if(exit_status == 0)
    {
        res = do_compound_list(_then, NULL /* redirect_list */);
        ERR_TRAP_OR_EXIT();
        if(local_redirects)
        {
            redirect_restore();
        }
        return res;
    }
    if(!_else)
    {
        if(local_redirects)
        {
            redirect_restore();
        }
        return 1;
    }
    if(_else->type == NODE_IF)
    {
        res = do_if_clause(_else, NULL /* redirect_list */);
        ERR_TRAP_OR_EXIT();
        if(local_redirects)
        {
            redirect_restore();
        }
        return res;
    }
    res = do_compound_list(_else, NULL /* redirect_list */);
    ERR_TRAP_OR_EXIT();
    if(local_redirects)
    {
        redirect_restore();
    }
    return res;
}
