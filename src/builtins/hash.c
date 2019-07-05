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

struct hashtab_s *utility_hashes = NULL;

int rehash_all();

/* defined in ../strbuf.c */
char *__get_malloced_str(char *str);


void init_utility_hashtable()
{
    utility_hashes = new_hashtable();
}

int hash_utility(char *utility, char *path)
{
    if(!utility || !path) return 0;
    if(!utility_hashes)
    {
        //fprintf(stderr, "%s: error reading utility hash table\n", UTILITY);
        return 0;
    }
    return add_hash_item(utility_hashes, utility, path) ? 1 : 0;
}

void unhash_utility(char *utility)
{
    if(!utility || !utility_hashes) return;
    rem_hash_item(utility_hashes, utility);
}

char *get_hashed_path(char *utility)
{
    if(!utility || !utility_hashes) return NULL;
    struct hashitem_s *entry = get_hash_item(utility_hashes, utility);
    if(!entry) return NULL;
    return entry->val;
}


int hash(int argc, char **argv)
{
    if(argc == 1)
    {
        dump_hashtable(utility_hashes, NULL);
        return 0;
    }
    int res = 0;
    /****************************
     * process the arguments
     ****************************/
    int   v = 1, c, unhash = 0, usepath = 0;
    char *path = NULL;
    int   pathlen = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "ahvrp:dl", &v, 1)) > 0)
    {
        switch(c)
        {
            /*
             * tcsh has a rehash utility that rehashes all contents of executable dirs in path.
             * additionally, it flushes the cache of home directories built by tilde expansion.
             * this flag does something similar: it forces us to re-search and re-hash all of 
             * the currently hashed utilities. we don't rehash all executable files as there would
             * probably be thousands of them.
             */
            case 'a':
                return rehash_all();
                
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_HASH, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'r':
                rem_all_items(utility_hashes, 0);
                return 0;
                
            case 'l':
                dump_hashtable(utility_hashes, NULL);
                return 0;
                
            case 'd':
                unhash = 1;
                break;
                
            case 'p':
                usepath = 1;
                path = __optarg;
                if(!path || path == INVALID_OPTARG)
                {
                    fprintf(stderr, "%s: missing argument to option -%c\r\n", UTILITY, c);
                    return 2;
                }
                /* is this shell restricted? */
                if(option_set('r'))
                {
                    /* bash says r-shells can't specify commands with '/' in their names */
                    fprintf(stderr, "%s: can't hash command containing '/': restricted shell\r\n", UTILITY);
                    return 2;
                }
                pathlen = strlen(path);
                break;                
        }
    }
    /* unknown option */
    if(c == -1) return 2;
    
    if(v >= argc)
    {
        dump_hashtable(utility_hashes, NULL);
        return 0;
    }

    for( ; v < argc; v++)
    { 
        char *arg = argv[v];
        if(unhash)
        {
            unhash_utility(arg);
        }
        else if(usepath)
        {
            char cmd[pathlen+strlen(arg)+2];    /* 2 for '\0' and possible '/' */
            if(path[pathlen-1] == '/') sprintf(cmd, "%s%s" , path, arg);
            else                       sprintf(cmd, "%s/%s", path, arg);
            if(!hash_utility(arg, cmd))
            {
                fprintf(stderr, "%s: failed to hash utility '%s'\r\n", SHELL_NAME, arg);
                res = 1;
            }
        }
        else
        {
            /* search for the requested utility using $PATH */
            char *p = search_path(arg, NULL, 1);
            if(!p)
            {
                fprintf(stderr, "%s: failed to locate utility '%s'\r\n", SHELL_NAME, arg);
                res = 1;
            }
            else
            {
                if(!hash_utility(arg, p))
                {
                    fprintf(stderr, "%s: failed to hash utility '%s'\r\n", SHELL_NAME, arg);
                    res = 1;
                }
                free_malloced_str(p);
            }
        }
    }
    return res;
}


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
                    fprintf(stderr, "%s: failed to locate utility '%s'\r\n", SHELL_NAME, entry->name);
                    res = 1;
                }
                else
                {
                    /* we directly free this bcuz it was alloc'd in string_hash.c */
                    if(entry->val) free(entry->val);
                    entry->val = __get_malloced_str(p);
                    free_malloced_str(p);
                }
                entry = entry->next;
            }
        }
    }
    return res;
}
