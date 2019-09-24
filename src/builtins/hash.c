/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
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
#include "../cmd.h"
#include "../symtab/string_hash.h"
#include "../debug.h"

#define UTILITY             "hash"

/*
 * this is the hashtable where we store the names of executable utilities and
 * their full pathnames, so that we can execute these utilities without having
 * to search through $PATH every time the utility is invoked.
 */
struct hashtab_s *utility_hashes = NULL;

/* defined below */
int rehash_all();

/* defined in ../strbuf.c */
char *__get_malloced_str(char *str);


/*
 * initialize the utility hashtable.
 */
void init_utility_hashtable()
{
    utility_hashes = new_hashtable();
}


/*
 * remember a utility for later invocation by hashing its name
 * and adding its path to the utility hashtable.. when we call the
 * utility later on, we don't need to go through $PATH to find the
 * utility, we just need to retrieve its path from the hashtable.
 *
 * returns 1 on success, 0 on failure.
 */
int hash_utility(char *utility, char *path)
{
    /* sanity checks */
    if(!utility || !path || !utility_hashes)
    {
        return 0;
    }
    /* hash the utility */
    return add_hash_item(utility_hashes, utility, path) ? 1 : 0;
}


/*
 * remove a utility from the hashtable, so that when we call it again,
 * we will need to go through $PATH in order to find the utility's path.
 */
void unhash_utility(char *utility)
{
    if(!utility || !utility_hashes)
    {
        return;
    }
    rem_hash_item(utility_hashes, utility);
}


/*
 * search for a utility in the hashtable and return its path.
 *
 * returns the path of the utility on success, NULL if the utility is
 * not found in the hashtable.
 */
char *get_hashed_path(char *utility)
{
    if(!utility || !utility_hashes)
    {
        return NULL;
    }
    struct hashitem_s *entry = get_hash_item(utility_hashes, utility);
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
 * the hash builtin utility (POSIX).. used to store the names and pathnames
 * of invoked utilities, so that the shell remembers where the utilities are and
 * doesn't have to go through $PATH in order to find a utility.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help hash` or `hash -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int hash(int argc, char **argv)
{
    /* no arguments or options. print the hashed utilities and return */
    if(argc == 1)
    {
        dump_hashtable(utility_hashes, NULL);
        return 0;
    }
    int res = 0;
    int   v = 1, c, unhash = 0, usepath = 0;
    char *path = NULL;
    int   pathlen = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "ahvrp:dl", &v, 1)) > 0)
    {
        switch(c)
        {
            /*
             * tcsh has a 'rehash' utility that rehashes all contents of executable dirs in path..
             * additionally, it flushes the cache of home directories built by tilde expansion..
             * this flag does something similar: it forces us to re-search and re-hash all of 
             * the currently hashed utilities.. we don't rehash all executable files as there would
             * probably be thousands of them.
             */
            case 'a':
                return rehash_all();
                
            /* -h prints help */
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_HASH, 1, 0);
                return 0;
                
            /* -v prints shell ver */
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            /* -r removes all hashed utilities from the table */
            case 'r':
                rem_all_items(utility_hashes, 0);
                return 0;
                
            /* -l prints the contents of the utilities hashtable */
            case 'l':
                dump_hashtable(utility_hashes, NULL);
                return 0;
                
            /* -d unhashes the upcoming arguments */
            case 'd':
                unhash = 1;
                break;
                
            /* -p provides a path to use instead of $PATH */
            case 'p':
                usepath = 1;
                path = __optarg;
                if(!path || path == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: missing argument to option -%c\n", UTILITY, c);
                    return 2;
                }
                /*
                 * is this shell restricted?
                 * bash says r-shells can't specify commands with '/' in their names.
                 * zsh doesn't allow r-shells to use hash, period.
                 */
                RESTRICTED_SHELL_NOHASH("can't hash command containing '/': restricted shell");
                pathlen = strlen(path);
                break;                
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /* no arguments. print the hashed utilities */
    if(v >= argc)
    {
        dump_hashtable(utility_hashes, NULL);
        return 0;
    }

    /* process the arguments */
    for( ; v < argc; v++)
    { 
        char *arg = argv[v];
        /* unhash utility */
        if(unhash)
        {
            unhash_utility(arg);
        }
        /* hash utility using the given path */
        else if(usepath)
        {
            char cmd[pathlen+strlen(arg)+2];    /* 2 for '\0' and possible '/' */
            if(path[pathlen-1] == '/')
            {
                sprintf(cmd, "%s%s" , path, arg);
            }
            else
            {
                sprintf(cmd, "%s/%s", path, arg);
            }
            /* check for a restricted shell (can't hash in r-shells) */
            RESTRICTED_SHELL_NOHASH("can't use the hash command: restricted shell");
            if(!hash_utility(arg, cmd))
            {
                fprintf(stderr, "%s: failed to hash utility '%s'\n", SHELL_NAME, arg);
                res = 1;
            }
        }
        else
        {
            /* search for the requested utility using $PATH */
            char *p = search_path(arg, NULL, 1);
            if(!p)
            {
                fprintf(stderr, "%s: failed to locate utility '%s'\n", SHELL_NAME, arg);
                res = 1;
            }
            else
            {
                /* check for a restricted shell (can't hash in r-shells) */
                if(startup_finished && option_set('r'))
                {
                    fprintf(stderr, "%s: can't use the hash command: restricted shell\n", UTILITY);
                    free_malloced_str(p);
                    return 2;
                }
                //RESTRICTED_SHELL_NOHASH("can't use the hash command: restricted shell");
                if(!hash_utility(arg, p))
                {
                    fprintf(stderr, "%s: failed to hash utility '%s'\n", SHELL_NAME, arg);
                    res = 1;
                }
                free_malloced_str(p);
            }
        }
    }
    return res;
}


/*
 * update the hashtable by rehashing all the hashed utilities.
 *
 * returns 0 if all utilities are located and hashed, 1 otherwise.
 */
int rehash_all()
{
    int res = 0;
    if(utility_hashes->used)
    {
        struct hashitem_s **h1 = utility_hashes->items;
        struct hashitem_s **h2 = utility_hashes->items + utility_hashes->size;
        for( ; h1 < h2; h1++)
        {
            struct hashitem_s *entry = *h1;
            while(entry)
            {
                /* search for the requested utility using $PATH */
                char *p = search_path(entry->name, NULL, 1);
                if(!p)
                {
                    fprintf(stderr, "%s: failed to locate utility '%s'\n", SHELL_NAME, entry->name);
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
