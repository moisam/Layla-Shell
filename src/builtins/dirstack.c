/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: dirstack.c
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

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <pwd.h>
#include <errno.h>
#include "setx.h"
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

struct dirstack_ent_s *dirstack = NULL;
int    stack_count = 0;
int    read_dirsfile = 0;
int    push_cwd();
int    push_dir(char *dir, struct dirstack_ent_s **stack, int *count);


int init_dirstack()
{
    if(read_dirsfile) return load_dirstack(NULL);
    else              return push_cwd();
}

void free_dirstack()
{
    struct dirstack_ent_s *ds = dirstack;
    while(ds)
    {
        struct dirstack_ent_s *next = ds->next;
        if(ds->path) free_malloced_str(ds->path);
        free(ds);
        ds = next;
    }
    dirstack = NULL;
    stack_count = 0;
}

/*
 * called on startup by a login shell, or a shell passed the --dirsfile option,
 * or by dirs when given the -L option.
 */
int load_dirstack(char *__path)
{
    char *path = NULL;
    if(__path) path = __path;       /* path given to 'dirs -L' */
    else                            /* 'dirs -L' invoked with no path, or we're called on shell startup */
    {
        path = get_shell_varp("DIRSFILE", DIRSTACK_FILE);
    }
    path = word_expand_to_str(path);
    if(!path)
    {
        return push_cwd();
    }
    FILE *f = fopen(path, "r");
    struct dirstack_ent_s *newstack = NULL;
    int newcount = 0;
    if(f)
    {
        int linemax = get_linemax();
        char buf[linemax];
        char *line = NULL;
        while((line = fgets(buf, linemax, f)))
        {
            if(*line == '\0' || *line == '#') continue;       /* don't add empty or commented lines */
            /* 
             * dirs are saved in the file in reverse order of that of the stack.
             * the format of each entry is: "pushd dir\n".
             */
            char *p = strstr(line, "pushd ");
#if 0
            if(!p)
            {
                p = strstr(line, "cd ");
                if(!p) continue;
                p += 3;
                while(*p && isspace(*p)) p++;   /* skip spaces after the 'cd' command word */
                if(!p) continue;
                cd(2, (char *[]){ "cd", p, NULL });
                continue;
            }
#endif
            p += 6;
            while(*p && isspace(*p)) p++;   /* skip spaces after the 'pushd' command word */
            if(!p) continue;
            int n = strlen(p)-1;
            if(p[n] == '\n') p[n] = '\0';
            if(cd(2, (char *[]){ "cd", p, NULL }) != 0)
            {
                /*
                * TODO: do something better than just b*^%$ about it.
                */
                fprintf(stderr, "%s: failed to cd to %s\n", SHELL_NAME, path);
                goto err;
            }
            if(!push_dir(p, &newstack, &newcount)) break;
            newcount++;
        }
        fclose(f);
    }
    else
    {
        fprintf(stderr, "%s: failed to load dirstack from %s: %s\r\n", SHELL_NAME, path, strerror(errno));
    }
    free(path);
    if(!newstack || !newcount) goto err;

    /* free the old stack */
    free_dirstack();

    /* save the new one */
    dirstack = newstack;
    stack_count = newcount;
    if(!stack_count) push_cwd();
    if(!cwd || strcmp(dirstack->path, cwd) != 0)
    {
        cd(2, (char *[]){ "cd", dirstack->path, NULL });
    }
    return 1;
    
err:
    if(newstack)
    {
        struct dirstack_ent_s *ds = newstack;
        while(ds)
        {
            struct dirstack_ent_s *next = ds->next;
            if(ds->path) free_malloced_str(ds->path);
            free(ds);
            ds = next;
        }
    }
    return 0;
}

/*
 * load the directory stack with the values in val. entries are separated by whitespace
 * chars. stack will be cleared before assigning new entries and cd'ing to the top of stack.
 */
int load_dirstackp(char *val)
{
    if(!val || !*val) return 0;
    /* 
     * dirs are saved in the string in the same order they were stored in the stack.
     */
    char *p = val, *p2;
    struct dirstack_ent_s *newstack = NULL, *last = NULL;
    int newcount = 0;
    while(*p)
    {
        while(*p && isspace(*p)) p++;   /* skip leading spaces */
        if(!p) break;
        p2 = p;
        while(*p2)         /* get word boundary */
        {
            if(isspace(*p2) && p2[-1] != '\\') break;
            p2++;
        }
        char *path = get_malloced_strl(p, 0, p2-p);
        if(!path) break;
        int n = strlen(path)-1;
        if(path[n] == '\n') path[n] = '\0';
        
        if(cd(2, (char *[]){ "cd", path, NULL }) != 0)
        {
            /*
             * TODO: do something better than just b*^%$ about it.
             */
            fprintf(stderr, "%s: failed to cd to %s\n", SHELL_NAME, path);
            free_malloced_str(path);
            goto err;
        }
            
        struct dirstack_ent_s *ds = malloc(sizeof(struct dirstack_ent_s));
        if(!ds)
        {
            free_malloced_str(path);
            goto err;
        }
        ds->path = path;
        ds->next = NULL;
        ds->prev = NULL;
        if(!last)
        {
            newstack     = ds;
            last         = ds;
        }
        else
        {
            last->next   = ds;
            ds->prev     = last;
            last         = ds;
        }
        newcount++;
        p = p2;
    }
    if(!newstack || !newcount) goto err;
    
    /* free the old stack */
    free_dirstack();

    /* save the new one */
    dirstack = newstack;
    stack_count = newcount;
    if(!stack_count) push_cwd();
    if(!cwd || strcmp(dirstack->path, cwd) != 0)
    {
        cd(2, (char *[]){ "cd", dirstack->path, NULL });
    }
    return 1;
    
err:
    if(newstack)
    {
        struct dirstack_ent_s *ds = newstack;
        while(ds)
        {
            struct dirstack_ent_s *next = ds->next;
            if(ds->path) free_malloced_str(ds->path);
            free(ds);
            ds = next;
        }
    }
    return 0;
}

/*
 * called on shutdown by a login shell, and by dirs when given the -S option.
 */
void save_dirstack(char *__path)
{
    char *path = NULL;
    if(__path) path = __path;       /* path given to 'dirs -S' */
    else                            /* 'dirs -S' invoked with no path, or we're called on shutdown */
    {
        path = get_shell_varp("DIRSFILE", DIRSTACK_FILE);
    }
    path = word_expand_to_str(path);
    if(!path) return;
    FILE *f = fopen(path, "w");
    if(f)
    {
        struct dirstack_ent_s *ds = dirstack;
        while(ds->next) ds = ds->next;
        while(ds)
        {
            if(ds->path[0]) fprintf(f, "pushd %s\n", ds->path);
            ds = ds->prev;
        }
    }
    else
    {
        fprintf(stderr, "%s: failed to save dirstack to %s: %s\r\n", SHELL_NAME, path, strerror(errno));
    }
    free(path);
}

struct dirstack_ent_s *get_dirstack_entryn(int n, struct dirstack_ent_s **__prev)
{
    char buf[16];
    sprintf(buf, "%d", n);
    return get_dirstack_entry(buf, __prev);
}

struct dirstack_ent_s *get_dirstack_entry(char *nstr, struct dirstack_ent_s **__prev)
{
    if(!nstr) return NULL;

    char *strend = NULL;
    char c = '+';
    if(*nstr == '-' || *nstr == '+') c = *nstr++;

    int n = strtol(nstr, &strend, 10);
    if(strend == nstr) return NULL;
    if(c == '-')                 /* count from the right, or end of list */
    {
        if(n == 0) n = stack_count-1;
        else n = stack_count-1-n;
    }
    if(n >= stack_count) return NULL;
    struct dirstack_ent_s *ds   = dirstack;
    struct dirstack_ent_s *prev = NULL    ;
    while(n-- && ds)
    {
        prev = ds;
        ds = ds->next;
    }
    if(__prev) *__prev = prev;
    return ds;
}

void print_dir(char *path, char *home, int fullpaths)
{
    /* print the directory as it is */
    if(fullpaths || !home) printf("%s", path);
    else
    {
        /* substitute every occurence of home dir with ~ */
        int lenhome = strlen(home);
        char *p = path, *p2;
        while((p2 = strstr(p, home)))
        {
            int len = p2-p;
            if(len)
            {
                char buf[len+1];
                strncpy(buf, p, len);
                p[len] = '\0';
                printf("%s", buf);
            }
            putchar('~');
            p = p2+lenhome;
        }
        printf("%s", p);
    }
}

void purge_dirstack(int flags)
{
    int fullpaths      = flag_set(flags, FLAG_DIRSTACK_FULL_PATHS    );
    int print_separate = flag_set(flags, FLAG_DIRSTACK_SEPARATE_LINES);
    int print_index    = flag_set(flags, FLAG_DIRSTACK_PRINT_INDEX   );
    int wrap           = flag_set(flags, FLAG_DIRSTACK_WRAP_ENTRIES  );
    struct dirstack_ent_s *ds = dirstack;
    int i = 0, n = 0;
    /* as in tcsh, -v takes precedence over -n */
    if(print_index && wrap) wrap = 0;

    while(ds)
    {
        //struct passwd *pw = getpwuid(geteuid());
        //char *home = pw ? pw->pw_dir : NULL;
        char *home = get_home();
        /* wrap if needed */
        int len = strlen(ds->path);
        if(wrap && n+len >= VGA_WIDTH)
        {
            printf("\r\n");
            n = 0;
        }
        if(print_index)
        {
            printf("%3d  ", i++);
            n += 5;
        }
        print_dir(ds->path, home, fullpaths);
        n += len+1;
        if(print_separate) putchar('\n');
        else if(ds->next) putchar(' ');
        ds = ds->next;
    }
    if(!print_separate) putchar('\n');
}

/*
 * same as purge_dirstack() above, but saves the output in a string, instead of
 * sending it to stdout.
 */
char *purge_dirstackp()
{
    struct dirstack_ent_s *ds = dirstack;
    /* calculate the space we will need */
    int i = 0;
    while(ds)
    {
        i += strlen(ds->path)+1;    /* add 1 for the separating space */
        ds = ds->next;
    }
    char *p = malloc(i+1);
    if(!p) return NULL;
    p[0] = '\0';
    /* now copy the entries */
    ds = dirstack;
    while(ds)
    {
        strcat(p, ds->path);
        if(ds->next) strcat(p, " ");
        ds = ds->next;
    }
    return p;
}

int push_dir(char *dir, struct dirstack_ent_s **stack, int *count)
{
    char *path = get_malloced_str(dir);
    if(!path) return 0;
    struct dirstack_ent_s *ds = malloc(sizeof(struct dirstack_ent_s));
    if(!ds)
    {
        free_malloced_str(path);
        return 0;
    }
    ds->path = path;
    ds->next = *stack;
    ds->prev = NULL;
    if(*stack) (*stack)->prev = ds;
    *stack = ds;
    (*count) = (*count)+1;
    return 1;
}

int push_cwd()
{
    return push_dir(cwd, &dirstack, &stack_count);
}


int dirs_cd()
{
    if(cd(2, (char *[]){ "cd", dirstack->path, NULL }) != 0) return 0;
    /* make sure we have a full path in the stack entry */
    if(dirstack->path[0] != '/' && dirstack->path[0] != '~')
    {
        free_malloced_str(dirstack->path);
        dirstack->path = get_malloced_str(cwd);
    }
    return 1;
}


/*
 * the dirs builtin utility (non-POSIX extension).
 */

int dirs(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int clear = 0, fullpaths = 0, print_separate = 0, print_index = 0, n = 0;
    int v = 1, c;
    int flags = 0, wrap = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hclnpvL:S:", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_DIRS, 1, 0);
                return 0;
                
            case 'v':
                print_index = 1;
                print_separate = 1;
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                flags |= FLAG_DIRSTACK_PRINT_INDEX;
                break;
                
            /*
             * in tcsh, -p does nothing, unlike in ksh/bash. we follow the latter.
             */
            case 'p':
                print_separate = 1;
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                break;
                
            case 'l':
                fullpaths = 1;
                flags |= FLAG_DIRSTACK_FULL_PATHS;
                break;
                
            case 'n':
                wrap = 1;
                flags |= FLAG_DIRSTACK_WRAP_ENTRIES;
                break;
                
            case 'c':
                clear = 1;
                break;
                
            /*
             * in tcsh, -L loads the directory stack from a file. the default is used if
             * no argument was given to -L.
             */
            case 'L':
                if(!__optarg || __optarg == INVALID_OPTARG)
                     n = load_dirstack(NULL    );
                else n = load_dirstack(__optarg);
                if(n) purge_dirstack(flags);
                return !n;
                
            /*
             * in tcsh, -S saves the directory stack to a file. the default is used if
             * no argument was given to -S.
             */
            case 'S':
                if(!__optarg || __optarg == INVALID_OPTARG)
                     return save_dirstack(NULL    ), 0;
                else return save_dirstack(__optarg), 0;
        }
    }
    /* we accept unknown options, as they might be -ve dirstack offsets */
    //if(c == -1) return 2;
        
    /* clear the stack, except for the cwd (top of stack) */
    if(clear)
    {
        struct dirstack_ent_s *ds = dirstack->next;
        while(ds)
        {
            struct dirstack_ent_s *next = ds->next;
            free_malloced_str(ds->path);
            free(ds);
            ds = next;
        }
    }

    if(v >= argc)
    {
        purge_dirstack(flags);
        return 0;
    }
  
    //struct passwd *pw = getpwuid(geteuid());
    //char *home = pw ? pw->pw_dir : NULL;
    char *home = get_home();
    int chars = 0;
    
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        if(*arg == '+' || *arg == '-' || isdigit(*arg))
        {
            struct dirstack_ent_s *ds = get_dirstack_entry(arg, NULL);
            if(!ds)
            {
                fprintf(stderr, "dirs: directory stack index out of range: %s\n", arg);
                return 2;
            }
            /* wrap if needed */
            int len = strlen(ds->path);
            if(wrap && chars+len >= VGA_WIDTH)
            {
                printf("\r\n");
                chars = 0;
            }
            if(print_index)
            {
                printf("%3d  ", n++);
                chars += 5;
            }
            print_dir(ds->path, home, fullpaths);
            chars += len+1;
            if(print_separate) putchar('\n');
            else               putchar(' ' );
        }
        else
        {
            fprintf(stderr, "dirs: unknown option: %s\n", arg);
            return 2;
        }
    }
    if(!print_separate) putchar('\n');
    return 0;
}


/*
 * the pushd builtin utility (non-POSIX extension).
 */

int pushd(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int flags = 0, silent = 0;
    int docd = 1;
    int v = 1;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
        
    for(v = 1; v < argc; v++)
    { 
        if(argv[v][0] == '-')
        {
            char *p = argv[v];
            if(strcmp(p, "-") == 0) break;
            if(strcmp(p, "--") == 0) { v++; break; }
            p++;
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], REGULAR_BUILTIN_PUSHD, 1, 0);
                        return 0;
                      
                    /*
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                    */
                        
                    case 'c':
                        docd = 0;
                        break;

                
                    /*
                     * in tcsh, -n, -l and -v has the same effect on pushd as on dirs.
                     */
                    case 'v':
                        flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                        flags |= FLAG_DIRSTACK_PRINT_INDEX;
                        break;
                
                    case 'l':
                        flags |= FLAG_DIRSTACK_FULL_PATHS;
                        break;
                
                    case 'n':
                        flags |= FLAG_DIRSTACK_WRAP_ENTRIES;
                        break;
            
                    /*
                     * in tcsh, -p overrides pushdsilent. to maintain uniformity, this flag will have the same
                     * effect it did in dirs, which is to print entries on separate lines (borrowed from ksh/bash).
                     */
                    case 'p':
                        flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                        break;
                
                    /* this is similar to setting tcsh's pushdsilent variable, which silences pushd & popd */
                    case 's':
                        silent = 1;
                        break;
                
                    default:                        
                        fprintf(stderr, "pushd: unknown option: %c\r\n", *p);
                        return 2;
                }
                p++;
            }
        }
        else break;
    }

    if(v >= argc)
    {
        /*
         * in tcsh, when pushd is called with no arguments and the pushdtohome variable
         * is set, pushd pushes $HOME (similar to what happens with cd). if the variable
         * is not set, pushd behaves as bash's version, which exchanges the top two dirs
         * on the stack.
         */
        if(optionx_set(OPTION_PUSHD_TO_HOME))
        {
            char *home = get_shell_varp("HOME", NULL);
            if(!home)
            {
                fprintf(stderr, "pushd: invalid directory name: $HOME\n");
                return 2;
            }
            char *cwd2 = word_expand_to_str(home);
            if(!cwd2)
            {
                fprintf(stderr, "pushd: invalid directory name: %s\n", cwd2);
                return 2;
            }
            if(cwd) free(cwd);
            cwd = cwd2;
            push_cwd();
        }
        else
        {
            /* exchange the top two directories */
            struct dirstack_ent_s *ds1 = dirstack;
            struct dirstack_ent_s *ds2 = ds1->next;
            if(ds2)        /* more than one entry in stack */
            {
                dirstack = ds2;
                ds1->next = ds2->next;
                ds1->prev = ds2;
                ds2->next = ds1;
                ds2->prev = NULL;
            }
        }
        /* now cd to the new directory */
        if(docd && !dirs_cd()) return 2;
        if(!silent) purge_dirstack(flags);
        return 0;
    }
  
    for( ; v < argc; v++)
    {
        /* bring the requested item to the top of the stack */
        char *arg = argv[v];
        /* tcsh's pushd recognizes "-" to mean $OLDPWD, just as cd */
        if(strcmp(arg, "-") == 0)
        {
            char *pwd = get_shell_varp("OLDPWD", NULL);
            if(!pwd)
            {
                fprintf(stderr, "pushd: invalid directory name: %s\n", arg);
                return 2;
            }
            if(cwd) free(cwd);
            cwd = __get_malloced_str(pwd);
            push_cwd();
        }
        else if(*arg == '+' || *arg == '-' || isdigit(*arg))
        {
            struct dirstack_ent_s *ds = get_dirstack_entry(arg, NULL);
            if(!ds)
            {
                fprintf(stderr, "pushd: directory stack index out of range: %s\n", arg);
                return 2;
            }
            /*
             * in tcsh, if the dextract variable is set, pushd extracts the n-th directory
             * and pushes it on top of the stack, instead of rotating the stack.
             */
            if(optionx_set(OPTION_DEXTRACT))
            {
                /* extract the directory */
                if(ds->prev) ds->prev->next = ds->next;
                if(ds->next) ds->next->prev = ds->prev;
                ds->prev = NULL;
                if(dirstack != ds) ds->next = dirstack;     /* avoid linking to self */
                dirstack = ds;
            }
            else
            {
                /* rotate the stack. link the last item to the first */
                struct dirstack_ent_s *ds2 = ds;
                while(ds2->next) ds2 = ds2->next;
                ds2->next = dirstack;
                /* unlink the prev item to the current item */
                ds2 = dirstack;
                dirstack = ds;
                while(ds2->next != ds) ds2 = ds2->next;
                ds2->next = NULL;
            }
        }
        else
        {
            char *cwd2 = word_expand_to_str(arg);
            if(!cwd2)
            {
                fprintf(stderr, "pushd: invalid directory name: %s\n", arg);
                return 2;
            }
            /*
             * in tcsh, if the dunique variable is set, pushd removes all instances 
             * of dir from the stack.
             */
            if(optionx_set(OPTION_DUNIQUE))
            {
                struct dirstack_ent_s *ds1 = dirstack;
                while(ds1)
                {
                    if(strcmp(ds1->path, cwd2) == 0)
                    {
                        struct dirstack_ent_s *ds2 = ds1->next;
                        if(ds1->prev) ds1->prev->next = ds1->next;
                        if(ds1->next) ds1->next->prev = ds1->prev;
                        free_malloced_str(ds1->path);
                        free(ds1);
                        if(dirstack == ds1) dirstack = NULL;
                        ds1 = ds2;
                        stack_count--;
                    }
                    else ds1 = ds1->next;
                }
            }
            if(cwd) free(cwd);
            cwd = cwd2;
            push_cwd();
        }
    }
    
    /* now cd to the new directory */
    if(docd && !dirs_cd()) return 2;
    if(!silent) purge_dirstack(flags);
    return 0;
}


/*
 * the popd builtin utility (non-POSIX extension).
 */

int popd(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int flags = 0, silent = 0;
    int docd = 1;
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hclnspv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_POPD, 1, 0);
                return 0;
                
            /*
            case 'v':
                printf("%s", shell_ver);
                return 0;
            */
                
            case 'c':
                docd = 0;
                break;
                
            /*
             * in tcsh, -n, -l and -v has the same effect on popd as on dirs.
             */
            case 'v':
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                flags |= FLAG_DIRSTACK_PRINT_INDEX;
                break;
                
            case 'l':
                flags |= FLAG_DIRSTACK_FULL_PATHS;
                break;
                
            case 'n':
                flags |= FLAG_DIRSTACK_WRAP_ENTRIES;
                break;
            
            /*
             * in tcsh, -p overrides pushdsilent. to maintain uniformity, this flag will have the same
             * effect it did in dirs, which is to print entries on separate lines (borrowed from ksh/bash).
             */
            case 'p':
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                break;
                
            /* this is similar to setting tcsh's pushdsilent variable, which silences pushd & popd */
            case 's':
                silent = 1;
                break;
        }
    }
    /* we accept unknown options, as they might be -ve dirstack offsets */
    //if(c == -1) return 2;

    if(v >= argc)
    {
        /* pop the top of the stack */
        struct dirstack_ent_s *ds1 = dirstack;
        if(dirstack->next)
        {
            dirstack = dirstack->next;
            if(dirstack) dirstack->prev = NULL;
            free_malloced_str(ds1->path);
            free(ds1);

            /* now cd to the new directory */
            if(docd && !dirs_cd()) return 2;
        }
        if(!silent) purge_dirstack(flags);
        return 0;
    }
  
    for( ; v < argc; v++)
    {
        /* bring the requested item to the top of the stack */
        char *arg = argv[v];
        if(*arg == '+' || *arg == '-' || isdigit(*arg))
        {
            struct dirstack_ent_s *prev = NULL;
            struct dirstack_ent_s *ds = get_dirstack_entry(arg, &prev);
            if(!ds)
            {
                fprintf(stderr, "popd: directory stack index out of range: %s\n", arg);
                return 2;
            }
            /* now remove the item */
            if(prev)
            {
                prev->next = ds->next;
                free_malloced_str(ds->path);
                free(ds);
                stack_count--;
            }
            else
            {
                /* don't remove the last item in the stack */
                if(ds->next)
                {
                    dirstack = ds->next;
                    free_malloced_str(ds->path);
                    free(ds);
                    stack_count--;
                }
            }
        }
        else
        {
            fprintf(stderr, "popd: invalid directory stack index: %s\n", arg);
            return 2;
        }
    }
    
    /* now cd to the new directory */
    if(docd && !dirs_cd()) return 2;
    if(!silent) purge_dirstack(flags);
    return 0;
}
