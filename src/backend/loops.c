/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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

int cur_loop_level = 0;     /* current loop level (number of nested loops) */
int req_loop_level = 0;     /* requested loop level */
int req_break      = 0;     /* if set, break was encountered in a loop */
int req_continue   = 0;     /* if set, continue was encountered in a loop */

/*
 * this function handles the break builtin utility, which is used to break out
 * of loops (for, while, until). an optional argument (argv[1]) can be used to
 * specify how many levels (loops) to break out from. the default is 1.
 * 
 * returns 0, unless an error occurs (such as an invalid argument being passed,
 * or 'break' being called outside a loop). in case of error, 1 is returned.
 */
int __break(int argc, char **argv)
{
    if(!cur_loop_level)
    {
        BACKEND_RAISE_ERROR(BREAK_OUTSIDE_LOOP, NULL, NULL);
        /* POSIX says non-interactive shell should exit on syntax errors */
        if(!option_set('i'))
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }
        return 1;
    }
    if(argc == 1)
    {
        req_loop_level = 1;
    }
    else
    {
        int n = atoi(argv[1]);
        if(n < 1)
        {
            return 1;
        }
        req_loop_level = n;
    }
    req_break = req_loop_level;
    return 0;
}

/*
 * this function handles the continue builtin utility, which is used to continue
 * execution of loops (for, while, until) from the top of the loop.
 * an optional argument (argv[1]) can be used to specify how many levels (loops) to
 * break out from before continuing the last loop from the top. the default is 1.
 * 
 * returns 0, unless an error occurs (such as an invalid argument being passed,
 * or 'continue' being called outside a loop). in case of error, 1 is returned.
 */
int __continue(int argc, char **argv)
{
    if(!cur_loop_level)
    {
        BACKEND_RAISE_ERROR(CONTINUE_OUTSIDE_LOOP, NULL, NULL);
        /* POSIX says non-interactive shell should exit on syntax errors */
        if(!option_set('i'))
        {
            exit_gracefully(EXIT_FAILURE, NULL);
        }
        return 1;
    }
    if(argc == 1)
    {
        req_loop_level = 1;
    }
    else
    {
        int n = atoi(argv[1]);
        if(n < 1)
        {
            return 1;
        }
        req_loop_level = n;
    }
    req_continue = req_loop_level;
    return 0;
}
#define ARG_COUNT   0x1000
/*
 * peform word expansion on the list items and create a char ** array
 * from it, similar to the main() function's **argv list.
 * 
 * returns the string array on success, NULL if there's not enough memory
 * to store the string arguments, or if the word expansion results in an
 * empty list.
 */
char **__make_list(struct word_s *first_tok, int *token_count)
{
    int count = 0;
    char *tmp[ARG_COUNT];
    struct word_s *t = first_tok;
    while(t)
    {
        glob_t glob;
        char **matches = filename_expand(cwd, t->data, &glob);
        if(!matches || !matches[0])
        {
            globfree(&glob);
            if(!optionx_set(OPTION_NULL_GLOB))       /* bash extension */
            {
                tmp[count++] = get_malloced_str(t->data);
            }
            if( optionx_set(OPTION_FAIL_GLOB))       /* bash extension */
            {
                fprintf(stderr, "%s: file globbing failed for %s\n", SHELL_NAME, t->data);
                while(--count >= 0)
                {
                    free_malloced_str(tmp[count]);
                }
                return errno = ENOMEM, NULL;
            }
        }
        else
        {
            char **m = matches;
            size_t j = 0;
            do
            {
                tmp[count++] = get_malloced_str(*m);
                if(count >= ARG_COUNT)
                {
                    break;
                }
            } while(++m, ++j < glob.gl_pathc);
            globfree(&glob);
        }
        if(count >= ARG_COUNT)
        {
            break;
        }
        t = t->next;
    }
    if(!count)
    {
        return NULL;
    }
    char **argv = (char **)malloc((count+1) * sizeof(char *));
    if(!argv)
    {
        while(--count >= 0)
        {
            free_malloced_str(tmp[count]);
        }
        return errno = ENOMEM, NULL;
    }
    *token_count = count;
    argv[count] = NULL;
    while(--count >= 0)
    {
        argv[count] = tmp[count];
    }
    return argv;
}

/*
 * extract and return the word list that is used as part of the
 * for and select loops. if no *wordlist is provided, we use the value
 * of the $@ special parameter, which contains the current values of the
 * positional parameters.
 *
 * returns the string array on success, NULL if there's not enough memory
 * to store the string arguments, or if the word list is empty.
 */
char **get_word_list(struct node_s *wordlist, int *_count)
{
    struct word_s *prev = NULL;
    struct word_s *head = NULL;
    struct word_s *cur  = NULL;
    struct word_s *tail = NULL;
    int    count = 0;
    char  **list = NULL;
    *_count = 0;
    
    if(wordlist)
    {
        struct word_s tok_list_head;
        tail = &tok_list_head;
        wordlist = wordlist->first_child;
        while(wordlist)
        {
            struct word_s *w = make_word(wordlist->val.str);
            tail->next = w;
            tail       = w;
            count++;
            wordlist = wordlist->next_sibling;
        }
        head = tok_list_head.next;
    }
    else
    {
        /* use the actual arguments to the script (i.e. "$@") */
        struct symtab_entry_s *entry = get_symtab_entry("#");
        count = atoi(entry->val);
        if(count)
        {
            head = get_all_pos_params('@', 1);
        }
    }
    
    if(!head)
    {
        return NULL;
    }

    /* now do POSIX parsing on those tokens */
    cur  = head;
    tail = head;
    while(cur)
    {
        /* then, word expansion */
        struct word_s *w = word_expand(cur->data);
        /* null? remove this token from list */
        if(!w)
        {
            w = cur->next;
            free(cur->data);
            free(cur);
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
        /* new sublist is formed (i.e. field splitting resulted in more than one field) */
        else
        {
            struct word_s *next = cur->next;
            if(w != cur)
            {
                free(cur->data);
                free(cur);
            }
            if(prev)
            {
                prev->next = w;
            }
            else
            {
                head = w;
            }
            /* find the last word */
            tail = w;
            while(tail->next)
            {
                tail = tail->next;
            }
            tail->next = next;
            prev       = tail;
            cur        = next;
        }
    }
    list = __make_list(head, &count);
    free_all_words(head);
    *_count = count;
    return list;
}


/* 
 * execute the second form of 'for' loops, the arithmetic for loop:
 * 
 *    for((expr1; expr2; expr3)); do commands; done
 * 
 * this is a non-POSIX extension used by all major shells.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_for_clause2(struct node_s *node, struct node_s *redirect_list)
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
        set_exit_status(0, 0);
        return 1;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
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

    /* first evaluate expr1 */
    char *str = expr1->val.str;
    char *str2;
    if(str && *str)
    {
        str2 = arithm_expand(str);
        if(!str2)     /* invalid expr */
        {
            redirect_restore();
            return 0;
        }
        free(str2);
    }

    /* then loop as long as expr2 evaluates to non-zero result */
    int res   = 0;
    int endme = 0;
    char *onestr = "1";
    cur_loop_level++;
    while(!endme)
    {
        str = expr2->val.str;
        if(str && *str)
        {
            str2 = arithm_expand(str);
            if(!str2)       /* invalid expr */
            {
                res = 0;
                break;
            }
        }
        else                /* treat it as 1 and loop */
        {
            str2 = onestr;
        }
        /* perform the loop body */
        if(atol(str2))
        {
            if(!do_do_group(commands, NULL))
            {
                res = 1;
            }
            if(SIGINT_received)
            {
                endme = 1;
            }
            if(req_break)
            {
                req_break--, endme = 1;
            }
            if(req_continue >= 1)
            {
                if(--req_continue)
                {
                    endme = 1;
                }
            }
            if(str2 != onestr)
            {
                free(str2);
            }
            res = 1;
            if(endme)
            {
                break;
            }
            /* evaluate expr3 */
            str = expr3->val.str;
            if(str && *str)
            {
                str2 = arithm_expand(str);
                if(!str2)     /* invalid expr */
                {
                    redirect_restore();
                    return 0;
                }
                free(str2);
            }
        }
        else
        {
            res   = 1;
            endme = 1;
        }
    }
    cur_loop_level--;
    redirect_restore();
    return res;
}


/* 
 * execute the first (classic) form of 'for' loops, which is defined by POSIX:
 * 
 *    for var [in word-list]; do commands; done
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_for_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    struct node_s *index = node->first_child;
    if(index->type == NODE_ARITHMETIC_EXPR)
    {
        return do_for_clause2(node, redirect_list);
    }

    struct node_s *wordlist = index->next_sibling;
    if(wordlist->type != NODE_WORDLIST)
    {
        wordlist = NULL;
    }
    struct node_s *commands = wordlist ? 
                              wordlist->next_sibling : 
                              index->next_sibling;
    if(!commands)
    {
        set_exit_status(0, 0);
        return 1;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
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

    int    count = 0;
    char **list  = get_word_list(wordlist, &count);
    int i;
    if(!count || !list)
    {
        set_exit_status(0, 0);
        redirect_restore();
        return 1;
    }

    
    int j     = 0;
    int res   = 0;
    int endme = 0;
    /* we should now be set at the first command inside the for loop */
    char *index_name = index->val.str;
    /*
     * we set FLAG_CMD_EXPORT so that the index var will be exported to all commands
     * inside the for loop.
     */
    if(__set(index_name, NULL, 0, FLAG_CMD_EXPORT, 0) == -1)
    {
        fprintf(stderr, "%s: can't assign to readonly variable\n", SHELL_NAME);
        goto end;
    }
    cur_loop_level++;
// loop:
    for( ; j < count; j++)
    {
        if(__set(index_name, list[j], 0, 0, 0) == -1)
        {
            fprintf(stderr, "%s: can't assign to readonly variable\n", SHELL_NAME);
            res = 0;
            goto end;
        }
        if(!do_do_group(commands, NULL /* redirect_list */))
        {
            res = 1;
        }
        if(SIGINT_received)
        {
            endme = 1;
        }
        if(req_break)
        {
            req_break--, endme = 1;
        }
        if(req_continue >= 1)
        {
            if(--req_continue)
            {
                endme = 1;
            }
        }
        if(endme)
        {
            break;
        }
        res = 1;
    }
end:
    for(i = 0; i < count; i++) free_malloced_str(list[i]);
    //free(list[0]);
    free(list);
    cur_loop_level--;
    redirect_restore();
    return res;
}


/* 
 * execute a select clause (or loop), which takes the form of:
 * 
 *    select name [in word-list]; do commands; done
 * 
 * this is a non-POSIX extension used by all major shells. you should notice the
 * similarity between this function's code and the do_for_clause() function above.
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_select_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    struct node_s *index    = node->first_child;
    struct node_s *wordlist = index->next_sibling;
    if(wordlist->type != NODE_WORDLIST)
    {
        wordlist = NULL;
    }
    struct node_s *commands = wordlist ? 
                              wordlist->next_sibling : 
                              index->next_sibling;
    if(!commands)
    {
        set_exit_status(0, 0);
        return 1;
    }
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
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
    int    count = 0;
    char **list  = get_word_list(wordlist, &count);
    int i;
    if(!count || !list)
    {
        set_exit_status(0, 0);
        redirect_restore();
        return 1;
    }
    
    /* we should now be set at the first command inside the for loop */
    char *index_name = index->val.str;
    __set(index_name, NULL, 0, 0, 0);
    int j     = 0;
    int res   = 0;
    int endme = 0;
    cur_loop_level++;
    for(j = 0; j < count; j++)
    {
        fprintf(stderr, "%d\t%s\n", j+1, list[j]);
    }
    for(;;)
    {
        print_prompt3();
        if(__read(2, (char *[]){ "read", "REPLY" }) != 0)
        {
            res = 0;
            fprintf(stderr, "\n\n");
            break;
        }
        /* parse the selection */
        char *strend;
        struct symtab_entry_s *entry = get_symtab_entry("REPLY");
        /*
         * empty response. ksh prints PS3, while bash and zsh reprint the select list.
         * the second approach seems more appropriate, so we follow it.
         */
        if(!entry->val || !entry->val[0])
        {
            for(j = 0; j < count; j++)
            {
                fprintf(stderr, "%d\t%s\n", j+1, list[j]);
            }
            continue;
        }
        int sel = strtol(entry->val, &strend, 10);
        /* invalid (non-numeric) response */
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
        __set(index_name, list[sel-1], 0, 0, 0);
        if(!do_do_group(commands, NULL /* redirect_list */))
        {
            res = 1;
        }
        if(SIGINT_received)
        {
            endme = 1;
        }
        if(req_break)
        {
            req_break--, endme = 1;
        }
        if(req_continue >= 1)
        {
            if(--req_continue)
            {
                endme = 1;
            }
        }
        if(endme)
        {
            break;
        }
        res = 1;
        /* if var is null, reprint the list */
        entry = get_symtab_entry("REPLY");
        if(!entry->val || !entry->val[0])
        {
            for(j = 0; j < count; j++)
            {
                fprintf(stderr, "%d\t%s\n", j+1, list[j]);
            }
        }
    }
    for(i = 0; i < count; i++)
    {
        free_malloced_str(list[i]);
    }
    //free(list[0]);
    free(list);
    cur_loop_level--;
    redirect_restore();
    return res;
}


/* 
 * execute a while clause (or loop), which takes the form of:
 * 
 *    while test; do commands; done
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_while_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    struct node_s *clause   = node->first_child;
    struct node_s *commands = clause->next_sibling;
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
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
    int res         = 0;
    int endme       = 0;
    int first_round = 1;
    cur_loop_level++;
    
    do
    {
        if(!do_compound_list(clause, NULL /* redirect_list */))
        {
            cur_loop_level--;
            if(local_redirects)
            {
                redirect_restore();
            }
            return 0;
        }
        if(exit_status == 0)
        {
            if(!do_do_group(commands, NULL /* redirect_list */))
            {
                res = 1;
            }
            if(SIGINT_received)
            {
                endme = 1;
            }
            if(req_break)
            {
                req_break--, endme = 1;
            }
            if(req_continue >= 1)
            {
                if(--req_continue)
                {
                    endme = 1;
                }
            }
            if(endme)
            {
                break;
            }
            first_round = 0;
            res = 1;
        }
        else
        {
            if(first_round)
            {
                set_exit_status(0, 0);
            }
            cur_loop_level--;
            if(local_redirects)
            {
                redirect_restore();
            }
            return 1;
        }
    } while(1);
    cur_loop_level--;
    if(local_redirects)
    {
        redirect_restore();
    }
    return res;
}


/* 
 * execute an until clause (or loop), which takes the form of:
 * 
 *    until test; do commands; done
 * 
 * returns 1 on success, 0 on failure (see the comment before do_complete_command() for
 * the relation between this result and the exit status of the commands executed).
 */
int  do_until_clause(struct node_s *node, struct node_s *redirect_list)
{
    if(!node)
    {
        return 0;
    }
    struct node_s *clause   = node->first_child;
    struct node_s *commands = clause->next_sibling;
    /* redirects specific to the loop should override global ones */
    struct node_s *local_redirects = commands ? 
                        commands->next_sibling : NULL;
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
    int res         = 0;
    int endme       = 0;
    int first_round = 1;
    cur_loop_level++;

    do
    {
        if(!do_compound_list(clause, NULL /* redirect_list */))
        {
            cur_loop_level--;
            if(local_redirects)
            {
                redirect_restore();
            }
            return 0;
        }
        if(exit_status == 0)
        {
            if(first_round)
            {
                set_exit_status(0, 0);
            }
            cur_loop_level--;
            if(local_redirects)
            {
                redirect_restore();
            }
            return 1;
        }
        else
        {
            if(!do_do_group(commands, NULL /* redirect_list */))
            {
                res = 1;
            }
            if(SIGINT_received)
            {
                endme = 1;
            }
            if(req_break)
            {
                req_break--, endme = 1;
            }
            if(req_continue >= 1)
            {
                if(--req_continue)
                {
                    endme = 1;
                }
            }
            if(endme)
            {
                break;
            }
            first_round = 0;
            res = 1;
        }
    } while(1);
    cur_loop_level--;
    if(local_redirects)
    {
        redirect_restore();
    }
    return res;
}
