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
#include "../signames.h"
#include "../debug.h"

#define UTILITY             "trap"

/*
 * flag is set when we are executing a trap,
 * to prevent exit() et al. from recuresively
 * calling traps.
 */
int executing_trap = 0;

/*
 * we only have 32 traps, so we don't need to
 * implement a hashtable, as a linear array
 * should do fine with that size of data.
 */
struct trap_item_s trap_table[TRAP_COUNT], saved_table[TRAP_COUNT];

struct sigaction trap_sigaction = { .sa_handler = trap_handler };

/* defined in main.c */
void SIGINT_handler(int signum);
void SIGWINCH_handler(int signum);
void SIGCHLD_handler(int signum);
void SIGHUP_handler(int signum);
void SIGQUIT_handler(int signum);


void init_traps()
{
    int i;
    for(i = 0; i < TRAP_COUNT; i++)
    {
        //trap_table[i].name       = signames[i]   ;
        trap_table[i].action     = ACTION_DEFAULT;
        trap_table[i].action_str = NULL          ;
    }
    memset(saved_table, 0, sizeof(struct trap_item_s)*TRAP_COUNT);
}


sigset_t intmask;
/*
 * block trapped signals. this is useful when the shell is waiting for
 * a foreground job to complete.
 * see page 47 of bash manual.
 */
void block_traps()
{
    sigemptyset(&intmask);
    struct trap_item_s *trap;
    int i = 1;
    for( ; i < total_signames; i++)
    {
        trap = &trap_table[i];
        if(trap->action == ACTION_EXECUTE)
            sigaddset(&intmask, i);
    }
    //sigaddset(&intmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &intmask, NULL);
}

/*
 * unblock trapped signals.
 */
void unblock_traps()
{
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);
}

/*
 * return a copy of all the shell's traps (set and unset).
 * this copy can be used later to restore traps to their
 * current values. this is useful when we are e.g. executing
 * a function. traps are reset on entry to the function and
 * restored after the function finishes execution.
 */
void save_traps()
{
    memcpy(saved_table, trap_table, sizeof(struct trap_item_s)*TRAP_COUNT);
    int i;
    for(i = 0; i < TRAP_COUNT; i++)
    {
        if(trap_table[i].action_str)
        {
            saved_table[i].action_str = get_malloced_str(trap_table[i].action_str);
        }
    }
}

/*
 * return the requested trap, resetting the trap to default action.
 */
struct trap_item_s *save_trap(char *name)
{
    struct trap_item_s *trap = get_trap_item(name);
    if(!trap) return NULL;
    struct trap_item_s *trap2 = malloc(sizeof(struct trap_item_s));
    if(!trap2) return NULL;
    memcpy(trap2, trap, sizeof(struct trap_item_s));
    trap->action = ACTION_DEFAULT;
    trap->action_str = NULL;
    return trap2;
}

/*
 * restore a previously saved trap, freeing the saved trap's memory.
 */
void restore_trap(char *name, struct trap_item_s *saved)
{
    if(!saved) return;
    struct trap_item_s *trap = get_trap_item(name);
    if(!trap) return;
    memcpy(trap, saved, sizeof(struct trap_item_s));
    free(saved);
}

/*
 * restore saved traps.
 * WARNING: DO NOT call this function if you didn't call save_traps() 
 *          earlier! this can mess the whole trap table.
 */
void restore_traps()
{
    int i;
    for(i = 0; i < TRAP_COUNT; i++)
    {
        if(trap_table[i].action_str)
        {
            free_malloced_str(trap_table[i].action_str);
            trap_table[i].action_str = NULL;
        }
    }
    memcpy(trap_table, saved_table, sizeof(struct trap_item_s)*TRAP_COUNT);
    for(i = 0; i < TRAP_COUNT; i++)
    {
        if(saved_table[i].action_str)
        {
            trap_table[i].action_str = get_malloced_str(saved_table[i].action_str);
        }
    }
}

/*
 * this is the function that gets called when a trap
 * condition occurs, i.e. when a signal is received or
 * the shell is exiting.
 */
void trap_handler(int signum)
{
    struct trap_item_s *trap = &trap_table[signum];
    if(trap->action != ACTION_EXECUTE) return;
    if(trap->action_str)
    {
        executing_trap = 1;
        /*
         * POSIX says the action argument shall be processed in a manner equivalent to:
         *       eval action
         */
        char *argv[] = { "eval", trap->action_str, NULL };
        eval(2, argv);
        executing_trap = 0;
    }
    /* make sure we reset the signal handler so no funny business happens */
    if(signum > 0 & signum < ERR_TRAP_NUM)
    {
        signal(signum, trap_handler);
    }
}

struct trap_item_s *get_trap_item(char *trap)
{
    if(!trap) return NULL;
    if(strcasecmp("EXIT"  , trap) == 0) return &trap_table[0];
    if(strcasecmp("ERR"   , trap) == 0) return &trap_table[ERR_TRAP_NUM   ];
    if(strcasecmp("CHLD"  , trap) == 0) return &trap_table[CHLD_TRAP_NUM  ];
    if(strcasecmp("DEBUG" , trap) == 0) return &trap_table[DEBUG_TRAP_NUM ];
    if(strcasecmp("RETURN", trap) == 0) return &trap_table[RETURN_TRAP_NUM];
    int i = 1;
    for( ; i < total_signames; i++)
    {
        if(strcasecmp(signames[i], trap) == 0)
        //if(strcasecmp(trap_table[i].name, trap) == 0)
            return &trap_table[i];
    }
    return NULL;
}

/*
 * checks for signals with alternate names, with or without 
 * a SIG prefix. these alternate names are:
 *    SIGPOLL = SIGIO
 *    SIGIOT  = SIGABRT
 *    SIGCLD  = SIGCHLD
 */
char *check_alt_name(char *signame)
{
    if(strncasecmp(signame, "SIG", 3) == 0) signame += 3;
    if(!strcasecmp(signame, "POLL")) return "SIGIO"  ;
    if(!strcasecmp(signame, "IOT" )) return "SIGABRT";
    if(!strcasecmp(signame, "CLD" )) return "SIGCHLD";
    return signame;
}

void __free_action_str(int i)
{
    if(trap_table[i].action_str)
    {
        free_malloced_str(trap_table[i].action_str);
        trap_table[i].action_str = NULL;
    }
}

void reset_nonignored_traps()
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
    /* reset the 31 signals (if not ignored) */
    int i = 1;
    for( ; i < total_signames; i++)
    {
        if(trap_table[i].action == ACTION_IGNORE) continue;
        struct sigaction *handler = get_sigaction(i);
        if(signal(i, handler->sa_handler) < 0) continue;
        trap_table[i].action = ACTION_DEFAULT;
        __free_action_str(i);
    }
}

void purge_traps()
{
    /* print the special EXIT trap */
    struct trap_item_s *trap = &trap_table[0];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" EXIT\r\n");
    }
    /* print the special ERR trap */
    trap = &trap_table[ERR_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" ERR\r\n");
    }
    /* print the special CHLD trap */
    trap = &trap_table[CHLD_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" CHLD\r\n");
    }
    /* print the special DEBUG trap */
    trap = &trap_table[DEBUG_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" DEBUG\r\n");
    }
    /* print the special RETURN trap */
    trap = &trap_table[RETURN_TRAP_NUM];
    if(trap->action == ACTION_EXECUTE && trap->action_str)
    {
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" RETURN\r\n");
    }
    /* print the rest of the traps */
    int i = 1;
    for( ; i < total_signames; i++)
    {
        trap = &trap_table[i];
        if(trap->action != ACTION_EXECUTE) continue;
        if(!trap->action_str) continue;
        printf("trap -- ");
        purge_quoted_val(trap->action_str);
        printf(" %s\r\n", signames[i]);
        //printf("trap -- %s %s\r\n", trap->action_str, trap->name);
    }
}


int trap(int argc, char **argv)
{
    if(argc == 1)
    {
        purge_traps();
        return 0;
    }
    
    int v;
    for(v = 1; v < argc; v++)
    { 
        if(argv[v][0] == '-')
        {
            if(argv[v][1] == '\0') break;       /* the '-' string */
            if(argv[v][1] == '-' && argv[v][2] == '\0') { v++; break; }       /* the '--' string */
            if(strcmp(argv[v], "-h") == 0) { print_help(argv[0], SPECIAL_BUILTIN_TRAP, 0, 0); return 0; }
            if(strcmp(argv[v], "-v") == 0) { printf("%s", shell_ver); return 0; }
            if(strcmp(argv[v], "-p") == 0) { purge_traps()          ; return 0; }
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
            fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, argv[v]);
            return 2;
        }
        else break;
    }
    if(v == argc) return 0;

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
        if(strend == actionstr) action = ACTION_EXECUTE;    /* non-numeric action */
        else if(n >= 0)         action = ACTION_DEFAULT;    /* numeric action */
        else                    action = ACTION_EXECUTE;
    }
    if(v == argc) return 0;

    sigemptyset(&trap_sigaction.sa_mask);
    trap_sigaction.sa_flags = 0;

    char *condition = NULL;
    int res = 0;
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
            if(condition[0] == '0' && condition[1] == '\0') condition = "EXIT";
            else goto invalid_trap;
        }
        else
        {
            condition   = check_alt_name(condition);
            int isexit  = !strcasecmp(condition, "EXIT"  );
            int iserr   = !strcasecmp(condition, "ERR"   );
            int ischld  = !strcasecmp(condition, "CHLD"  );
            int isdebug = !strcasecmp(condition, "DEBUG" );
            int isret   = !strcasecmp(condition, "RETURN");
            /* if condition doesn't start with SIG, we will need to add it. */
            if(strncasecmp(condition, "SIG", 3))
            {
                /* our buf above can only take 32 chars (28 + 3 for 'SIG' + NULL)
                 * why not use a larger buffer? because no signal should ever have
                 * a name of more than 28 chars!
                 */
                if(len > 28) goto invalid_trap;
                if(!isexit && !iserr && !ischld && !isdebug && !isret)
                {
                    sprintf(buf, "SIG%s", condition);
                    condition = buf;
                }
            }
            /* get the signal number corresponding to this 'condition' argument */
            if     (isexit ) i = 0;
            else if(iserr  ) i = ERR_TRAP_NUM   ;
            else if(ischld ) i = CHLD_TRAP_NUM  ;
            else if(isdebug) i = DEBUG_TRAP_NUM ;
            else if(isret  ) i = RETURN_TRAP_NUM;
            else
            {
                for(i = 1; i < total_signames; i++)
                {
                    if(strcasecmp(signames[i], condition) == 0) break;
                }
                if(i == total_signames) goto invalid_trap;
            }
        }
        struct trap_item_s *trap = get_trap_item(condition);
        if(!trap) goto invalid_trap;
        
        /* 
         * POSIX says signals ignored on entry to a non-interactive shell 
         * cannot be trapped or reset.
         */
        struct sigaction *handler = get_sigaction(i);
        if(i > 0 && i < total_signames && !option_set('i') && handler->sa_handler == SIG_IGN) continue;
        
        /* remove the old action string and set the new one */
        if(trap->action_str)
        {
            free_malloced_str(trap->action_str);
            trap->action_str = NULL;
        }

        switch(action)
        {
            case ACTION_DEFAULT:
                if(i >= ERR_TRAP_NUM || i == 0)
                {
                    trap->action = ACTION_DEFAULT;
                    break;
                }
                if(signal(i, handler->sa_handler) < 0)
                {
                    fprintf(stderr, "%s: failed to reset trap to default: %s\r\n", UTILITY, strerror(errno));
                    res = 1;
                }
                else trap->action = ACTION_DEFAULT;
                
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
                    case SIGINT  : if(option_set('i')) signal(SIGINT  , SIGINT_handler  ); break;
                    case SIGCHLD : if(option_set('i')) signal(SIGCHLD , SIGCHLD_handler ); break;
                    case SIGWINCH: if(option_set('i')) signal(SIGWINCH, SIGWINCH_handler); break;
                    case SIGQUIT : if(option_set('q')) signal(SIGQUIT , SIGQUIT_handler ); break;
                    case SIGHUP  : signal(SIGHUP , SIGHUP_handler); break;
                }
                break;
                
            case ACTION_IGNORE:
                if(i >= ERR_TRAP_NUM || i == 0)
                {
                    trap->action = ACTION_IGNORE;
                    break;
                }                
                if(signal(i, SIG_IGN) < 0)
                {
                    fprintf(stderr, "%s: failed to ignore trap: %s\r\n", UTILITY, strerror(errno));
                    res = 1;
                }
                else trap->action = ACTION_IGNORE;
                break;
                
            case ACTION_EXECUTE:
                if(i >= ERR_TRAP_NUM || i == 0)
                {
                    trap->action = ACTION_EXECUTE;
                    trap->action_str = get_malloced_str(actionstr);
                    break;
                }
                if(signal(i, trap_handler) < 0)
                //if(sigaction(i, &trap_sigaction, NULL) < 0)
                {
                    fprintf(stderr, "%s: failed to set trap: %s\r\n", UTILITY, strerror(errno));
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
    fprintf(stderr, "%s: Unknown trap condition: %s\r\n", UTILITY, condition);
    return 1;

}

