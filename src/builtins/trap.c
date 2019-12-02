/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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
#include "../cmd.h"
#include "../symtab/string_hash.h"
#include "../sig.h"
#include "../debug.h"

#define UTILITY             "trap"

/*
 * flag is set when we are executing a trap, to prevent exit() et al. from
 * recuresively calling traps.. the value of this flag equals the signal
 * number plus 1, so that EXIT will set this flag to 1, SIGHUP to 2, etc.
 */
int executing_trap = 0;

/*
 * we only have 32 traps, so we don't need to implement a hashtable, as a
 * linear array should do fine with such a small size of data.
 */
struct trap_item_s trap_table[TRAP_COUNT], saved_table[TRAP_COUNT];

// struct sigaction trap_sigaction = { .sa_handler = trap_handler };

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
        //trap_table[i].name       = signames[i]   ;
        trap_table[i].action     = ACTION_DEFAULT;
        trap_table[i].action_str = NULL          ;
    }
    memset(saved_table, 0, sizeof(struct trap_item_s)*TRAP_COUNT);
}


/* mask we use to block and unblock traps (see below) */
sigset_t intmask;

/*
 * block trapped signals. this is useful when the shell is waiting for
 * a foreground job to complete (see page 47 of the bash manual for more
 * information).
 */
void block_traps(void)
{
    sigemptyset(&intmask);
    struct trap_item_s *trap;
    int i = 1;
    for( ; i < total_signames; i++)
    {
        trap = &trap_table[i];
        if(trap->action == ACTION_EXECUTE)
        {
            sigaddset(&intmask, i);
        }
    }
    //sigaddset(&intmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &intmask, NULL);
}


/*
 * unblock trapped signals.
 */
void unblock_traps(void)
{
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);
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
    struct trap_item_s *trap = get_trap_item(name);
    if(!trap)
    {
        return;
    }
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
        executing_trap = signum+1;
        /*
         * POSIX says the action argument shall be processed in a manner equivalent to
         * us calling:
         *
         *       eval action
         */
        char *argv[] = { "eval", trap->action_str, NULL };
        eval_builtin(2, argv);
        executing_trap = 0;
    }
    if(signum > 0 && signum < ERR_TRAP_NUM)
    {
        signal_received = 1;
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
    /* the special EXIT trap */
    if(strcasecmp("EXIT"  , trap) == 0)
    {
        return &trap_table[0];
    }
    /* the special ERR (error) trap */
    if(strcasecmp("ERR"   , trap) == 0)
    {
        return &trap_table[ERR_TRAP_NUM   ];
    }
    /* the special CHILD (child exit) trap */
    if(strcasecmp("CHLD"  , trap) == 0)
    {
        return &trap_table[CHLD_TRAP_NUM  ];
    }
    /* the special DEBUG trap */
    if(strcasecmp("DEBUG" , trap) == 0)
    {
        return &trap_table[DEBUG_TRAP_NUM ];
    }
    /* the special RETURN (from function or dot script) trap */
    if(strcasecmp("RETURN", trap) == 0)
    {
        return &trap_table[RETURN_TRAP_NUM];
    }
    int i = 1;
    /* signal traps */
    for( ; i < total_signames; i++)
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
    if(strcasecmp(signame, "POLL") == 0)
    {
        return "SIGIO";
    }
    if(strcasecmp(signame, "IOT" ) == 0)
    {
        return "SIGABRT";
    }
    if(strcasecmp(signame, "CLD" ) == 0)
    {
        return "SIGCHLD";
    }
    return signame;
}


/*
 * free the memory used to store the action string of trap #i.
 */
void __free_action_str(int i)
{
    if(trap_table[i].action_str)
    {
        free_malloced_str(trap_table[i].action_str);
        trap_table[i].action_str = NULL;
    }
}


/*
 * reset the traps that are not ignored by the shell to their default values.
 * called from a child process (or subshell) after it is forked.
 */
void reset_nonignored_traps(void)
{
    /* reset the special EXIT trap */
    if(trap_table[0].action != ACTION_IGNORE)
    {
        trap_table[0].action = ACTION_DEFAULT;
        __free_action_str(0);
    }
    /* reset the special ERR trap */
    if(trap_table[ERR_TRAP_NUM].action != ACTION_IGNORE)
    {
        trap_table[ERR_TRAP_NUM].action = ACTION_DEFAULT;
        __free_action_str(ERR_TRAP_NUM);
    }
    /* reset the special CHLD trap */
    if(trap_table[CHLD_TRAP_NUM].action != ACTION_IGNORE)
    {
        trap_table[CHLD_TRAP_NUM].action = ACTION_DEFAULT;
        __free_action_str(CHLD_TRAP_NUM);
    }
    /* reset the special DEBUG trap */
    if(trap_table[DEBUG_TRAP_NUM].action != ACTION_IGNORE)
    {
        trap_table[DEBUG_TRAP_NUM].action = ACTION_DEFAULT;
        __free_action_str(DEBUG_TRAP_NUM);
    }
    /* reset the special RETURN trap */
    if(trap_table[RETURN_TRAP_NUM].action != ACTION_IGNORE)
    {
        trap_table[RETURN_TRAP_NUM].action = ACTION_DEFAULT;
        __free_action_str(RETURN_TRAP_NUM);
    }
    /* reset the 31 signals (if they are not ignored) */
    int i = 1;
    for( ; i < total_signames; i++)
    {
        if(trap_table[i].action == ACTION_IGNORE)
        {
            continue;
        }
        struct sigaction *handler = get_sigaction(i);
        sigemptyset(&handler->sa_mask);
        handler->sa_flags = 0;
        if(sigaction(i, handler, NULL) != -1)
        {
            trap_table[i].action = ACTION_DEFAULT;
            __free_action_str(i);
        }
    }
}


/*
 * print the traps.
 */
void purge_traps(void)
{
    /* print the special EXIT trap */
    struct trap_item_s *trap = &trap_table[0];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" EXIT\n");
    }
    /* print the special ERR trap */
    trap = &trap_table[ERR_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" ERR\n");
    }
    /* print the special CHLD trap */
    trap = &trap_table[CHLD_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" CHLD\n");
    }
    /* print the special DEBUG trap */
    trap = &trap_table[DEBUG_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" DEBUG\n");
    }
    /* print the special RETURN trap */
    trap = &trap_table[RETURN_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" RETURN\n");
    }
    /* print the rest of the traps */
    int i = 1;
    for( ; i < total_signames; i++)
    {
        trap = &trap_table[i];
        if(trap->action != ACTION_EXECUTE)
        {
            continue;
        }
        if(!trap->action_str)
        {
            continue;
        }
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" %s\n", signames[i]);
        //printf("trap -- %s %s\n", trap->action_str, trap->name);
    }
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
    /* no arguments. print traps and return */
    if(argc == 1)
    {
        purge_traps();
        return 0;
    }
    
    /* process options and arguments */
    int v;
    for(v = 1; v < argc; v++)
    { 
        if(argv[v][0] == '-')
        {
            /* the '-' special option */
            if(argv[v][1] == '\0')
            {
                break;
            }
            /* the '--' special option */
            if(argv[v][1] == '-' && argv[v][2] == '\0')
            {
                v++;
                break;
            }
            /*
             * as POSIX doesn't define any options for the trap builtin utility,
             * we don't recognize options in the --posix mode.
             */
            if(!option_set('P'))
            {
                if(strcmp(argv[v], "-h") == 0)
                {
                    print_help(argv[0], SPECIAL_BUILTIN_TRAP, 0, 0);
                    return 0;
                }
                if(strcmp(argv[v], "-v") == 0)
                {
                    printf("%s", shell_ver);
                    return 0;
                }
                if(strcmp(argv[v], "-p") == 0)
                {
                    purge_traps();
                    return 0;
                }
                if(strcmp(argv[v], "-l") == 0)
                {
                    int i;
                    printf("0\tEXIT\n");
                    for(i = 1; i < total_signames; i++)
                    {
                        printf("%d\t%s\n", i, signames[i]+3);   /* print without the SIG prefix */
                    }
                    printf("32\tERR\n");
                    printf("33\tCHLD\n");
                    printf("34\tDEBUG\n");
                    printf("35\tRETURN\n");
                    return 0;
                }
            }
            fprintf(stderr, "%s: unknown option: %s\n", UTILITY, argv[v]);
            return 2;
        }
        else
        {
            break;
        }
    }

    /* no arguments */
    if(v == argc)
    {
        return 0;
    }

    /* get the requested action */
    char *actionstr = argv[v++];
    int action = ACTION_EXECUTE;
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
        char *strend;
        long n = strtol(actionstr, &strend, 10);
        if(strend == actionstr)
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
        int len = strlen(condition);
        char buf[32];
        int i = 0;
        /* we compare to 3 because the SIG prefix is 3 chars long (see below) */
        if(len < 3)
        {
            /* the "0" condition is equivalent to "EXIT" */
            if(condition[0] == '0' && condition[1] == '\0')
            {
                condition = "EXIT";
            }
            else
            {
                goto invalid_trap;
            }
        }
        else
        {
            condition   = check_alt_name(condition);
            int isexit  = !strcasecmp(condition, "EXIT"  );
            int iserr   = !strcasecmp(condition, "ERR"   );
            int ischld  = !strcasecmp(condition, "CHLD"  );
            int isdebug = !strcasecmp(condition, "DEBUG" );
            int isret   = !strcasecmp(condition, "RETURN");
            /* if condition doesn't start with SIG, we will need to add the SIG prefix */
            if(strncasecmp(condition, "SIG", 3))
            {
                /*
                 * our buf above can only take 32 chars (28 + 3 for 'SIG' + NULL)
                 * why not use a larger buffer? because no signal should ever have
                 * a name of more than 28 chars!
                 */
                if(len > 28)
                {
                    goto invalid_trap;
                }
                if(!isexit && !iserr && !ischld && !isdebug && !isret)
                {
                    sprintf(buf, "SIG%s", condition);
                    condition = buf;
                }
            }
            /* get the signal number corresponding to this 'condition' argument */
            if     (isexit )
            {
                i = 0;
            }
            else if(iserr  )
            {
                i = ERR_TRAP_NUM   ;
            }
            else if(ischld )
            {
                i = CHLD_TRAP_NUM  ;
            }
            else if(isdebug)
            {
                i = DEBUG_TRAP_NUM ;
            }
            else if(isret  )
            {
                i = RETURN_TRAP_NUM;
            }
            else
            {
                for(i = 1; i < total_signames; i++)
                {
                    if(strcasecmp(signames[i], condition) == 0)
                    {
                        break;
                    }
                }
                if(i == total_signames)
                {
                    goto invalid_trap;
                }
            }
        }

        /* get the trap struct for this condition */
        struct trap_item_s *trap = get_trap_item(condition);
        if(!trap)
        {
            goto invalid_trap;
        }
        
        struct sigaction *default_sigact = get_sigaction(i);
        struct sigaction sigact;
        if(i > 0 && i < total_signames)
        {
            /*
             * POSIX says signals ignored on entry to a non-interactive shell
             * cannot be trapped or reset.
             */
            if(!option_set('i') && default_sigact->sa_handler == SIG_IGN)
            {
                continue;
            }
            sigemptyset(&sigact.sa_mask);
            sigact.sa_flags = 0;
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
                    fprintf(stderr, "%s: failed to reset trap to default: %s\n", UTILITY, strerror(errno));
                    res = 1;
                }
                else
                {
                    trap->action = ACTION_DEFAULT;
                }
                
                /*
                 * if this is an interactive shell, reset the default handler for some important signals
                 * (this mirrors what we do in main.c). for example we reset SIGINT, so that when the user
                 * presses ^C it still kills the input and prints PS1. this happens for all except SIGHUP,
                 * which is reset regardless of the shell's interactivity state.
                 * 
                 * NOTE: any changes made to the default signal actions in here must be mirrored in main.c.
                 */
                switch(i)
                {
                    case SIGINT  :
                        if(option_set('i'))
                        {
                            sigact.sa_handler = SIGINT_handler;
                            sigaction(SIGINT, &sigact, NULL);
                        }
                        break;

                    case SIGCHLD :
                        if(option_set('i'))
                        {
                            sigact.sa_handler = SIGCHLD_handler;
                            sigaction(SIGCHLD, &sigact, NULL);
                        }
                        break;

                    case SIGWINCH:
                        if(option_set('i'))
                        {
                            sigact.sa_handler = SIGWINCH_handler;
                            sigaction(SIGWINCH, &sigact, NULL);
                        }
                        break;

                    case SIGQUIT :
                        if(option_set('q'))
                        {
                            sigact.sa_handler = SIGQUIT_handler;
                            sigaction(SIGQUIT, &sigact, NULL);
                        }
                        break;

                    case SIGHUP  :
                        sigact.sa_handler = SIGHUP_handler;
                        sigaction(SIGHUP, &sigact, NULL);
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
                    fprintf(stderr, "%s: failed to ignore trap: %s\n", UTILITY, strerror(errno));
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
                    fprintf(stderr, "%s: failed to set trap: %s\n", UTILITY, strerror(errno));
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
    fprintf(stderr, "%s: Unknown trap condition: %s\n", UTILITY, condition);
    return 1;

}
