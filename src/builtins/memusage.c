/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: memusage.c
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
#include <stdio.h>
#include <sys/resource.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../symtab/string_hash.h"
#include "../parser/node.h"
#include "../debug.h"

#define UTILITY         "memusage"

long long memusage_symtab_stack(long long *res);
long long memusage_symtab(struct symtab_s *symtab, long long *res);
long long memusage_hashtab(struct hashtab_s *hashtab, long long *res, int addvals);
long long memusage_node(struct node_s *node, long long *__res);
long long memusage_traps(long long *__res);
long long memusage_aliases(long long *__res);
long long memusage_history(long long *__res);
long long memusage_dirstack(long long *res);

void print_mu_stack(int lengthy);
void print_mu_hashtab(int lengthy);
void print_mu_str_hashtab(int lengthy);
void print_mu_traps();
void print_mu_inputbuf();
void print_mu_history();
void print_mu_cmdbuf();
void print_mu_dirstack(int lengthy);
void print_mu_vm(int lengthy);
void print_mu_aliases();

void output_size(long long __size);

/* defined in trap.c */
extern struct trap_item_s trap_table[];

/* defined in strbuf.c */
extern struct hashtab_s *str_hashes;


int memusage(int argc, char **argv)
{
    int lengthy = 0;

    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvl", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_MEMUSAGE, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'l':
                lengthy = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        printf("This utility shows the shell's dynamic memory usage. It reports the current usage\r\n"
               "and size of shell buffers, symbol table stack, hash tables, and so on.\r\n"
               "It is important to understand that this information does not include statically\r\n"
               "allocated memory, nor does it include dynamic memory allocated by different functions\r\n"
               "as part of their call stack. It doesn't also include memory allocated by different parts\r\n"
               "of the shell to store and process strings.\r\n\n"
               "This data is only provided for general information and experimental purposes. Please\r\n"
               "don't draw any conclusions whatsoever from the numbers provided in here!\r\n\n"
              );
        printf("Shell memory usage:\n");
        printf("===================\n");
        print_mu_vm(lengthy);
        print_mu_stack(lengthy);
        print_mu_hashtab(lengthy);
        print_mu_str_hashtab(lengthy);
        print_mu_dirstack(lengthy);
        print_mu_aliases();
        print_mu_traps();
        print_mu_inputbuf();
        print_mu_history();
        print_mu_cmdbuf();
        return 0;
    }
    
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        if(strcmp(arg, "stack") == 0 || strcmp(arg, "symtabs") == 0)
            print_mu_stack(lengthy);
        else if(strcmp(arg, "hash") == 0 || strcmp(arg, "hashtab") == 0)
            print_mu_hashtab(lengthy);
        else if(strcmp(arg, "strbuf") == 0 || strcmp(arg, "strtab") == 0)
            print_mu_str_hashtab(lengthy);
        else if(strcmp(arg, "traps") == 0)
            print_mu_traps();
        else if(strcmp(arg, "input") == 0)
            print_mu_inputbuf();
        else if(strcmp(arg, "history") == 0)
            print_mu_history();
        else if(strcmp(arg, "cmdbuf") == 0 || strcmp(arg, "cmdbuffer") == 0)
            print_mu_cmdbuf();
        else if(strcmp(arg, "dirstack") == 0)
            print_mu_dirstack(lengthy);
        else if(strcmp(arg, "vm") == 0)
            print_mu_vm(lengthy);
        else if(strcmp(arg, "aliases") == 0)
            print_mu_aliases();
    }
    
    return 0;
}

/*
 * functions to print out pretty and formatted results.
 */

void print_mu_vm(int lengthy)
{
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    printf("* System memory usage: ");
    if(lengthy)
    {
        printf("\n  - Resident Set Size (RSS): ");
            output_size((size_t)(rusage.ru_maxrss * 1024L));
        printf("\n  - RSS shared memory size: ");
            output_size((size_t)(rusage.ru_ixrss * 1024L)); printf("-sec");
        printf("\n  - Data segment size: ");
            output_size((size_t)(rusage.ru_idrss * 1024L)); printf("-sec");
        printf("\n  - Stack segment size: ");
            output_size((size_t)(rusage.ru_isrss * 1024L)); printf("-sec");
        printf("\n");
    }
    else
    {
        output_size((rusage.ru_maxrss + rusage.ru_ixrss + rusage.ru_idrss + rusage.ru_isrss) * 1024L);
        printf("\n");
    }
}

void print_mu_dirstack(int lengthy)
{
    long long res[2];
    long long i = memusage_dirstack(res);
    printf("* Directory stack: ");
    if(!lengthy)
    {
        output_size(i);
        printf("\n");
    }
    else
    {
        printf("\n  - stack structure: "); output_size(res[0]);
        printf("\n  - directory names (strings): "); output_size(res[1]);
        printf("\n");
    }
}


void print_mu_stack(int lengthy)
{
    long long res[3];
    long long i = memusage_symtab_stack(res);
    printf("* Symbol table stack: ");
    if(!lengthy)
    {
        output_size(i);
        printf("\n");
    }
    else
    {
        printf("\n  - stack structure: "); output_size(res[0]);
        printf("\n  - symbol names and values (strings): "); output_size(res[1]);
        printf("\n  - function definitions: "); output_size(res[2]);
        printf("\n");
    }
}


void print_mu_hashtab(int lengthy)
{
    long long res[2];
    long long i = memusage_hashtab(utility_hashes, res, 1);
    printf("* Utility names and paths hashtable: ");
    if(!lengthy)
    {
        output_size(i);
        printf("\n");
    }
    else
    {
        printf("\n  - hashtable structure: "); output_size(res[0]);
        printf("\n  - utility names and paths (strings): "); output_size(res[1]);
        printf("\n");
    }
}


void print_mu_str_hashtab(int lengthy)
{
    long long res[2];
    long long i = memusage_hashtab(str_hashes, res, 0);
    printf("* Internal string buffer: ");
    if(!lengthy)
    {
        output_size(i);
        printf("\n");
    }
    else
    {
        printf("\n  - hashtable structure: "); output_size(res[0]);
        printf("\n  - string values: "); output_size(res[1]);
        printf("\n");
    }
}


void print_mu_traps()
{
    long long i = memusage_traps(NULL);
    printf("* Traps (strings): ");
    output_size(i);
    printf("\n");
}


void print_mu_inputbuf()
{
    printf("* Input buffer: ");
    output_size(src->bufsize);
    printf("\n");
}


void print_mu_aliases()
{
    long long i = memusage_aliases(NULL);
    printf("* Alias names and values (strings): ");
    output_size(i);
    printf("\n");
}


void print_mu_history()
{
    long long i = memusage_history(NULL);
    printf("* Command line history (strings): ");
    output_size(i);
    printf("\n");
}


void print_mu_cmdbuf()
{
    long long sz = (long long)cmdbuf_size;
    printf("* Command line buffer (string): ");
    output_size(sz);
    printf("\n");
}


/*
 * functions to do the actual work of calculating memory usage of
 * different shell components.
 */

long long memusage_dirstack(long long *res)
{
    res[0] = 0;
    res[1] = 0;
    extern struct dirstack_ent_s *dirstack;     /* dirstack.c */
    struct dirstack_ent_s *ds = dirstack;
    while(ds)
    {
        res[0] += sizeof(struct dirstack_ent_s);      /* memory used by the struct */
        if(ds->path)
            res[1] += strlen(ds->path)+1;             /* memory used by the strings */
        ds = ds->next;
    }
    return res[0]+res[1];
}


long long memusage_symtab_stack(long long *res)
{
    int i;
    res[0] = sizeof(struct symtab_stack_s);
    res[1] = 0;
    res[2] = 0;
    struct symtab_stack_s *stack = get_symtab_stack();
    for(i = 0; i < stack->symtab_count; i++)
    {
        long long res2[3] = { 0, 0, 0 };
        if(memusage_symtab(stack->symtab_list[i], res2))
        {
            res[0] += res2[0];      /* memory used by the symtab */
            res[1] += res2[1];      /* memory used by the strings */
            res[2] += res2[2];      /* memory used by function definitions */
        }
    }
    return res[0]+res[1]+res[2];
}


long long memusage_symtab(struct symtab_s *symtab, long long *__res)
{
    if(!symtab) return 0;
    long long res[3];
    res[0] = sizeof(struct symtab_s);
    res[1] = 0;
    res[2] = 0;
    
#ifdef USE_HASH_TABLES
    
    res[0] += symtab->size * sizeof(struct symtab_entry_s *);
    if(symtab->used)
    {
        struct symtab_entry_s **h1 = symtab->items;
        struct symtab_entry_s **h2 = symtab->items + symtab->size;
        for( ; h1 < h2; h1++)
        {
            struct symtab_entry_s *entry = *h1;
                
#else

    struct symtab_entry_s *entry  = symtab->first;
            
#endif
            
            while(entry)
            {
                res[0] += sizeof(struct symtab_entry_s);
                if(entry->val ) res[1] += strlen(entry->val )+1;
                if(entry->name) res[1] += strlen(entry->name)+1;
                if(entry->func_body) res[2] += memusage_node(entry->func_body, NULL);
                entry = entry->next;
            }
                
#ifdef USE_HASH_TABLES

        }
    }
        
#endif

    if(__res) __res[0] = res[0], __res[1] = res[1], __res[2] = res[2];
    return res[0]+res[1]+res[2];
}


long long memusage_hashtab(struct hashtab_s *hashtab, long long *__res, int addvals)
{
    if(!hashtab)
    {
        if(__res) __res[0] = 0, __res[1] = 0;
        return 0;
    }
    long long res[3];
    res[0] = sizeof(struct hashtab_s);
    res[0] += hashtab->size * sizeof(struct hashitem_s *);
    res[1] = 0;

    if(hashtab->used)
    {
        struct hashitem_s **h1 = hashtab->items;
        struct hashitem_s **h2 = hashtab->items + hashtab->size;
        for( ; h1 < h2; h1++)
        {
            struct hashitem_s *entry = *h1;
            while(entry)
            {
                res[0] += sizeof(struct hashitem_s);
                if(addvals && entry->val) res[1] += strlen(entry->val)+1;
                if(entry->name) res[1] += strlen(entry->name)+1;
                entry = entry->next;
            }
        }
    }

    if(__res) __res[0] = res[0], __res[1] = res[1];
    return res[0]+res[1];
}


long long memusage_node(struct node_s *node, long long *__res)
{
    if(!node) return 0;
    long long res[2];
    res[0] = sizeof(struct node_s);
    res[1] = 0;
    struct node_s *child = node->first_child;
    while(child)
    {
        long long res2[2] = { 0, 0 };
        struct node_s *next = child->next_sibling;
        memusage_node(child, res2);
        res[0] += res2[0];
        res[1] += res2[1];
        child = next;
    }
    
    if(node->val_type == VAL_STR)
    {
        if(node->val.str) res[1] += strlen(node->val.str)+1;
    }

    if(__res) __res[0] = res[0], __res[1] = res[1];
    return res[0]+res[1];
}


long long memusage_traps(long long *__res)
{
    long long res = 0;
    int i;
    for(i = 0; i < TRAP_COUNT; i++)
    {
        if(trap_table[i].action == ACTION_EXECUTE)
        {
            res += strlen(trap_table[i].action_str)+1;
        }
    }
    if(__res) __res[0] = res;
    return res;
}


long long memusage_aliases(long long *__res)
{
    long long res = 0;
    int i;
    for(i = 0; i < MAX_ALIASES; i++)
    {
        if(__aliases[i].name) res += strlen(__aliases[i].name);
        if(__aliases[i].val ) res += strlen(__aliases[i].val );
    }
    if(__res) __res[0] = res;
    return res;
}


long long memusage_history(long long *__res)
{
    long long res = 0;
    int i;
    for(i = 0; i < cmd_history_end; i++)
    {
        if(cmd_history[i].cmd) res += strlen(cmd_history[i].cmd);
    }
    if(__res) __res[0] = res;
    return res;
}

/*
 * output byte size in a properly formatted way.
 */
void output_size(long long __size)
{
    int i = 0;
    double size = __size;
    static char *units[] = { "bytes", "kb", "Mb", "Gb", "Tb", "Pb", "Eb", "Zb", "Yb" };
    while(size > 1024)
    {
        size /= 1024;
        i++;
    }
    printf("%.*f %s", i, size, units[i]);
}
