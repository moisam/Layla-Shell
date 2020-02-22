/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: trap.c
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

/* required macro definition for sig*() functions */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include "builtins.h"
#include "../cmd.h"
#include "../sig.h"
#include "../symtab/string_hash.h"
#include "../backend/backend.h"
#include "../debug.h"

#define UTILITY             "trap"

/*
 * flag is set when we are executing a trap, to prevent exit() et al. from
 * recuresively calling traps.. the value of this flag equals the signal
 * number plus 1, so that EXIT will set this flag to 1, SIGHUP to 2, etc.
 */
int executing_trap = 0;

/*
 * bitmap containing pending traps that result from receiving signals while
 * the shell is waiting for a foreground job, or a background job through wait().
 */
long pending_traps = 0;

/*
 * we only have 32 traps, so we don't need to implement a hashtable, as a
 * linear array should do fine with such a small size of data.
 */
struct trap_item_s trap_table[TRAP_COUNT];

/*
 * pointers to the 'special' trap names and their table entries.
 */
struct special_trap_s
{
    char *name;
    struct trap_item_s *trap;
}
special_traps[] =
{
    { "EXIT"  , &trap_table[0              ] },
    { "ERR"   , &trap_table[ERR_TRAP_NUM   ] },
    { "CHLD"  , &trap_table[CHLD_TRAP_NUM  ] },
    { "DEBUG" , &trap_table[DEBUG_TRAP_NUM ] },
    { "RETURN", &trap_table[RETURN_TRAP_NUM] },
};

int special_traps_count = sizeof(special_traps)/sizeof(struct special_trap_s);

/* defined in main.c */
void SIGINT_handler(int signum);
void SIGWINCH_handler(int signum);
void SIGCHLD_handler(int signum);
void SIGHUP_handler(int signum);
void SIGQUIT_handler(int signum);


/*
 * initialize traps by setting the default action for each trap.
 * called on shell startup.
 */
void init_traps(void)
{
    int i = 0;
    for( ; i < TRAP_COUNT; i++)
    {
        trap_table[i].action     = ACTION_DEFAULT;
        trap_table[i].action_str = NULL          ;
    }
}


/*
 * return the requested trap, resetting the trap action to the default action.
 */
struct trap_item_s *save_trap(char *name)
{
    /* get the trap struct */
    struct trap_item_s *trap = get_trap_item(name);
    if(!trap)
    {
        return NULL;
    }
    
    /* get a copy of the trap */
    struct trap_item_s *trap2 = malloc(sizeof(struct trap_item_s));
    if(!trap2)
    {
        return NULL;
    }
    memcpy(trap2, trap, sizeof(struct trap_item_s));
    
    /* reset trap to default action */
    trap->action = ACTION_DEFAULT;
    trap->action_str = NULL;
    
    /* return the copy */
    return trap2;
}


/*
 * restore a previously saved trap, freeing the saved trap's memory.
 */
void restore_trap(char *name, struct trap_item_s *saved)
{
    if(!saved)
    {
        return;
    }
    
    /* get the trap item */
    struct trap_item_s *trap = get_trap_item(name);
    if(!trap)
    {
        return;
    }
    
    /* free the old action string */
    if(trap->action_str)
    {
        free_malloced_str(trap->action_str);
    }
    
    /* set the new trap */
    memcpy(trap, saved, sizeof(struct trap_item_s));
    free(saved);
}


/*
 * execute the trap corresponding to the given signal number.
 *
 * this is the function that gets called when a trap condition occurs, i.e. when
 * a signal is received or when the shell is exiting.
 */
void trap_handler(int signum)
{
    struct trap_item_s *trap = &trap_table[signum];
    if(trap->action != ACTION_EXECUTE)
    {
        return;
    }

    /* prevent recursive execution of traps */
    if(executing_trap == signum+1)
    {
        return;
    }
    
    /* execute the trap command */
    if(trap->action_str)
    {
        /* if waiting for a child process, store the trap for later execution */
        if(waiting_pid)
        {
            pending_traps |= (1 << (signum-1));
            return;
        }
        
        executing_trap = signum+1;
        /*
         * POSIX says the action argument shall be processed in a manner equivalent to
         * us calling:
         *
         *       eval action
         */
        char *argv[] = { "eval", trap->action_str, NULL };
        do_builtin_internal(eval_builtin, 2, argv);
        executing_trap = 0;
    }
}


/*
 * execute any pending traps.
 */
void do_pending_traps(void)
{
    if(!pending_traps)
    {
        return;
    }
    
    long i, j;
    for(i = 1, j = 1; i < SIGNAL_COUNT; i++, j <<= 1)
    {
        if((pending_traps & j) == j)
        {
            trap_handler(i);
            pending_traps &= ~j;
        }
    }
}


/*
 * return the trap struct corresponding to the trap name given in the 'trap'
 * parameter.
 *
 * returns sthe trap struct, or NULL if 'trap' is an invalid trap name.
 */
struct trap_item_s *get_trap_item(char *trap)
{
    /* invalid signal name */
    if(!trap)
    {
        return NULL;
    }

    /* check the special traps first */
    int i;
    for(i = 0; i < special_traps_count; i++)
    {
        if(strcasecmp(special_traps[i].name, trap) == 0)
        {
            return special_traps[i].trap;
        }
    }
    
    /* signal traps */
    for(i = 1; i < SIGNAL_COUNT; i++)
    {
        if(strcasecmp(signames[i], trap) == 0)
        {
            return &trap_table[i];
        }
    }
    
    /* trap not found */
    return NULL;
}


/*
 * check for signals with alternate names, with or without
 * a SIG prefix.. these alternate names are:
 *    SIGPOLL = SIGIO
 *    SIGIOT  = SIGABRT
 *    SIGCLD  = SIGCHLD
 *
 * returns the "standard" name, which is the one on the right-side of the equal
 * sign in the above lines.. if signame is not one of the above, returns the
 * signame as-is.
 */
char *check_alt_name(char *signame)
{
    /* skip the SIG prefix, if any */
    if(strncasecmp(signame, "SIG", 3) == 0)
    {
        signame += 3;
    }
    else if(strcasecmp(signame, "POLL") == 0)
    {
        return "SIGIO";
    }
    else if(strcasecmp(signame, "IOT" ) == 0)
    {
        return "SIGABRT";
    }
    else if(strcasecmp(signame, "CLD" ) == 0)
    {
        return "SIGCHLD";
    }
    
    return signame;
}


/*
 * check if the given condition represents a trap condition.
 */
static char trap_name_buf[32];

int check_trap_condition(char *s, int *cond_number, char **cond_str)
{
    int i;
    char *strend;
    int len = strlen(s);
    
    /* check for numeric signal names */
    if(isdigit(*s))
    {
        i = strtol(s, &strend, 10);
        if(*strend || i < 0 || i >= SIGNAL_COUNT)
        {
            return 0;
        }
        
        if(i == 0)
        {
            s = "EXIT";
        }
        else
        {
            s = signames[i];
        }
    }
    else
    {
        s = check_alt_name(s);
        i = (strcasecmp(s, "EXIT") == 0) ? 0 :
            (strcasecmp(s, "ERR") == 0) ? ERR_TRAP_NUM :
            (strcasecmp(s, "CHLD") == 0) ? CHLD_TRAP_NUM :
            (strcasecmp(s, "DEBUG") == 0) ? DEBUG_TRAP_NUM :
            (strcasecmp(s, "RETURN") == 0) ? RETURN_TRAP_NUM : -1;

        /* if condition doesn't start with SIG, we will need to add the SIG prefix */
        if(strncasecmp(s, "SIG", 3))
        {
            /*
             * our buf above can only take 32 chars (28 + 3 for 'SIG' + NULL)
             * why not use a larger buffer? because no signal should ever have
             * a name of more than 28 chars!
             */
            if(len > 28)
            {
                return 0;
            }

            if(i == -1)
            {
                sprintf(trap_name_buf, "SIG%s", s);
                s = trap_name_buf;
            }
        }
        
        /* get the signal number corresponding to this 'condition' argument */
        if(i == -1)
        {
            for(i = 1; i < SIGNAL_COUNT; i++)
            {
                if(strcasecmp(signames[i], s) == 0)
                {
                    break;
                }
            }

            if(i == SIGNAL_COUNT)
            {
                return 0;
            }
        }
    }
    
    (*cond_str) = s;
    (*cond_number) = i;
    return 1;
}


/*
 * reset the traps that are not ignored by the shell to their default values.
 * called when we're executing an external command (from the command's child process).
 */
void reset_nonignored_traps(void)
{
    /*
     * NOTE: we don't need to reset the special traps, as this function is only
     * called when we fork a child process.. as the child process will eventually
     * exec, its memory will be overwritten and the special traps will be void
     * in all cases.
     */
    
    /* reset the 31 signals (if they are not ignored) */
    int i = 1;
    for( ; i < SIGNAL_COUNT; i++)
    {
        if(trap_table[i].action == ACTION_IGNORE)
        {
            continue;
        }
        struct sigaction *handler = get_sigaction(i);
        sigemptyset(&handler->sa_mask);
        handler->sa_flags = 0;
        sigaction(i, handler, NULL);
    }
}


/*
 * print the value of one trap.
 */
void print_one_trap(char *trap_name, struct trap_item_s *trap)
{
    switch(trap->action)
    {
        case ACTION_EXECUTE:
            printf("trap -- ");
            if(trap->action_str)
            {
                char *val = quote_val(trap->action_str, 1, 0);
                if(val)
                {
                    printf("%s", val);
                    free(val);
                }
                else
                {
                    printf("\"\"");
                }
            }
            else
            {
                printf("\"\"");
            }
            printf(" %s\n", trap_name);
            break;

        case ACTION_IGNORE:
            printf("trap -- \"\" %s\n", trap_name);
            break;
    }
}


/*
 * print the given traps, or all traps if argv[0] is NULL.
 */
int print_traps(char **argv)
{
    int i = 0;
    if(argv && *argv)
    {
        char *condition;
        while(*argv)
        {
            if(!check_trap_condition(*argv, &i, &condition))
            {
                PRINT_ERROR("%s: unknown trap condition: %s\n", UTILITY, condition);
                return 1;
            }

            /* get the trap struct for this condition */
            struct trap_item_s *trap = get_trap_item(condition);
            if(!trap)
            {
                PRINT_ERROR("%s: unknown trap condition: %s\n", UTILITY, condition);
                return 1;
            }
            
            print_one_trap(condition, trap);
            argv++;
        }
    }
    else
    {
        /* check the special traps first */
        for(i = 0; i < special_traps_count; i++)
        {
            print_one_trap(special_traps[i].name, special_traps[i].trap);
        }
    
        /* print the rest of the traps */
        for(i = 1; i < SIGNAL_COUNT; i++)
        {
            print_one_trap(signames[i], &trap_table[i]);
        }
    }
    return 0;
}


/*
 * the trap builtin utility (POSIX).. used to set, unset and print traps.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help trap` or `trap -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int trap_builtin(int argc, char **argv)
{
    /* process options and arguments */
    int i, v = 1;
    int print = 0;
    while(argv[v] && argv[v][0] == '-')
    {
        char *p = argv[v];
        /* the '-' special option */
        if(p[1] == '\0')
        {
            break;
        }
            
        /* the '--' special option */
        if(p[1] == '-' && p[2] == '\0')
        {
            v++;
            break;
        }
            
        /*
         * as POSIX doesn't define any options for the trap builtin utility,
         * we don't recognize options in the --posix mode.
         */
        if(option_set('P'))
        {
            PRINT_ERROR("%s: unknown option: %s\n", UTILITY, p);
            return 2;
        }

        switch(*++p)
        {
            case 'h':
                print_help(argv[0], &TRAP_BUILTIN, 0);
                return 0;
                    
            case 'v':
                printf("%s", shell_ver);
                return 0;
                        
            case 'l':
                printf("0\tEXIT\n");
                for(i = 1; i < SIGNAL_COUNT; i++)
                {
                    printf("%d\t%s\n", i, signames[i]+3);   /* print without the SIG prefix */
                }
                printf("32\tERR\n");
                printf("33\tCHLD\n");
                printf("34\tDEBUG\n");
                printf("35\tRETURN\n");
                return 0;
                
            case 'p':
                print = 1;
                break;
                    
            default:
                PRINT_ERROR("%s: unknown option: %c\n", UTILITY, *p);
                return 2;
        }
        v++;
    }

    /* no arguments */
    if(print || v == argc)
    {
        return print_traps(&argv[v]);
    }

    /* get the requested action */
    char *actionstr = argv[v++];
    int action = ACTION_EXECUTE;
    char *strend;
    long n;
    
    if(actionstr[0] == '\0')    /* null or "" string */
    {
        action = ACTION_IGNORE;
    }
    else if(actionstr[0] == '-' && actionstr[1] == '\0') /* the '-' string */
    {
        action = ACTION_DEFAULT;
    }
    else
    {
        n = strtol(actionstr, &strend, 10);
        if(*strend || strend == actionstr)
        {
            action = ACTION_EXECUTE;    /* non-numeric action */
        }
        else if(n >= 0)
        {
            action = ACTION_DEFAULT;    /* numeric action */
        }
        else
        {
            action = ACTION_EXECUTE;
        }
    }

    /* no arguments (we need the name of one or more traps) */
    if(v == argc)
    {
        return 0;
    }

    char *condition = NULL;
    int   res = 0;
    /* now loop through the given conditions and set their traps */
    for( ; v < argc; v++)
    {
        condition = argv[v];
        int i = 0;
        if(!check_trap_condition(condition, &i, &condition))
        {
            goto invalid_trap;
        }
        
        /* get the trap struct for this condition */
        struct trap_item_s *trap = get_trap_item(condition);
        if(!trap)
        {
            goto invalid_trap;
        }
        
        struct sigaction *default_sigact = get_sigaction(i);
        struct sigaction sigact;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        
        if(i > 0 && i < SIGNAL_COUNT)
        {
            /*
             * POSIX says signals ignored on entry to a non-interactive shell
             * cannot be trapped or reset.
             */
            if(!interactive_shell && default_sigact->sa_handler == SIG_IGN)
            {
                continue;
            }
        }
        
        /* remove the old action string and set the new one */
        if(trap->action_str)
        {
            free_malloced_str(trap->action_str);
            trap->action_str = NULL;
        }

        /* now set the trap action */
        switch(action)
        {
            /* set action to the default action */
            case ACTION_DEFAULT:
                /* handle the special traps: EXIT, ERR, DEBUG, CHILD, RETURN */
                if(i >= ERR_TRAP_NUM || i == 0)
                {
                    trap->action = ACTION_DEFAULT;
                    break;
                }

                /* restore the signal action to the default we've got from our parent process */
                sigact.sa_handler = default_sigact->sa_handler;
                sigact.sa_sigaction = default_sigact->sa_sigaction;
                if(sigaction(i, &sigact, NULL) != 0)
                {
                    PRINT_ERROR("%s: failed to reset trap to default: %s\n", UTILITY, strerror(errno));
                    res = 1;
                }
                else
                {
                    trap->action = ACTION_DEFAULT;
                }

                /*
                 * if this is an interactive shell, reset the default handler 
                 * for some important signals (bash).. this mirrors what we do 
                 * in init_signals() in sig.c.. for example we reset SIGINT, so 
                 * that when the user presses ^C it still kills the input and 
                 * prints PS1.. this happens for all except SIGHUP, which is 
                 * reset regardless of the shell's interactivity state.
                 */
                switch(i)
                {
                    case SIGINT:
                        if(interactive_shell)
                        {
                            set_signal_handler(SIGINT, SIGINT_handler);
                        }
                        break;

                    case SIGWINCH:
                        if(interactive_shell)
                        {
                            set_signal_handler(SIGWINCH, SIGWINCH_handler);
                        }
                        break;
                        
                    case SIGTSTP:
                    case SIGTTIN:
                    case SIGTTOU:
                        if(interactive_shell && option_set('m'))
                        {
                            set_signal_handler(i, SIG_IGN);
                        }
                        break;
                        
                    case SIGTERM:
                        if(interactive_shell)
                        {
                            set_signal_handler(i, SIG_IGN);
                        }
                        break;

                    case SIGQUIT:
                        set_SIGQUIT_handler();
                        break;
                        
                    case SIGALRM:
                        set_SIGALRM_handler();
                        break;

                    case SIGCHLD:
                        set_signal_handler(SIGCHLD, SIGCHLD_handler);
                        break;

                    case SIGHUP:
                        set_signal_handler(SIGHUP, SIGHUP_handler);
                        break;
                }
                break;
                
            /* set action to the ignore action */
            case ACTION_IGNORE:
                /* handle the special traps: EXIT, ERR, DEBUG, CHILD, RETURN */
                if(i >= ERR_TRAP_NUM || i == 0)
                {
                    trap->action = ACTION_IGNORE;
                    break;
                }                
                
                /* ignore the signal */
                sigact.sa_handler = SIG_IGN;
                if(sigaction(i, &sigact, NULL) != 0)
                {
                    PRINT_ERROR("%s: failed to ignore trap: %s\n", UTILITY, strerror(errno));
                    res = 1;
                }
                else
                {
                    trap->action = ACTION_IGNORE;
                }
                break;
                
            /* set action to the command execute action */
            case ACTION_EXECUTE:
                /* handle the special traps: EXIT, ERR, DEBUG, CHILD, RETURN */
                if(i >= ERR_TRAP_NUM || i == 0)
                {
                    trap->action = ACTION_EXECUTE;
                    trap->action_str = get_malloced_str(actionstr);
                    break;
                }
                
                /* set the signal handler to our trap function */
                sigact.sa_handler = trap_handler;
                if(sigaction(i, &sigact, NULL) != 0)
                {
                    PRINT_ERROR("%s: failed to set trap: %s\n", UTILITY, strerror(errno));
                    res = 1;
                }
                else
                {
                    trap->action = ACTION_EXECUTE;
                    trap->action_str = get_malloced_str(actionstr);
                }
                break;
        }
    }
    
    return res;

invalid_trap:
    PRINT_ERROR("%s: unknown trap condition: %s\n", UTILITY, condition);
    return 1;
}
