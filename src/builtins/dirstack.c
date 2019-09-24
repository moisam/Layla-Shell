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

struct dirstack_ent_s *dirstack = NULL;     /* the directory stack */
int    stack_count   = 0;                   /* count of stack entries */
int    read_dirsfile = 0;                   /* flag to load dirstack from an external file */

int    push_cwd();
int    push_dir(char *dir, struct dirstack_ent_s **stack, int *count);


/*
 * initialize the dirstack (called on shell startup), loading the stack from
 * an external file if the read_dirsfile flag is non-zero, or pushing the current
 * working directory as the sole entry on the stack if the flag is zero.
 * 
 * returns 1 on success, 0 on failure.
 */
int init_dirstack()
{
    if(read_dirsfile)
    {
        return load_dirstack(NULL);
    }
    else
    {
        return push_cwd();
    }
}


/*
 * free the memory used by a dirstack entries.
 */
void __free_dirstack(struct dirstack_ent_s *ds)
{
    while(ds)
    {
        struct dirstack_ent_s *next = ds->next;
        if(ds->path)
        {
            free_malloced_str(ds->path);
        }
        free(ds);
        ds = next;
    }
}


/*
 * free the memory used by the dirstack structure entries and set the
 * dirstack pointer to NULL and the stack count to zero.
 */
void free_dirstack()
{
    __free_dirstack(dirstack);
    dirstack = NULL;
    stack_count = 0;
}


/*
 * called on startup by a login shell, or a shell passed the --dirsfile option,
 * or by the dirs builtin when its passed the -L option.. this function loads
 * the directory stack from the file specified in __path.. the file must contain
 * a series of 'pushd' commands, optionally intermixed with comment lines (that
 * start with '#') or empty lines.. no other commands are allowed, and each line
 * must contain only one command.. generally, you don't create such a file by hand,
 * the shell automatically creates it on shutdown if its a login shell and the
 * extended option OPTION_SAVE_DIRS is set (see the definition of the exit_gracefully()
 * function in exit.c).
 * 
 * returns 1 on success, 0 on failure.
 */
int load_dirstack(char *__path)
{
    char *path = NULL;
    /* we have a path (i.e. 'dirs -L' was invoked with a path) */
    if(__path)
    {
        path = __path;
    }
    /*
     * we have no path (i.e. 'dirs -L' was invoked with no path, or we're being called
     * on shell shutdwn).. in this case, we use $DIRSFILE or, if its NULL, the default
     * path: ~/.lshdirs.
     */
    else
    {
        path = get_shell_varp("DIRSFILE", DIRSTACK_FILE);
    }
    /* perform word expansion on the path */
    path = word_expand_to_str(path);
    if(!path)
    {
        return push_cwd();
    }
    /* open the file and read the commands, line by line */
    FILE *f = fopen(path, "r");
    struct dirstack_ent_s *newstack = NULL;
    int newcount = 0;
    if(f)
    {
        int  linemax = get_linemax();
        char buf[linemax];
        char *line = NULL;
        /* get the next line from file */
        while((line = fgets(buf, linemax, f)))
        {
            if(*line == '\0' || *line == '#') continue;       /* don't add empty or commented lines */
            /* 
             * dirs are saved in the file in reverse order of that of the stack.
             * the format of each entry is: "pushd dir\n".
             */
            char *p = strstr(line, "pushd ");
            /*
             * NOTE: for now, we only parse 'pushd' commands when reading the dirs file,
             *       as they are the only thing we need.. however, tcsh allows the dirs file
             *       to contain 'cd' commands.. if you want to parse and use 'cd' commands in
             *       the dirs file, remove the following if and endif marcos.
             */
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
            p += 6;     /* skip the 'pushd ' part of the line */
            while(*p && isspace(*p))
            {
                p++;   /* skip spaces after the 'pushd ' part */
            }
            if(!p)      /* line with 'pushd ' but no arguments. skip */
            {
                continue;
            }
            /* overwrite the '\n' so we can pass the argument to cd */
            int n = strlen(p)-1;
            if(p[n] == '\n')
            {
                p[n] = '\0';
            }
            /*
             * cd to the directory, so we can check it exists and is a directory.
             * this step is also important if the file contains pushd commands with
             * relative (vs absolute) directory paths.
             */
            if(cd(2, (char *[]){ "cd", p, NULL }) != 0)
            {
                /*
                * TODO: do something better than just b*^%$ing about it.
                */
                fprintf(stderr, "%s: failed to cd to %s\n", SHELL_NAME, path);
                goto err;
            }
            /* push the directory on the stack */
            if(!push_dir(p, &newstack, &newcount))
            {
                break;
            }
            newcount++;
        }
        fclose(f);
    }
    else
    {
        /* error opening the dirs file */
        fprintf(stderr, "%s: failed to load dirstack from %s: %s\n",
                SHELL_NAME, path, strerror(errno));
    }
    free(path);
    if(!newstack || !newcount)
    {
        goto err;
    }

    /* free the old stack */
    free_dirstack();

    /* save the new one */
    dirstack = newstack;
    stack_count = newcount;
    if(!stack_count)
    {
        push_cwd();
    }
    /* cd to the dir on top of the stack if its not the same as our cwd */
    if(!cwd || strcmp(dirstack->path, cwd) != 0)
    {
        cd(2, (char *[]){ "cd", dirstack->path, NULL });
    }
    return 1;
    
err:
    if(newstack)
    {
        /* free the memory used by the partially parsed new dirstack */
        __free_dirstack(newstack);
    }
    return 0;
}


/*
 * load the directory stack with the values in val.. entries must be separated by
 * whitespace characters.. the stack will be cleared before assigning new entries
 * and cd'ing to the directory on top of the stack.
 * 
 * returns 1 on success, 0 on failure.
 */
int load_dirstackp(char *val)
{
    if(!val || !*val)
    {
        return 0;
    }
    /* 
     * dirs are saved in the string in the same order they were stored in the stack, so we
     * traverse the string and add each entry to the stack.
     */
    char *p = val, *p2;
    struct dirstack_ent_s *newstack = NULL, *last = NULL;
    int newcount = 0;
    while(*p)
    {
        while(*p && isspace(*p))    /* skip leading spaces */
        {
            p++;
        }
        if(!p)                      /* end of string */
        {
            break;
        }
        p2 = p;
        while(*p2)                  /* get next word boundary */
        {
            if(isspace(*p2) && p2[-1] != '\\')
            {
                break;
            }
            p2++;
        }
        /* extract the next dir path from the string */
        char *path = get_malloced_strl(p, 0, p2-p);
        if(!path)
        {
            break;
        }
        int n = strlen(path)-1;
        /* remove the '\n' from the dir path (happens if dir is the last entry on the line) */
        if(path[n] == '\n')
        {
            path[n] = '\0';
        }
        /* cd to the directory */
        if(cd(2, (char *[]){ "cd", path, NULL }) != 0)
        {
            /*
             * TODO: do something better than just b*^%$ing about it.
             */
            fprintf(stderr, "%s: failed to cd to %s\n", SHELL_NAME, path);
            free_malloced_str(path);
            goto err;
        }
        /*
         * push the directory on the stack, but we can't call push_dir() as we did in
         * load_dirstack() as we need to push the entries in reverse order, so we'll do
         * it manually here.
         */
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
            newstack   = ds;
            last       = ds;
        }
        else
        {
            last->next = ds;
            ds->prev   = last;
            last       = ds;
        }
        newcount++;
        p = p2;
    }
    if(!newstack || !newcount)
    {
        goto err;
    }
    
    /* free the old stack */
    free_dirstack();

    /* save the new one */
    dirstack = newstack;
    stack_count = newcount;
    if(!stack_count)
    {
        push_cwd();
    }
    /* cd to the dir on top of the stack if its not the same as our cwd */
    if(!cwd || strcmp(dirstack->path, cwd) != 0)
    {
        cd(2, (char *[]){ "cd", dirstack->path, NULL });
    }
    return 1;
    
err:
    if(newstack)
    {
        /* free the memory used by the partially parsed new dirstack */
        __free_dirstack(newstack);
    }
    return 0;
}


/*
 * save the directory stack to the external file specified in the __path parameter.
 * called on shutdown by the login shell if the extended option OPTION_SAVE_DIRS is set,
 * and by the dirs builtin when its passed the -S option.
 */
void save_dirstack(char *__path)
{
    char *path = NULL;
    /* we have a path (i.e. 'dirs -S' was invoked with a path) */
    if(__path)
    {
        path = __path;
    }
    /*
     * we have no path (i.e. 'dirs -S' was invoked with no path, or we're being called
     * on shell shutdwn).. in this case, we use $DIRSFILE or, if its NULL, the default
     * path: ~/.lshdirs.
     */
    else
    {
        path = get_shell_varp("DIRSFILE", DIRSTACK_FILE);
    }
    /* perform word expansion on the path */
    path = word_expand_to_str(path);
    /* empty path */
    if(!path)
    {
        return;
    }
    /* open the file and write out the dirstack entries to it */
    FILE *f = fopen(path, "w");
    if(f)
    {
        /*
         * start from the bottom of the stack, so the entries are read in the correct
         * order the next time we load the file.
         */
        struct dirstack_ent_s *ds = dirstack;
        while(ds->next)
        {
            ds = ds->next;
        }
        while(ds)
        {
            if(ds->path[0])
            {
                fprintf(f, "pushd %s\n", ds->path);
            }
            ds = ds->prev;
        }
    }
    else
    {
        /* error opening the file */
        fprintf(stderr, "%s: failed to save dirstack to %s: %s\n", SHELL_NAME, path, strerror(errno));
    }
    free(path);
}


/*
 * fetch the n-th entry in the dirstack, counting is zero based.. negative counts
 * start from the end of the list.
 * 
 * returns the dirstack entry, or NULL if n is out of bounds or the stack is empty.
 */
struct dirstack_ent_s *get_dirstack_entryn(int n, struct dirstack_ent_s **__prev)
{
    char buf[16];
    sprintf(buf, "%d", n);
    return get_dirstack_entry(buf, __prev);
}


/*
 * fetch the n-th entry in the dirstack, counting is zero based.. negative counts
 * start from the end of the list.. n is the numeric value we get from parsing the
 * nstr argument.
 * 
 * returns the dirstack entry, or NULL if n is out of bounds or the stack is empty.
 */
struct dirstack_ent_s *get_dirstack_entry(char *nstr, struct dirstack_ent_s **__prev)
{
    if(!nstr)
    {
        return NULL;
    }

    char *strend = NULL;
    char c = '+';
    /* skip any leading - or + sign (and remember it) */
    if(*nstr == '-' || *nstr == '+')
    {
        c = *nstr++;
    }

    int n = strtol(nstr, &strend, 10);
    if(strend == nstr)          /* error parsing number */
    {
        return NULL;
    }
    if(c == '-')                /* count from the right, or end of list */
    {
        if(n == 0)
        {
            n = stack_count-1;
        }
        else
        {
            n = stack_count-1-n;
        }
    }
    if(n >= stack_count)        /* invalid number given */
    {
        return NULL;
    }
    struct dirstack_ent_s *ds   = dirstack;
    struct dirstack_ent_s *prev = NULL    ;
    /* get the n-th stack entry */
    while(n-- && ds)
    {
        prev = ds;
        ds = ds->next;
    }
    if(__prev)
    {
        *__prev = prev;
    }
    return ds;
}


/*
 * print the given directory path, contracting home to ~ if needed.
 */
void print_dir(char *path, char *home, int fullpaths)
{
    /* print the directory as it is */
    if(fullpaths || !home)
    {
        printf("%s", path);
    }
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


/*
 * print the contents of the directory stack.
 */
void purge_dirstack(int flags)
{
    int fullpaths      = flag_set(flags, FLAG_DIRSTACK_FULL_PATHS    );
    int print_separate = flag_set(flags, FLAG_DIRSTACK_SEPARATE_LINES);
    int print_index    = flag_set(flags, FLAG_DIRSTACK_PRINT_INDEX   );
    int wrap           = flag_set(flags, FLAG_DIRSTACK_WRAP_ENTRIES  );
    struct dirstack_ent_s *ds = dirstack;
    int i = 0, n = 0;
    /* as in tcsh, -v takes precedence over -n */
    if(print_index && wrap)
    {
        wrap = 0;
    }

    while(ds)
    {
        char *home = get_home();
        /* wrap if needed */
        int len = strlen(ds->path);
        if(wrap && n+len >= VGA_WIDTH)
        {
            printf("\n");
            n = 0;
        }
        if(print_index)
        {
            printf("%3d  ", i++);
            n += 5;
        }
        print_dir(ds->path, home, fullpaths);
        n += len+1;
        if(print_separate)
        {
            putchar('\n');
        }
        else if(ds->next)
        {
            putchar(' ');
        }
        ds = ds->next;
    }
    if(!print_separate)
    {
        putchar('\n');
    }
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
    if(!p)
    {
        return NULL;
    }
    p[0] = '\0';
    /* now copy the entries */
    ds = dirstack;
    while(ds)
    {
        strcat(p, ds->path);
        if(ds->next)
        {
            strcat(p, " ");
        }
        ds = ds->next;
    }
    return p;
}


/*
 * push a directory on the given directory stack.
 *
 * returns 1 if the directory is pushed on the stack, 0 in case of error.
 * count is incremented by 1 after the push.
 */
int push_dir(char *dir, struct dirstack_ent_s **stack, int *count)
{
    char *path = get_malloced_str(dir);
    if(!path)
    {
        return 0;
    }
    struct dirstack_ent_s *ds = malloc(sizeof(struct dirstack_ent_s));
    if(!ds)
    {
        free_malloced_str(path);
        return 0;
    }
    ds->path = path;
    ds->next = *stack;
    ds->prev = NULL;
    if(*stack)
    {
        (*stack)->prev = ds;
    }
    *stack = ds;
    (*count) = (*count)+1;
    return 1;
}


/*
 * push the current working directory on the directory stack.
 *
 * returns 1 if the directory is pushed on the stack, 0 in case of error.
 * stack_count is incremented by 1 after the push.
 */
int push_cwd()
{
    return push_dir(cwd, &dirstack, &stack_count);
}


/*
 * cd to the directory on the top of the directory stack.
 *
 * returns 1 on success, 0 in case of error.
 */
int dirs_cd()
{
    if(cd(2, (char *[]){ "cd", dirstack->path, NULL }) != 0)
    {
        return 0;
    }
    /* make sure we have a full path in the stack entry */
    if(dirstack->path[0] != '/' && dirstack->path[0] != '~')
    {
        free_malloced_str(dirstack->path);
        dirstack->path = get_malloced_str(cwd);
    }
    return 1;
}


/*
 * the dirs builtin utility (non-POSIX).. used to print, save and load the
 * directory stack.
 * returns 0 on success, non-zero otherwise.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help dirs` or `dirs -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int dirs(int argc, char **argv)
{
    int clear = 0, fullpaths = 0, print_separate = 0, print_index = 0, n = 0;
    int v = 1, c;
    int flags = 0, wrap = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
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
                {
                    n = load_dirstack(NULL    );
                }
                else
                {
                    n = load_dirstack(__optarg);
                }
                if(n)
                {
                    purge_dirstack(flags);
                }
                return !n;
                
            /*
             * in tcsh, -S saves the directory stack to a file. the default is used if
             * no argument was given to -S.
             */
            case 'S':
                if(!__optarg || __optarg == INVALID_OPTARG)
                {
                    return save_dirstack(NULL    ), 0;
                }
                else
                {
                    return save_dirstack(__optarg), 0;
                }
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
    /* no arguments. print the dirstack and return */
    if(v >= argc)
    {
        purge_dirstack(flags);
        return 0;
    }
  
    char *home = get_home();
    int chars = 0;
    /* parse the arguments */
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
                printf("\n");
                chars = 0;
            }
            if(print_index)
            {
                printf("%3d  ", n++);
                chars += 5;
            }
            print_dir(ds->path, home, fullpaths);
            chars += len+1;
            if(print_separate)
            {
                putchar('\n');
            }
            else
            {
                putchar(' ' );
            }
        }
        else
        {
            fprintf(stderr, "dirs: unknown option: %s\n", arg);
            return 2;
        }
    }
    if(!print_separate)
    {
        putchar('\n');
    }
    return 0;
}


/*
 * the pushd builtin utility (non-POSIX extension).. used to push a directory on
 * the directory stack. returns 0 on success, non-zero otherwise.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help pushd` or `pushd -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int pushd(int argc, char **argv)
{
    int flags = 0, silent = 0;
    int docd = 1;
    int v = 1;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    for(v = 1; v < argc; v++)
    { 
        if(argv[v][0] == '-')
        {
            char *p = argv[v];
            /* special option '-' */
            if(strcmp(p, "-") == 0)
            {
                break;
            }
            /* special option '--' */
            if(strcmp(p, "--") == 0)
            {
                v++;
                break;
            }
            /* skip the '-' and parse the options */
            p++;
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], REGULAR_BUILTIN_PUSHD, 1, 0);
                        return 0;
                        
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
                        fprintf(stderr, "pushd: unknown option: %c\n", *p);
                        return 2;
                }
                p++;
            }
        }
        else
        {
            break;
        }
    }
    /* no arguments */
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
            if(cwd)
            {
                free(cwd);
            }
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
        if(docd && !dirs_cd())
        {
            return 2;
        }
        if(!silent)
        {
            purge_dirstack(flags);
        }
        return 0;
    }
    /* now parse the arguments */
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
            if(cwd)
            {
                free(cwd);
            }
            cwd = __get_malloced_str(pwd);
            push_cwd();
        }
        /* numeric argument */
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
                if(ds->prev)
                {
                    ds->prev->next = ds->next;
                }
                if(ds->next)
                {
                    ds->next->prev = ds->prev;
                }
                ds->prev = NULL;
                if(dirstack != ds)
                {
                    ds->next = dirstack;     /* avoid linking to self */
                }
                dirstack = ds;
            }
            else
            {
                /* rotate the stack. link the last item to the first */
                struct dirstack_ent_s *ds2 = ds;
                while(ds2->next)
                {
                    ds2 = ds2->next;
                }
                ds2->next = dirstack;
                /* unlink the prev item to the current item */
                ds2 = dirstack;
                dirstack = ds;
                while(ds2->next != ds)
                {
                    ds2 = ds2->next;
                }
                ds2->next = NULL;
            }
        }
        /* non-numeric argument */
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
                        if(ds1->prev)
                        {
                            ds1->prev->next = ds1->next;
                        }
                        if(ds1->next)
                        {
                            ds1->next->prev = ds1->prev;
                        }
                        free_malloced_str(ds1->path);
                        free(ds1);
                        if(dirstack == ds1)
                        {
                            dirstack = NULL;
                        }
                        ds1 = ds2;
                        stack_count--;
                    }
                    else
                    {
                        ds1 = ds1->next;
                    }
                }
            }
            if(cwd)
            {
                free(cwd);
            }
            cwd = cwd2;
            push_cwd();
        }
    }
    /* now cd to the new directory */
    if(docd && !dirs_cd())
    {
        return 2;
    }
    if(!silent)
    {
        purge_dirstack(flags);
    }
    return 0;
}


/*
 * the popd builtin utility (non-POSIX extension).. used to pop a directory from
 * the directory stack. returns 0 on success, non-zero otherwise.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help popd` or `popd -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int popd(int argc, char **argv)
{
    int flags = 0, silent = 0;
    int docd = 1;
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hclnspv", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_POPD, 1, 0);
                return 0;
                
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
    /* no arguments */
    if(v >= argc)
    {
        /* pop the top of the stack */
        struct dirstack_ent_s *ds1 = dirstack;
        if(dirstack->next)
        {
            dirstack = dirstack->next;
            if(dirstack)
            {
                dirstack->prev = NULL;
            }
            free_malloced_str(ds1->path);
            free(ds1);

            /* now cd to the new directory */
            if(docd && !dirs_cd())
            {
                return 2;
            }
        }
        if(!silent)
        {
            purge_dirstack(flags);
        }
        return 0;
    }
    /* now parse the arguments */
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
    if(docd && !dirs_cd())
    {
        return 2;
    }
    if(!silent)
    {
        purge_dirstack(flags);
    }
    return 0;
}
