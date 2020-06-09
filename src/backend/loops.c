/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: loops.c
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

/* Macro definitions needed to use sig*() and setenv() */
#define _POSIX_C_SOURCE 200112L
/* For uslepp(), but also _POSIX_C_SOURCE shouldn't be >= 200809L */
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
#include "../builtins/builtins.h"
#include "../builtins/setx.h"


int cur_loop_level = 0;     /* Current loop level (number of nested loops) */
int req_break      = 0;     /* If set, break was encountered in a loop */
int req_continue   = 0;     /* If set, continue was encountered in a loop */


/*
 * This function handles the break builtin utility, which is used to break out
 * of loops (for, while, until). An optional argument (argv[1]) can be used to
 * specify how many levels (loops) to break out from. the default is 1.
 * 
 * Returns 0, unless an error occurs (such as an invalid argument being passed,
 * or 'break' being called outside a loop). in case of error, 1 is returned.
 */
int break_builtin(int argc, char **argv)
{
    if(!cur_loop_level)
    {
        PRINT_ERROR("%s: break clause outside a loop\n", SOURCE_NAME);
        return 1;
    }
    
    if(argc == 1)
    {
        req_break = 1;
    }
    else
    {
        char *strend = NULL;
        int n = strtol(argv[1], &strend, 10);
        if(*strend || n < 1)
        {
            PRINT_ERROR("break: invalid loop count: %s\n", argv[1]);
            return 1;
        }
        req_break = n;
    }
    return 0;
}


/*
 * This function handles the continue builtin utility, which is used to continue
 * execution of loops (for, while, until) from the top of the loop.
 * An optional argument (argv[1]) can be used to specify how many levels (loops) to
 * break out from before continuing the last loop from the top. the default is 1.
 * 
 * Returns 0, unless an error occurs (such as an invalid argument being passed,
 * or 'continue' being called outside a loop). in case of error, 1 is returned.
 */
int continue_builtin(int argc, char **argv)
{
    if(!cur_loop_level)
    {
        PRINT_ERROR("%s: continue clause outside a loop\n", SOURCE_NAME);
        return 1;
    }

    if(argc == 1)
    {
        req_continue = 1;
    }
    else
    {
        char *strend = NULL;
        int n = strtol(argv[1], &strend, 10);
        if(*strend || n < 1)
        {
            PRINT_ERROR("continue: invalid loop count: %s\n", argv[1]);
            return 1;
        }
        req_continue = n;
    }
    return 0;
}


/*
 * Extract and return the word list that is used as part of the
 * for and select loops. If no *nodelist is provided, we use the value
 * of the $@ special parameter, which contains the current values of the
 * positional parameters.
 *
 * Returns the string array on success, NULL if there's not enough memory
 * to store the words, or if the resultant word list is empty.
 */
struct word_s *get_loop_wordlist(struct node_s *nodelist)
{
    struct word_s *w, *cur, *prev, *head = NULL, *tail = NULL;
    
    if(nodelist)
    {
        nodelist = nodelist->first_child;
        while(nodelist)
        {
            if((w = make_word(nodelist->val.str)) == NULL)
            {
                free_all_words(head);
                PRINT_ERROR("%s: insufficient memory for loop's wordlist\n", 
                            SOURCE_NAME);
                return NULL;
            }

            if(head)
            {
                tail->next = w;
            }
            else
            {
                head = w;
            }
            
            tail = w;
            nodelist = nodelist->next_sibling;
        }
    }
    else
    {
        /* Use the actual arguments to the script (i.e. "$@") */
        int count = get_shell_vari("#", 0);
        int i = 1;
        char buf[32];
        
        if(!count)
        {
            return NULL;
        }

        while(i <= count)
        {
            sprintf(buf, "%d", i);
            char *p2 = get_shell_varp(buf, "");
            
            if((w = make_word(p2)) == NULL)
            {
                free_all_words(head);
                PRINT_ERROR("%s: insufficient memory for loop's wordlist\n", 
                            SOURCE_NAME);
                return NULL;
            }

            if(head)
            {
                tail->next = w;
            }
            else
            {
                head = w;
            }
            
            tail = w;
            i++;
        }
    }
    
    /* Now go POSIX-style on those tokens */
    cur  = head;
    tail = head;
    prev = NULL;

    while(cur)
    {
        /* Then, word expansion */
        w = word_expand(cur->data,
              FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES|FLAG_FIELD_SPLITTING);

        /* Null? remove this token from list */
        if(!w)
        {
            w = cur->next;
            /* Free this word struct */
            free(cur->data);
            free(cur);
            
            /* And remove it from the list */
            if(prev)
            {
                prev->next = w;
            }
            else
            {
                head = w;
            }
            cur = w;
        }
        /* New sublist is formed (i.e. field splitting resulted in more than one field) */
        else
        {
            struct word_s *next = cur->next;
            /* Free this word struct */
            if(w != cur)
            {
                free(cur->data);
                free(cur);
            }
            
            /* And remove it from the list */
            if(prev)
            {
                prev->next = w;
            }
            else
            {
                head = w;
            }

            /* Find the last word */
            tail = w;
            while(tail->next)
            {
                tail = tail->next;
            }
            
            /* And adjust the pointers */
            tail->next = next;
            prev = tail;
            cur = next;
        }
    }

    return head;
}


/* 
 * Execute the second form of 'for' loops, the arithmetic for loop:
 * 
 *    for((expr1; expr2; expr3)); do commands; done
 * 
 * This is a non-POSIX extension used by all major shells.
 * 
 * Returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_for_loop2(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }

    struct node_s *expr1 = node->first_child;
    if(!expr1 || expr1->type != NODE_ARITHMETIC_EXPR)
    {
        return 0;
    }
    
    struct node_s *expr2 = expr1->next_sibling;
    if(!expr2 || expr2->type != NODE_ARITHMETIC_EXPR)
    {
        return 0;
    }
    
    struct node_s *expr3 = expr2->next_sibling;
    if(!expr3 || expr3->type != NODE_ARITHMETIC_EXPR)
    {
        return 0;
    }
    
    struct node_s *commands = expr3->next_sibling;
    if(!commands)
    {
        set_internal_exit_status(0);
        return 1;
    }
    
    /* Redirects specific to the loop should override global ones */
    int saved_fd[3] = { -1, -1, -1 };
    struct node_s *local_redirects = commands ? commands->next_sibling : NULL;
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }

    if(redirect_list)
    {
        if(!redirect_prep_and_do(redirect_list, saved_fd))
        {
            return 0;
        }
    }

    /* First evaluate expr1 */
    char *str = expr1->val.str;
    char *str2;
    if(str && *str)
    {
        /*
         * The DEBUG trap is executed (bash):
         * - before each simple command
         * - before each for, case, select, and arithmetic for commands
         * - before the first command in a shell function
         */
        trap_handler(DEBUG_TRAP_NUM);    
        
        str2 = arithm_expand(str);
        if(!str2)     /* invalid expr */
        {
            if(redirect_list)
            {
                restore_stds(saved_fd);
            }
            return 0;
        }
        free(str2);
    }

    /* Then loop as long as expr2 evaluates to non-zero result */
    int res = 0;
    char *onestr = "1";
    cur_loop_level++;

    while(1)
    {
        str = expr2->val.str;
        if(str && *str)
        {
            trap_handler(DEBUG_TRAP_NUM);    
            str2 = arithm_expand(str);
            if(!str2)       /* Invalid expr */
            {
                res = 0;
                break;
            }
            
            if(!*str2)      /* Empty expression, treat as 1 */
            {
                str2 = onestr;
            }
        }
        else                /* Treat it as 1 and loop */
        {
            str2 = onestr;
        }

        /*
         * Perform the loop body (we don't need to call strtol(), because we
         * know arithm_expand() only returns a string-formatted number.
         */
        if(atol(str2))
        {
            if(!do_do_group(src, commands, NULL))
            {
                res = 0;
                break;
            }
            
            if(return_set || signal_received == SIGINT)
            {
                break;
            }
            
            if(req_break)
            {
                req_break--;
                break;
            }
            
            if(req_continue)
            {
                if(--req_continue)
                {
                    break;
                }
            }
            
            if(str2 != onestr)
            {
                free(str2);
            }
            res = 1;
            
            /* Evaluate expr3 */
            str = expr3->val.str;
            if(str && *str)
            {
                trap_handler(DEBUG_TRAP_NUM);    
                str2 = arithm_expand(str);
                if(!str2)     /* Invalid expr */
                {
                    res = 0;
                    break;
                }
                free(str2);
            }
        }
        else
        {
            res = 1;
            break;
        }
    }
    cur_loop_level--;

    if(redirect_list)
    {
        restore_stds(saved_fd);
    }
    return res;
}


/* 
 * Execute the first (classic) form of 'for' loops, which is defined by POSIX:
 * 
 *    for var [in word-list]; do commands; done
 * 
 * Returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_for_loop(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    
    struct node_s *index = node->first_child;
    if(index->type == NODE_ARITHMETIC_EXPR)
    {
        return do_for_loop2(src, node, redirect_list);
    }

    struct node_s *wordlist = index->next_sibling;
    if(wordlist->type != NODE_WORDLIST)
    {
        wordlist = NULL;
    }
    
    struct node_s *commands = wordlist ? wordlist->next_sibling : index->next_sibling;
    if(!commands)
    {
        set_internal_exit_status(0);
        return 1;
    }

    /* Redirects specific to the loop should override global ones */
    int saved_fd[3] = { -1, -1, -1 };
    struct node_s *local_redirects = commands ? commands->next_sibling : NULL;
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }

    if(redirect_list)
    {
        if(!redirect_prep_and_do(redirect_list, saved_fd))
        {
            return 0;
        }
    }

    struct word_s *list = get_loop_wordlist(wordlist);
    if(!list)
    {
        set_internal_exit_status(0);
        if(redirect_list)
        {
            restore_stds(saved_fd);
        }
        return 1;
    }
    
    /* We should now be set at the first command inside the for loop */
    int res = 0;
    char *index_name = index->val.str;
    struct word_s *l = list;

    /* Get our index variable's symbol table entry */
    struct symtab_entry_s *entry = get_symtab_entry(index_name);
    if(!entry)
    {
        entry = add_to_symtab(index_name);
    }
    
    /* Check we're not trying to assign to a readonly variable */
    if(flag_set(entry->flags, FLAG_READONLY))
    {
        READONLY_ASSIGN_ERROR(SOURCE_NAME, index_name, "variable");
        /* Set l to NULL so we won't enter the loop below */
        l = NULL;
        res = 0;
    }
    else
    {
        /*
         * We set FLAG_CMD_EXPORT so that the index var will be exported to all commands
         * inside the for loop.
         */
        symtab_entry_setval(entry, NULL);
        entry->flags |= FLAG_CMD_EXPORT;
    }
    
    cur_loop_level++;

    /*
     * The DEBUG trap is executed (bash):
     * - before each simple command
     * - before each for, case, select, and arithmetic for commands
     * - before the first command in a shell function
     */
    trap_handler(DEBUG_TRAP_NUM);    
    
    for( ; l; l = l->next)
    {
        symtab_entry_setval(entry, l->data);
        res = do_do_group(src, commands, NULL);

        if(!res || return_set || signal_received == SIGINT)
        {
            break;
        }
        
        if(req_break)
        {
            req_break--;
            break;
        }
        
        if(req_continue)
        {
            if(--req_continue)
            {
                break;
            }
        }
        //res = 1;
    }

    /* Free used memory */
    free_all_words(list);
    cur_loop_level--;

    if(redirect_list)
    {
        restore_stds(saved_fd);
    }
    return res;
}


/* 
 * Execute a select clause (or loop), which takes the form of:
 * 
 *    select name [in word-list]; do commands; done
 * 
 * This is a non-POSIX extension used by all major shells. You should notice the
 * similarity between this function's code and the do_for_loop() function above.
 * 
 * Returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_select_loop(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }

    int saved_fd[3] = { -1, -1, -1 };
    struct node_s *index    = node->first_child;
    struct node_s *wordlist = index->next_sibling;
    if(wordlist->type != NODE_WORDLIST)
    {
        wordlist = NULL;
    }
    
    struct node_s *commands = wordlist ? wordlist->next_sibling : index->next_sibling;
    if(!commands)
    {
        set_internal_exit_status(0);
        return 1;
    }

    /* Redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? commands->next_sibling : NULL;
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }

    if(redirect_list)
    {
        if(!redirect_prep_and_do(redirect_list, saved_fd))
        {
            return 0;
        }
    }
    
    struct word_s *list = get_loop_wordlist(wordlist);
    
    if(!list)
    {
        set_internal_exit_status(0);
        if(redirect_list)
        {
            restore_stds(saved_fd);
        }
        return 1;
    }
    
    /* We should now be set at the first command inside the for loop */
    char *index_name = index->val.str;
    int j, res = 0, count = 0;
    struct word_s *l = list;

    if(do_set(index_name, NULL, 0, 0, 0) == NULL)
    {
        if(redirect_list)
        {
            restore_stds(saved_fd);
        }
        return 0;
    }

    cur_loop_level++;
    
    /*
     * The DEBUG trap is executed (bash):
     * - before each simple command
     * - before each for, case, select, and arithmetic for commands
     * - before the first command in a shell function
     */
    trap_handler(DEBUG_TRAP_NUM);    

    for(j = 0, l = list; l; j++, l = l->next)
    {
        fprintf(stderr, "%d\t%s\n", j+1, list->data);
        count++;
    }
    
    for( ; ; )
    {
        print_prompt3();
        if(do_builtin_internal(read_builtin, 2, (char *[]){ "read", "REPLY" }) != 0)
        {
            res = 0;
            fprintf(stderr, "\n\n");
            break;
        }

        /* Parse the selection */
        char *strend;
        struct symtab_entry_s *entry = get_symtab_entry("REPLY");
        
        /*
         * Empty response. ksh prints PS3, while bash and zsh reprint the select list.
         * the second approach seems more appropriate, so we follow it.
         */
        if(!entry->val || !entry->val[0])
        {
            for(j = 0, l = list; l; j++, l = l->next)
            {
                fprintf(stderr, "%d\t%s\n", j+1, list->data);
            }
            continue;
        }
        
        int sel = strtol(entry->val, &strend, 10);
        /* Invalid (non-numeric) response */
        if(strend == entry->val)
        {
            symtab_entry_setval(entry, NULL);
            continue;
        }
        
        if(sel < 1 || sel > count)
        {
            symtab_entry_setval(entry, NULL);
            continue;
        }
        
        j = sel-1;
        for(j = sel-1, l = list; j > 0; j--, l = l->next)
        {
            ;
        }
        
        if(do_set(index_name, l->data, 0, 0, 0) == NULL)
        {
            break;
        }
        
        if(!do_do_group(src, commands, NULL))
        {
            res = 0;
            break;
        }
        
        if(return_set || signal_received == SIGINT)
        {
            break;
        }
        
        if(req_break)
        {
            req_break--;
            break;
        }
        
        if(req_continue)
        {
            if(--req_continue)
            {
                break;
            }
        }
        res = 1;
        
        /* If var is null, reprint the list */
        entry = get_symtab_entry("REPLY");
        if(!entry->val || !entry->val[0])
        {
            for(j = 0, l = list; l; j++, l = l->next)
            {
                fprintf(stderr, "%d\t%s\n", j+1, list->data);
            }
        }
    }
    
    free_all_words(list);
    cur_loop_level--;

    if(redirect_list)
    {
        restore_stds(saved_fd);
    }
    return res;
}


/* 
 * Execute a while clause (or loop), which takes the form of:
 * 
 *    while test; do commands; done
 * 
 * Returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_while_loop(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }

    int saved_fd[3] = { -1, -1, -1 };
    struct node_s *clause   = node->first_child;
    struct node_s *commands = clause->next_sibling;
    
    /* Redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? commands->next_sibling : NULL;
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }
    
    if(redirect_list)
    {
        if(!redirect_prep_and_do(redirect_list, saved_fd))
        {
            return 0;
        }
    }
    
    int res = 0;
    cur_loop_level++;
    do
    {
        in_test_clause = 1;
        if(!do_compound_list(src, clause, NULL))
        {
            res = 0;
            in_test_clause = 0;
            break;
        }
        in_test_clause = 0;
        
        if(exit_status == 0)
        {
            if(!do_do_group(src, commands, NULL))
            {
                res = 0;
                break;
            }
            
            if(return_set || signal_received == SIGINT)
            {
                break;
            }
            
            if(req_break)
            {
                req_break--;
                break;
            }
            
            if(req_continue)
            {
                if(--req_continue)
                {
                    break;
                }
            }
            res = 1;
        }
        else
        {
            res = 1;
            break;
        }
    } while(1);
    cur_loop_level--;

    if(redirect_list)
    {
        restore_stds(saved_fd);
    }
    return res;
}


/* 
 * Execute an until clause (or loop), which takes the form of:
 * 
 *    until test; do commands; done
 * 
 * Returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int do_until_loop(struct source_s *src, struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }

    int saved_fd[3] = { -1, -1, -1 };
    struct node_s *clause   = node->first_child;
    struct node_s *commands = clause->next_sibling;
    /* Redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? commands->next_sibling : NULL;
    if(local_redirects)
    {
        redirect_list = local_redirects;
    }
    
    if(redirect_list)
    {
        if(!redirect_prep_and_do(redirect_list, saved_fd))
        {
            return 0;
        }
    }
    
    int res = 0;
    cur_loop_level++;
    do
    {
        in_test_clause = 1;
        if(!do_compound_list(src, clause, NULL))
        {
            res = 0;
            in_test_clause = 0;
            break;
        }
        in_test_clause = 0;
        
        if(exit_status == 0)
        {
            res = 1;
            break;
        }
        else
        {
            if(!do_do_group(src, commands, NULL))
            {
                res = 0;
                break;
            }
            
            if(return_set || signal_received == SIGINT)
            {
                break;
            }
            
            if(req_break)
            {
                req_break--;
                break;
            }
            
            if(req_continue)
            {
                if(--req_continue)
                {
                    break;
                }
            }
            res = 1;
        }
    } while(1);
    cur_loop_level--;

    if(redirect_list)
    {
        restore_stds(saved_fd);
    }
    return res;
}
