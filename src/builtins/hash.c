/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: hash.c
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

#include <stdlib.h>
#include "builtins.h"
#include "../cmd.h"
#include "../symtab/string_hash.h"
#include "../debug.h"

#define UTILITY             "hash"

/*
 * This is the hashtable where we store the names of executable utilities and
 * their full pathnames, so that we can execute these utilities without having
 * to search through $PATH every time the utility is invoked.
 */
struct hashtab_s *utility_hashtable = NULL;

/* defined below */
int rehash_all();


/*
 * Initialize the utility hashtable.
 */
void init_utility_hashtable(void)
{
    utility_hashtable = new_hashtable();
}


/*
 * Remember a utility for later invocation by hashing its name
 * and adding its path to the utility hashtable. When we call the
 * utility later on, we don't need to go through $PATH to find the
 * utility, we just need to retrieve its path from the hashtable.
 *
 * Returns 1 on success, 0 on failure.
 */
int hash_utility(char *utility, char *path)
{
    /* sanity checks */
    if(!utility || !path || !utility_hashtable)
    {
        return 0;
    }
    
    /* hash the utility */
    return add_hash_item(utility_hashtable, utility, path) ? 1 : 0;
}


/*
 * Remove a utility from the hashtable, so that when we call it again,
 * we will need to go through $PATH in order to find the utility's path.
 */
void unhash_utility(char *utility)
{
    if(!utility || !utility_hashtable)
    {
        return;
    }
    
    rem_hash_item(utility_hashtable, utility);
}


/*
 * Search for a utility in the hashtable and return its path.
 *
 * Returns the path of the utility on success, NULL if the utility is
 * not found in the hashtable.
 */
char *get_hashed_path(char *utility)
{
    if(!utility || !utility_hashtable)
    {
        return NULL;
    }
    
    struct hashitem_s *entry = get_hash_item(utility_hashtable, utility);
    if(!entry)
    {
        return NULL;
    }
    
    return entry->val;
}


#define RESTRICTED_SHELL_NOHASH(msg)                    \
do {                                                    \
    if(startup_finished && option_set('r'))             \
    {                                                   \
        fprintf(stderr, "%s: %s\n", UTILITY, (msg));  \
        return 2;                                       \
    }                                                   \
} while(0)


/*
 * The hash builtin utility (POSIX). Used to store the names and pathnames
 * of invoked utilities, so that the shell remembers where the utilities are and
 * doesn't have to go through $PATH in order to find a utility.
 *
 * Returns 0 on success, non-zero otherwise.
 *
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help hash` or `hash -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int hash_builtin(int argc, char **argv)
{
    /* hashing must be enabled */
    if(!option_set('h'))
    {
        PRINT_ERROR("%s: hashing is disabled (use `set -o hashall` to reenable it)\n", UTILITY);
        return 1;
    }
    
    /* no arguments or options. print the hashed utilities and return */
    if(argc == 1)
    {
        dump_hashtable(utility_hashtable, NULL);
        return 0;
    }

    int   res = 0;
    int   v = 1, c, unhash = 0, list_only = 0;
    char *usepath = NULL;
    int   is_restirected = (startup_finished && option_set('r'));

    /* use our default format if -t isn't specified */
    char *format = "%s=%s\n";

    /*
     * Recognize the options defined by POSIX if we are running in --posix mode,
     * or all possible options if running in the regular mode.
     */
    char *opts = option_set('P') ? "r" : "adhlp:rtv";
    
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, opts, &v, FLAG_ARGS_PRINTERR)) > 0)
    {
        switch(c)
        {
            /*
             * tcsh has a 'rehash' utility that rehashes all contents of executable dirs in path.
             * Additionally, it flushes the cache of home directories built by tilde expansion.
             * This flag does something similar: it forces us to re-search and re-hash all of 
             * the currently hashed utilities. We don't rehash all executable files as there would
             * probably be thousands of them.
             */
            case 'a':
                return rehash_all();
                
            /* -h prints help */
            case 'h':
                print_help(argv[0], &HASH_BUILTIN, 0);
                return 0;
                
            /* -v prints shell ver */
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* -r removes all hashed utilities from the table */
            case 'r':
                rem_all_items(utility_hashtable, 0);
                return 0;
                
            /* -l prints the contents of the utilities hashtable */
            case 'l':
                dump_hashtable(utility_hashtable, NULL);
                return 0;
                
            /* -d unhashes the upcoming arguments */
            case 'd':
                unhash = 1;
                break;
                
            /* -p provides a path to use instead of $PATH */
            case 'p':
                usepath = internal_optarg;
                if(!usepath || usepath == INVALID_OPTARG)
                {
                    PRINT_ERROR("%s: missing argument to option -%c\n", UTILITY, c);
                    return 2;
                }

                /*
                 * Is this shell restricted?
                 * bash says r-shells can't specify commands with '/' in their names.
                 * zsh doesn't allow r-shells to use hash, period.
                 */
                if(is_restirected && strchr(usepath, '/'))
                {
                    PRINT_ERROR("%s: cannot hash command containing '/': "
                                "restricted shell\n", UTILITY);
                    return 2;
                }
                break;                
                
            /* -t lists the commands along with their pathnames (bash) */
            case 't':
                list_only = 1;
                format = "%s\t%s\n";
                break;
        }
    }
    
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /* no arguments */
    if(v >= argc)
    {
        if(list_only)
        {
            /* -t was specified, which needs at least one argument */
            PRINT_ERROR("%s: option needs argument: -%c\n", UTILITY, 't');
            return 2;
        }
        else
        {
            /* print the hashed utilities */
            dump_hashtable(utility_hashtable, NULL);
            return 0;
        }
    }
    
    /* check for a restricted shell (can't hash in r-shells) */
    if(is_restirected)
    {
        PRINT_ERROR("%s: cannot use the hash utility: restricted shell\n", UTILITY);
        return 2;
    }
    
    /* process the arguments */
    for( ; v < argc; v++)
    { 
        char *arg = argv[v];
        /* list commands with the -t option */
        if(list_only)
        {
            struct hashitem_s *entry = get_hash_item(utility_hashtable, arg);
            if(entry)
            {
                printf(format, entry->name, entry->val);
            }
            else
            {
                PRINT_ERROR("%s: cannot find hashed utility: %s\n", UTILITY, arg);
                res = 1;
            }
        }
        /* unhash utility */
        else if(unhash)
        {
            unhash_utility(arg);
        }
        /* hash utility using the given path */
        else if(usepath)
        {
            if(file_exists(usepath))
            {
                if(!hash_utility(arg, usepath))
                {
                    PRINT_ERROR("%s: failed to hash utility: %s\n", UTILITY, arg);
                    res = 1;
                }
            }
            else
            {
                PRINT_ERROR("%s: file doesn't exist or is not a regular file: "
                            "%s\n", UTILITY, usepath);
                res = 1;
            }
        }
        else
        {
            /* silently ignore shell builtins and functions */
            if(is_builtin(arg) || is_function(arg))
            {
                continue;
            }
            
            /* search for the requested utility using $PATH */
            char *p = search_path(arg, NULL, 1);
            if(!p)
            {
                PRINT_ERROR("%s: failed to locate utility: %s\n", UTILITY, arg);
                res = 1;
            }
            else
            {
                if(!hash_utility(arg, p))
                {
                    PRINT_ERROR("%s: failed to hash utility: %s\n", UTILITY, arg);
                    res = 1;
                }
                free_malloced_str(p);
            }
        }
    }
    return res;
}


/*
 * Update the hashtable by rehashing all the hashed utilities.
 *
 * Returns 0 if all utilities are located and hashed, 1 otherwise.
 */
int rehash_all(void)
{
    int res = 0;
    if(utility_hashtable->used)
    {
        struct hashitem_s **h1 = utility_hashtable->items;
        struct hashitem_s **h2 = utility_hashtable->items + utility_hashtable->size;
        for( ; h1 < h2; h1++)
        {
            struct hashitem_s *entry = *h1;
            while(entry)
            {
                /* search for the requested utility using $PATH */
                char *p = search_path(entry->name, NULL, 1);
                if(!p)
                {
                    PRINT_ERROR("%s: failed to locate utility '%s'\n", UTILITY, entry->name);
                    res = 1;
                }
                else
                {
                    /* we directly free this because it was alloc'd in string_hash.c */
                    if(entry->val)
                    {
                        free(entry->val);
                    }
                    entry->val = __get_malloced_str(p);
                    free_malloced_str(p);
                }
                entry = entry->next;
            }
        }
    }
    return res;
}
