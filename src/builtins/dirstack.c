/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020, 2024 (c)
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
#include <sys/stat.h>
#include "builtins.h"
#include "setx.h"
#include "../include/cmd.h"
#include "../symtab/symtab.h"
#include "../include/debug.h"

struct dirstack_ent_s *dirstack = NULL;     /* the directory stack */
int    stack_count   = 0;                   /* count of stack entries */
int    read_dirsfile = 0;                   /* flag to load dirstack from an external file */

int    push_dir(char *dir, struct dirstack_ent_s **stack, int *count, char *utility);


/*
 * Initialize the dirstack (called on shell startup), loading the stack from
 * an external file if the read_dirsfile flag is non-zero, or pushing the current
 * working directory as the sole entry on the stack if the flag is zero.
 * 
 * Returns 1 on success, 0 on failure.
 */
int init_dirstack(void)
{
    if(read_dirsfile)
    {
        return load_dirstack(NULL);
    }
    else
    {
        return push_cwd(SHELL_NAME);
    }
}


/*
 * Free the memory used by a dirstack entries.
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
 * Free the memory used by the dirstack structure entries and set the
 * dirstack pointer to NULL and the stack count to zero.
 */
void free_dirstack(void)
{
    __free_dirstack(dirstack);
    dirstack = NULL;
    stack_count = 0;
}


/*
 * Called on startup by a login shell, or a shell passed the --dirsfile option,
 * or by the dirs builtin when its passed the -L option. This function loads
 * the directory stack from the file specified in __path. The file must contain
 * a series of 'pushd' commands, optionally intermixed with comment lines (that
 * start with '#') or empty lines. No other commands are allowed, and each line
 * must contain only one command. Generally, you don't create such a file by hand,
 * the shell automatically creates it on shutdown if its a login shell and the
 * extended option OPTION_SAVE_DIRS is set (see the definition of the exit_gracefully()
 * function in exit.c).
 * 
 * Returns 1 on success, 0 on failure.
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
     * We have no path (i.e. 'dirs -L' was invoked with no path, or we're being called
     * on shell shutdwn). In this case, we use $DIRSFILE or, if its NULL, the default
     * path: ~/.lshdirs.
     */
    else
    {
        path = get_shell_varp("DIRSFILE", DIRSTACK_FILE);
    }

    /* perform word expansion on the path */
    path = word_expand_to_str(path, FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES);
    if(!path)
    {
        return push_cwd(SHELL_NAME);
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
             * Dirs are saved in the file in reverse order of that of the stack.
             * the format of each entry is: "pushd dir\n".
             */
            char *p = strstr(line, "pushd ");
            /*
             * NOTE: For now, we only parse 'pushd' commands when reading the dirs file,
             *       as they are the only thing we need. However, tcsh allows the dirs file
             *       to contain 'cd' commands. If you want to parse and use 'cd' commands in
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
                do_builtin_internal(cd_builtin, 2, (char *[]){ "cd", p, NULL });
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
            if(do_builtin_internal(cd_builtin, 2, (char *[]){ "cd", p, NULL }) != 0)
            {
                /*
                * TODO: do something better than just b*^%$ing about it.
                */
                PRINT_ERROR(SHELL_NAME, "failed to cd to %s", path);
                goto err;
            }
            
            /* push the directory on the stack */
            if(!push_dir(p, &newstack, &newcount, SOURCE_NAME))
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
        PRINT_ERROR(SHELL_NAME, "failed to load dirstack from %s: %s",
                path, strerror(errno));
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
        push_cwd(SOURCE_NAME);
    }

    /* cd to the dir on top of the stack if its not the same as our cwd */
    if(!cwd || strcmp(dirstack->path, cwd) != 0)
    {
        do_builtin_internal(cd_builtin, 2, (char *[]){ "cd", dirstack->path, NULL });
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
 * Load the directory stack with the values in val. Entries must be separated by
 * whitespace characters. The stack will be cleared before assigning new entries
 * and cd'ing to the directory on top of the stack.
 * 
 * Returns 1 on success, 0 on failure.
 */
int load_dirstackp(char *val)
{
    if(!val || !*val)
    {
        return 0;
    }
    /* 
     * Dirs are saved in the string in the same order they were stored in the stack, so we
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
        if(do_builtin_internal(cd_builtin, 2, (char *[]){ "cd", path, NULL }) != 0)
        {
            /*
             * TODO: do something better than just b*^%$ing about it.
             */
            PRINT_ERROR(SHELL_NAME, "failed to cd to %s", path);
            free_malloced_str(path);
            goto err;
        }
        
        /*
         * Push the directory on the stack, but we can't call push_dir() as we did in
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
        push_cwd(SHELL_NAME);
    }
    
    /* cd to the dir on top of the stack if its not the same as our cwd */
    if(!cwd || strcmp(dirstack->path, cwd) != 0)
    {
        do_builtin_internal(cd_builtin, 2, (char *[]){ "cd", dirstack->path, NULL });
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
 * Save the directory stack to the external file specified in the __path parameter.
 * Called on shutdown by the login shell if the extended option OPTION_SAVE_DIRS is set,
 * and by the dirs builtin when its passed the -S option.
 */
void save_dirstack(char *__path)
{
    /* we have no dirstack */
    if(!dirstack)
    {
        return;
    }
    
    char *path = NULL;
    /* we have a path (i.e. 'dirs -S' was invoked with a path) */
    if(__path)
    {
        path = __path;
    }
    /*
     * We have no path (i.e. 'dirs -S' was invoked with no path, or we're being called
     * on shell shutdown). In this case, we use $DIRSFILE or, if its NULL, the default
     * path: ~/.lshdirs.
     */
    else
    {
        path = get_shell_varp("DIRSFILE", NULL);
        if(!path)
        {
            path = __get_malloced_str(DIRSTACK_FILE);
        }
    }

    /* perform word expansion on the path */
    path = word_expand_to_str(path, FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES);

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
        PRINT_ERROR(SHELL_NAME, "failed to save dirstack to %s: %s", 
                    path, strerror(errno));
    }
    
    free(path);
}


/*
 * Fetch the n-th entry in the dirstack, counting is zero based. Negative counts
 * start from the end of the list.
 * 
 * Returns the dirstack entry, or NULL if n is out of bounds or the stack is empty.
 */
struct dirstack_ent_s *get_dirstack_entryn(int n, struct dirstack_ent_s **__prev)
{
    char buf[16];
    sprintf(buf, "%d", n);
    return get_dirstack_entry(buf, __prev, NULL);
}


/*
 * Fetch the n-th entry in the dirstack, counting is zero based. Negative counts
 * start from the end of the list. n is the numeric value we get from parsing the
 * nstr argument.
 * 
 * Returns the dirstack entry, or NULL if n is out of bounds or the stack is empty.
 */
struct dirstack_ent_s *get_dirstack_entry(char *nstr, struct dirstack_ent_s **__prev, int *n)
{
    char *strend = NULL;
    char c = '+';
    
    if(!nstr)
    {
        return NULL;
    }

    /* skip any leading - or + sign (and remember it) */
    if(*nstr == '-' || *nstr == '+')
    {
        c = *nstr++;
    }

    int n2 = strtol(nstr, &strend, 10);
    if(*strend)          /* error parsing number, or invalid digit in number */
    {
        return NULL;
    }

    if(c == '-')                /* count from the right, or end of list */
    {
        if(n2 == 0)
        {
            n2 = stack_count-1;
        }
        else
        {
            n2 = stack_count-1-n2;
        }
    }
    
    if(n2 >= stack_count)        /* invalid number given */
    {
        return NULL;
    }
    
    if(n)
    {
        (*n) = n2;
    }
    
    struct dirstack_ent_s *ds   = dirstack;
    struct dirstack_ent_s *prev = NULL    ;
    
    /* get the n-th stack entry */
    while(n2-- && ds)
    {
        prev = ds;
        ds = ds->next;
    }
    
    if(__prev)
    {
        (*__prev) = prev;
    }
    
    return ds;
}


/*
 * Print the given directory path, contracting home to ~ if needed.
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
 * Print the contents of the directory stack.
 */
void purge_dirstack(int flags)
{
    int fullpaths      = flag_set(flags, FLAG_DIRSTACK_FULL_PATHS    );
    int print_separate = flag_set(flags, FLAG_DIRSTACK_SEPARATE_LINES);
    int print_index    = flag_set(flags, FLAG_DIRSTACK_PRINT_INDEX   );
    int wrap           = flag_set(flags, FLAG_DIRSTACK_WRAP_ENTRIES  );
    int i = 0;
    size_t n = 0;
    struct dirstack_ent_s *ds = dirstack;
    char *home = get_home(1);

    /* as in tcsh, -v takes precedence over -n */
    if(print_index && wrap)
    {
        wrap = 0;
    }

    while(ds)
    {
        /* wrap if needed */
        size_t len = strlen(ds->path);
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
    
    if(!print_separate && dirstack)
    {
        putchar('\n');
    }
}


/*
 * Same as purge_dirstack() above, but saves the output in a string, instead of
 * sending it to stdout.
 */
char *purge_dirstackp(void)
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
 * Push a directory on the given directory stack.
 * 'utility' contains the name of the calling builtin utility.
 *
 * Returns 1 if the directory is pushed on the stack, 0 in case of error.
 * count is incremented by 1 after the push.
 */
int push_dir(char *dir, struct dirstack_ent_s **stack, int *count, char *utility)
{
    struct stat st;
    if(stat(dir, &st) == 0)
    {
        /* check if the new path is a directory */
        if(!S_ISDIR(st.st_mode))
        {
            PRINT_ERROR(utility, "cannot push `%s`: %s", dir, strerror(ENOTDIR));
            return 0;
        }
    }
    else
    {
        PRINT_ERROR(utility, "cannot push `%s`: %s", dir, strerror(ENOENT));
        return 0;
    }

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
        struct dirstack_ent_s *prev = (*stack)->prev;
        if(prev)
        {
            prev->next = ds;
            ds->prev = prev;
        }
        (*stack)->prev = ds;
    }

    *stack = ds;
    (*count) = (*count)+1;
    return 1;
}


/*
 * Push the current working directory on the directory stack.
 * 'utility' contains the name of the calling builtin utility.
 *
 * Returns 1 if the directory is pushed on the stack, 0 in case of error.
 * stack_count is incremented by 1 after the push.
 */
int push_cwd(char *utility)
{
    return push_dir(cwd, &dirstack, &stack_count, utility);
}


/*
 * cd to the directory on the top of the directory stack.
 *
 * Returns 1 on success, 0 in case of error.
 */
int dirs_cd(void)
{
    if(do_builtin_internal(cd_builtin, 2, (char *[]){ "cd", dirstack->path, NULL }) != 0)
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
 * The dirs builtin utility (non-POSIX). Used to print, save and load the
 * directory stack.
 * 
 * Returns 0 on success, non-zero otherwise.
 * 
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help dirs` or `dirs -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int dirs_builtin(int argc, char **argv)
{
    int clear = 0, fullpaths = 0, print_separate = 0, print_index = 0, n = 0;
    int v = 1, c;
    int flags = 0, wrap = 0;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hclpvwL:S:", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'c':
                clear = 1;
                break;
                
            case 'h':
                print_help(argv[0], &DIRS_BUILTIN, 0);
                return 0;
                
            /*
             * in tcsh, -p does nothing, unlike in ksh/bash. we follow the latter.
             */
            case 'p':
                print_separate = 1;
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                break;
                
            /*
             * In tcsh, -n, -l and -v manipulate the output format.
             * We use -w instead of -n because, although we don't use -n here,
             * we use it in pushd and popd to suppress cd in order to be 
             * bash-compatible. To keep consistency between dirs, pushd, and 
             * popd, we use -w instead of -n for all three utilities.
             */
            case 'l':
                fullpaths = 1;
                flags |= FLAG_DIRSTACK_FULL_PATHS;
                break;
                
            case 'v':
                print_index = 1;
                print_separate = 1;
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                flags |= FLAG_DIRSTACK_PRINT_INDEX;
                break;
                
            case 'w':
                wrap = 1;
                flags |= FLAG_DIRSTACK_WRAP_ENTRIES;
                break;
                
            /*
             * In tcsh, -L loads the directory stack from a file. The default is used if
             * no argument was given to -L.
             */
            case 'L':
                if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                {
                    n = load_dirstack(NULL);
                }
                else
                {
                    n = load_dirstack(internal_optarg);
                }

                if(n)
                {
                    purge_dirstack(flags);
                }

                return !n;
                
            /*
             * In tcsh, -S saves the directory stack to a file. The default is used if
             * no argument was given to -S.
             */
            case 'S':
                if(!internal_optarg || internal_optarg == INVALID_OPTARG)
                {
                    save_dirstack(NULL);
                }
                else
                {
                    save_dirstack(internal_optarg);
                }
                return 0;
        }
    }
    /* we accept unknown options, as they might be -ve dirstack offsets */
        
    /* clear the stack, except for the cwd (top of stack) */
    if(clear && dirstack)
    {
        struct dirstack_ent_s *ds = dirstack->next;
        while(ds)
        {
            struct dirstack_ent_s *next = ds->next;
            free_malloced_str(ds->path);
            free(ds);
            ds = next;
        }
        dirstack->next = NULL;
        stack_count = 1;
    }

    /* no arguments. print the dirstack and return */
    if(v >= argc)
    {
        purge_dirstack(flags);
        return 0;
    }
  
    char *home = get_home(1);
    size_t chars = 0;

    /* parse the arguments */
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        if(*arg == '+' || *arg == '-' || isdigit(*arg))
        {
            struct dirstack_ent_s *ds = get_dirstack_entry(arg, NULL, &n);
            if(!ds)
            {
                PRINT_ERROR("dirs", "directory stack index out of range: %s", arg);
                return 2;
            }

            /* wrap if needed */
            size_t len = strlen(ds->path);
            if(wrap && chars+len >= VGA_WIDTH)
            {
                printf("\n");
                chars = 0;
            }

            if(print_index)
            {
                printf("%3d  ", n);
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
                putchar(' ');
            }
        }
        else
        {
            OPTION_UNKNOWN_STR_ERROR("dirs", arg);
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
 * The pushd builtin utility (non-POSIX extension). Used to push a directory on
 * the directory stack.
 * 
 * Returns 0 on success, non-zero otherwise.
 * 
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help pushd` or `pushd -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int pushd_builtin(int argc, char **argv)
{
    int flags = 0, silent = 0;
    int cd_to_dir = 1, v = 1, res = 0;
    
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
            
            /* negative stack offset */
            if(isdigit(*p))
            {
                break;
            }
            
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], &PUSHD_BUILTIN, 0);
                        return 0;
                        
                    case 'n':
                        cd_to_dir = 0;
                        break;
                
                    /*
                     * In tcsh, -p overrides 'pushdsilent'. to maintain uniformity,
                     * this flag will have the same effect it did in dirs, which 
                     * is to print entries on separate lines (borrowed from ksh/bash).
                     */
                    case 'p':
                        flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                        break;
                
                    /*
                     * this is similar to setting tcsh's 'pushdsilent' variable, 
                     * which silences pushd & popd.
                     */
                    case 's':
                        silent = 1;
                        break;
                
                    /*
                     * In tcsh, -n, -l and -v has the same effect on pushd as on dirs.
                     * We use -w instead of -n, as we need -n to suppress cd in order
                     * to be bash-compatible.
                     */
                    case 'l':
                        flags |= FLAG_DIRSTACK_FULL_PATHS;
                        break;
                
                    case 'v':
                        flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                        flags |= FLAG_DIRSTACK_PRINT_INDEX;
                        break;
                
                    case 'w':
                        flags |= FLAG_DIRSTACK_WRAP_ENTRIES;
                        break;
            
                    default:
                        OPTION_UNKNOWN_ERROR("pushd", *p);
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
         * In tcsh, when pushd is called with no arguments and the pushdtohome variable
         * is set, pushd pushes $HOME (similar to what happens with cd). If the variable
         * is not set, pushd behaves as bash's version, which exchanges the top two dirs
         * on the stack.
         */
        if(optionx_set(OPTION_PUSHD_TO_HOME))
        {
            char *home = get_shell_varp("HOME", NULL);
            if(!home)
            {
                PRINT_ERROR("pushd", "invalid directory name: %s", "$HOME");
                return 1;
            }
            
            char *cwd2 = word_expand_to_str(home, FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES);
            if(!cwd2)
            {
                PRINT_ERROR("pushd", "invalid directory name: %s", home);
                return 1;
            }
            
            /* now cd to the new directory */
            if(cd_to_dir)
            {
                res = do_cd(1, 2, (char *[]){ "pushd", cwd2 }, 0, 0, DO_CD_PUSH_DIRSTACK);
            }
            else
            {
                /*
                 * if pushing without cd'ing, push as the 2nd entry on the stack,
                 * as the top entry must always be our cwd.
                 */
                res = !push_dir(cwd2, (dirstack && dirstack->next) ? 
                                &dirstack->next : &dirstack, &stack_count, "pushd");
            }
            
            free(cwd2);
        }
        else
        {
            /* check for an empty stack (including stack with one entry -- the cwd) */
            if(stack_count < 2)
            {
                PRINT_ERROR("pushd", "cannot push on an empty stack");
                return 1;
            }
            
            /* exchange the top two directories */
            struct dirstack_ent_s *ds1 = dirstack;
            struct dirstack_ent_s *ds2 = ds1->next;
            
            dirstack = ds2;
            ds1->next = ds2->next;
            ds1->prev = ds2;
            ds2->next = ds1;
            ds2->prev = NULL;
        
            /* now cd to the new directory */
            if(cd_to_dir)
            {
                res = do_cd(1, 2, (char *[]){ "pushd", dirstack->path }, 0, 0, 0);
            }
        }
        
        /* only print the the stack if the push was successfuly */
        if(!silent && !res)
        {
            purge_dirstack(flags);
        }
        
        return res;
    }

    /* we accept at most one directory name */
    if(argc-v > 1)
    {
        PRINT_ERROR("pushd", "too many arguments");
        return 2;
    }
    
    /* now parse the argument */
    char *dir = NULL, *arg = argv[v];
    int free_dir = 0, push_stack = DO_CD_PUSH_DIRSTACK;

    /* tcsh's pushd recognizes "-" to mean $OLDPWD, just as cd does */
    if(*arg == '-' && arg[1] == '\0')
    {
        dir = get_shell_varp("OLDPWD", NULL);
        if(!dir)
        {
            PRINT_ERROR("pushd", "invalid directory name: %s", arg);
            return 1;
        }
    }
    /* numeric argument */
    else if(*arg == '+' || *arg == '-' || isdigit(*arg))
    {
        struct dirstack_ent_s *ds = get_dirstack_entry(arg, NULL, NULL);
        if(!ds)
        {
            PRINT_ERROR("pushd", "directory stack index out of range: %s", arg);
            return 1;
        }
        
        /* if the offset is +0, it's not an error, but don't modify the stack */
        if(ds == dirstack)
        {
            return 0;
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
        
        dir = dirstack->path;
        push_stack = 0;
    }
    /* non-numeric argument */
    else
    {
        //char *cwd2 = word_expand_to_str(arg);
        dir = word_expand_to_str(arg, FLAG_PATHNAME_EXPAND|FLAG_REMOVE_QUOTES);
        if(!dir)
        {
            PRINT_ERROR("pushd", "invalid directory name: %s", arg);
            return 1;
        }
        free_dir = 1;

        /*
         * in tcsh, if the dunique variable is set, pushd removes all instances 
         * of dir from the stack.
         */
        if(optionx_set(OPTION_DUNIQUE))
        {
            struct dirstack_ent_s *ds1 = dirstack;
            while(ds1)
            {
                if(strcmp(ds1->path, dir) == 0)
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
    }

    /* now cd to the new directory */
    if(cd_to_dir)
    {
        res = do_cd(1, 2, (char *[]){ "pushd", dir }, 0, 0, push_stack);
    }
    else if(push_stack)
    {
        /*
         * if pushing without cd'ing, push as the 2nd entry on the stack,
         * as the top entry must always be our cwd.
         */
        res = !push_dir(dir, (dirstack && dirstack->next) ? 
                             &dirstack->next : &dirstack, &stack_count, "pushd");
    }

    if(!silent && !res)
    {
        purge_dirstack(flags);
    }

    if(dir && free_dir)
    {
        free(dir);
    }

    return res;
}


/*
 * The popd builtin utility (non-POSIX extension). Used to pop a directory from
 * the directory stack.
 * 
 * Returns 0 on success, non-zero otherwise.
 * 
 * See the manpage for the list of options and an explanation of what each option does.
 * You can also run: `help popd` or `popd -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */
int popd_builtin(int argc, char **argv)
{
    int flags = 0, silent = 0;
    int cd_to_dir = 1;
    int v = 1, c;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hlnpsvw", &v, 0)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &POPD_BUILTIN, 0);
                return 0;
                
            case 'n':
                cd_to_dir = 0;
                break;
                
            /*
             * In tcsh, -n, -l and -v has the same effect on popd as on dirs.
             * We use -w instead of -n, as we need -n to suppress cd in order
             * to be bash-compatible.
             */
            case 'v':
                flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                flags |= FLAG_DIRSTACK_PRINT_INDEX;
                break;
                
            case 'l':
                flags |= FLAG_DIRSTACK_FULL_PATHS;
                break;
                
            case 'w':
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

    /* no arguments */
    if(v >= argc)
    {
        /* check for an empty stack (including stack with one entry -- the cwd) */
        if(stack_count < 2)
        {
            PRINT_ERROR("popd", "cannot pop from an empty stack");
            return 1;
        }
            
        /* pop the top of the stack */
        struct dirstack_ent_s *ds1 = dirstack;
        dirstack = dirstack->next;
        dirstack->prev = NULL;
        stack_count--;
        
        free_malloced_str(ds1->path);
        free(ds1);

        /* now cd to the new directory */
        if(cd_to_dir &&
           do_cd(1, 2, (char *[]){ "popd", dirstack->path }, 0, 0, 0) != 0)
        {
            return 1;
        }

        if(!silent)
        {
            purge_dirstack(flags);
        }
        
        return 0;
    }

    /* we accept at most one directory name */
    if(argc-v > 1)
    {
        PRINT_ERROR("popd", "too many arguments");
        return 2;
    }
    
    /* now parse the argument */
    char *arg = argv[v];
    if(*arg == '+' || *arg == '-' || isdigit(*arg))
    {
        struct dirstack_ent_s *prev = NULL;
        struct dirstack_ent_s *ds = get_dirstack_entry(arg, &prev, NULL);
        if(!ds)
        {
            PRINT_ERROR("popd", "directory stack index out of range: %s", arg);
            return 1;
        }
        
        /* check for an empty stack (including stack with one entry -- the cwd) */
        if(stack_count < 2)
        {
            PRINT_ERROR("popd", "cannot pop from an empty stack");
            return 1;
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
            dirstack = ds->next;
            free_malloced_str(ds->path);
            free(ds);
            stack_count--;
        }
    }
    else
    {
        PRINT_ERROR("popd", "invalid directory stack index: %s", arg);
        return 1;
    }

    /* now cd to the new directory */
    if(cd_to_dir && do_cd(1, 2, (char *[]){ "popd", dirstack->path }, 0, 0, 0) != 0)
    {
        return 1;
    }

    /* make sure we have a full path in the stack entry */
    if(dirstack->path[0] != '/' && dirstack->path[0] != '~' &&
       strcmp(dirstack->path, cwd))
    {
        free_malloced_str(dirstack->path);
        dirstack->path = get_malloced_str(cwd);
    }

    if(!silent)
    {
        purge_dirstack(flags);
    }

    return 0;
}
