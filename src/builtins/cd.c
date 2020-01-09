/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: cd.c
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

/* macro definition needed to use setenv() */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../parser/parser.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY             "cd"

char *cwd = NULL;

/*
 * this function executes the 'cwdcmd' special alias, if defined.. this alias contains
 * the command(s) we should execute whenever we're changing the cwd. the reason why we
 * don't use run_alias_cmd() of alias.c is discussed in the next paragraph.
 * 
 * tcsh's manpage says that executing the 'cwdcmd' special alias result in an
 * infinite loop if the alias contained 'cd', 'pushd' or 'popd' (which makes sense).
 * we try to detect this condition early by scanning the alias (if it is defined)
 * for these words.. of course, these words might appear in the alias AFTER it is
 * expanded, and so we check AFTER the word expansion is performed.
 */
void do_cwdcmd(void)
{
    static char *wordlist[] = { "cd", "popd", "pushd", "cwdcmd" };
    int wordcount = 4;
    /* get the alias value */
    char *cmd = get_alias_val("cwdcmd");
    char *p;
    if(cmd && *cmd)
    {
        /* perform word expansion on the alias value */
        cmd = word_expand_to_str(cmd);
        if(cmd)
        {
            /* prevent an infinite loop by checking for the 'prohibited' words */
            while(wordcount--)
            {
                if((p = strstr(cmd, wordlist[wordcount])))
                {
                    /* bailout if the word is followed by whitespace or NULL */
                    char c = p[strlen(wordlist[wordcount])];
                    if(!c || isspace(c))
                    {
                        return;
                    }
                }
            }
            /* run the parsed cwdcmd alias command(s) */
            eval_builtin(2, (char *[]){ "eval", cmd, NULL });
            free(cmd);
        }
    }
}


/*
 * execute 'cd' when its called with the hyphen argument (as `cd -`).. according to POSIX,
 * this command shall be equivalent to changing to the previous working directory, followed
 * by printing the current working directory:
 * 
 *          cd "$OLDPWD" && pwd.
 * 
 * this function returns 0 on success, non-zero if an error occurred.
 */
int cd_hyphen(void)
{
    /* get the value of the old working dir from the $OLDPWD variable */
    struct symtab_entry_s *entry = get_symtab_entry("OLDPWD");
    struct symtab_entry_s *entry2 = get_symtab_entry("PWD");
    if(!entry || !entry->val)
    {
        fprintf(stderr, "%s: failed to change directory: $OLDPWD is not set\n", UTILITY);
        return 3;
    }
    
    /* get an malloc'd copy of the $OLDPWD */
    char *pwd = __get_malloced_str(entry->val);
    if(!pwd)
    {
        fprintf(stderr, "%s: failed to change directory: %s\n", UTILITY, strerror(errno));
        return 3;
    }
    
    /* change directory to the $OLDPWD */
    if(chdir(pwd) != 0)
    {
        fprintf(stderr, "%s: failed to change directory: %s\n", UTILITY, strerror(errno));
        return 3;
    }
    
    /* set the new value of $PWD in both the environment and our shell variables list */
    setenv("PWD", pwd, 1);
    setenv("OLDPWD", entry2 ? entry2->val : "", 1);
    symtab_entry_setval(entry, entry2 ? entry2->val : NULL);
    symtab_entry_setval(entry2, pwd);
    
    /* print the new current working dir */
    printf("%s\n", pwd);
    
    /* and save it in the cwd global variable, after releasing its old value */
    if(cwd)
    {
        free(cwd);
    }
    cwd = pwd;
    
    /* in tcsh, special alias cwdcmd is run after cd changes the directory */
    do_cwdcmd();
    
    return 0;
}


/*
 * return the value of the home directory.. if the $HOME variable is set, use its value.
 * otherwise, get the home directory from the passwd database.
 * returns the home directory of the current user, or NULL in case of error.. if the
 * caller wants to do anything with the result, it should make its own copy and work
 * on it, as our return value is shared by other functions in the shell.
 */
char *get_home(void)
{
    char *home = get_shell_varp("HOME", NULL);
    /*
     * in this case ($HOME is unset or null), the behaviour is implementation-defined,
     * as per POSIX.. we try to read home directory from the passwd database.
     */
    if(!home || !*home)
    {
        struct passwd *pw = getpwuid(geteuid());
        if(!pw)
        {
            return NULL;
        }
        home = pw->pw_dir;
    }
    return home;
}


/*
 * search for directory in the $CDPATH.. if directory is an absolute path, or a relative
 * path starting with dot '.' or dot-dot '..', return directory.. otherwise, search $CDPATH
 * and return the absolute path of the directory.
 */
char *search_cdpath(char *directory, int *print_cwd)
{
    /* get the $CDPATH */
    char   *cdpath = get_shell_varp("CDPATH", NULL);

    /* no directory argument. return NULL */
    if(!directory)
    {
        return NULL;
    }
    
    /*
     * check if directory is an absolute path that begins with '/', or a
     * relative path that begins with '.' or '..'.
     */
    switch(directory[0])
    {
        case '/':       /* directory starts with '/' */
            return __get_malloced_str(directory);
                
        case '.':
            switch(directory[1])
            {
                case '/':       /* directory starts with './' */
                case '\0':      /* directory is '.' */
                    return __get_malloced_str(directory);
                        
                case '.':       /* directory starts with '..' */
                    if(directory[2] == '\0' || directory[2] == '/')
                    {
                        return __get_malloced_str(directory);
                    }
            }
    }

    /* no $CDPATH. return the directory as-is */
    if(!cdpath)
    {
        return __get_malloced_str(directory);
    }
            
    char   *cp     = cdpath;
    size_t  dirlen = strlen(directory);
    struct  stat st;

    /* search for the directory in $CDPATH */
    while(*cdpath)
    {
        /* skip the colon(s) in $CDPATH */
        while(*cp && *cp != ':')
        {
            cp++;
        }

        /*
         * get the next entry in $CDPATH.
         * we add three for \0 terminator, possible /, and leading dot
         * (in case the path is NULL and we need to append ./)
         */
        size_t pathlen = (cp-cdpath);
        char path[pathlen+dirlen+3];

        if(pathlen == 0)
        {
            strcpy(path, "./");
        }
        else
        {
            strncpy(path, cdpath, pathlen);
            path[pathlen] = '\0';
            if(path[pathlen-1] != '/')
            {
                strcat(path, "/");
            }
            (*print_cwd) = 1;
        }

        /* form the new path as the path segment from $CDPATH plus the directory name */
        strcat(path, directory);
        if(stat(path, &st) == 0)
        {
            /* check if the new path is a directory */
            if(S_ISDIR(st.st_mode))
            {
                return __get_malloced_str(path);
            }
        }
        
        (*print_cwd) = 0;
        /* move on to the next entry in $CDPATH */
        cdpath = ++cp;
    }
    return __get_malloced_str(directory);
}


/*
 * convert curpath to an absolute path by removing any dot '.' and dot-dot '..'
 * components.. then remove any trailing slashes, and convert any sequence of 3 or
 * more slashes to one slash.
 * 
 * returns 1 on success, 0 on failure.
 */
int absolute_pathname(char *curpath)
{
    struct  stat st;
    char *cp1 = curpath;
    char *cp2 = cp1;
    
    /* read curpath one char at a time */
    while(*cp1)
    {
        if(cp1[0] == '.' && cp1[1] == '.')
        {
            /*****************************
             * next component is dot-dot
             *****************************/
            switch(cp1[2])
            {
                case '\0':      /* '..' */
                case '/':       /* '../' */
                    /* is there a preceding component? */
                    if(cp1 != curpath)
                    {
                        char *pp = cp1-1;   /* this points to the slash before current component */
                        while(pp >= curpath && *pp == '/')      /* skip all slashes */
                        {
                            pp--;
                        }
                        
                        if(pp <= curpath)   /* prev is root */
                        {
                            cp2 = cp1;
                            break;
                        }
                        
                        /* prev is not root */
                        char *pp2 = pp;
                        while(pp >= curpath && *pp != '/')      /* skip to prev's head */
                        {
                            pp--;
                        }
                        
                        if(*pp == '/')
                        {
                            pp++;
                        }
                        
                        if(strncmp(pp, "..", 2) == 0 && (pp[2] == '\0' || pp[2] == '/'))
                        {
                            /* prev is dot-dot ('..' or '../') */
                            cp2 = cp1;
                            break;
                        }
                        
                        /* POSIX Step 8.b.i: check if preceding component is a dir */
                        size_t l = pp2-curpath+1;
                        char   prev[l+1];
                        
                        strncpy(prev, curpath, l);
                        prev[l] = '\0';
                        if(stat(prev, &st) != 0 || !S_ISDIR(st.st_mode))
                        {
                            fprintf(stderr, "%s: not a directory: %s\n", UTILITY, prev);
                            return 0;
                        }
                        
                        /* now remove prev and cur components */
                        pp2 = cp1+2;    /* skip current dot-dot */
                        while(*pp2 && *pp2 == '/')      /* skip all slashes after dot-dot */
                        {
                            pp2++;
                        }
                        
                        cp2 = pp;
                        while((*pp++ = *pp2++))
                        {
                            ;
                        }
                        
                        cp1 = cp2;
                        continue;
                    }
                    break;
                    
                default:
                    cp2 = cp1;
                    break;
            }
        }
        else if(cp1[0] == '.')
        {
            /*****************************
             * next component is dot
             *****************************/
            cp2 = cp1;
            if(cp1[1] == '\0' || cp1[1] == '/')
            {
                /* remove the dot */
                delete_char_at(cp1, 0);
                
                /* and any slashes after it */
                while(*cp1 == '/')
                {
                    delete_char_at(cp1, 0);
                }
                
                cp1 = cp2;
                continue;
            }
        }
    
        /* skip component */
        while(*cp2 && *cp2 != '/')
        {
            cp2++;
        }
        
        /* skip slashes */
        while(*cp2 && *cp2 == '/')
        {
            cp2++;
        }
        cp1 = cp2;
    }
    
    /* (1) replace leading 3+ slashes with a single slash */
    if(strncmp(curpath, "///", 3) == 0)
    {
        cp1 = curpath+1;
        cp2 = cp1;
        while(*cp2++ == '/')
        {
            ;
        }
        
        while((*cp1++ = *cp2++))
        {
            ;
        }
    }
    
    /* (2) look for trailing slashes ... */
    cp1 = curpath+strlen(curpath);
    cp2 = cp1-1;
    while(cp1 >= curpath && *--cp1 == '/')
    {
        ;
    }
    
    /* ... and remove them */
    if(cp1 >= curpath && cp1 != cp2-1)
    {
        *++cp1 = '\0';
    }
    
    /* (3) replace non-leading multiple slashes with a single slash */
    cp1 = curpath;
    while(*++cp1)
    {
        if(*cp1 == '/')
        {
            cp2 = cp1+1;
            while(*cp2 == '/')
            {
                delete_char_at(cp2, 0);
            }
        }
    }
    return 1;
}


/*
 * if curpath is longer than the maximum length allowed on the system,
 * convert it to a relative path by removing the leading component(s).
 * 
 * returns 1 on success, 0 on failure.
 */
int shorten_path(char *curpath, char *pwd, size_t pwdlen)
{
    /* get the system defined maxium path length */
    long path_max = sysconf(_PC_PATH_MAX);
    if(path_max <= 0)
    {
        /* use our default length if the system has none defined */
        path_max = DEFAULT_PATH_MAX;
    }
    
    /* curpath is longer than the maxium length */
    if(strlen(curpath) >= (size_t)path_max)
    {
        int can_do = 0;
        if(!pwd || !*pwd)
        {
            fprintf(stderr, "%s: $PWD environment variable is not set\n", UTILITY);
            return 0;
        }
        
        /* determine how much we need to cut from the beginning of curpath */
        if(pwd[pwdlen-1] == '/')
        {
            can_do = strncmp(curpath, pwd, pwdlen) == 0 ? 1 : 0;
        }
        else
        {
            char pwd2[pwdlen+2];
            strcpy(pwd2, pwd);
            strcat(pwd2, "/");
            can_do = strncmp(curpath, pwd2, pwdlen) == 0 ? 1 : 0;
            pwdlen++;
        }
        
        /* make curpath a relative path */
        if(can_do)
        {
            char *cp1 = curpath;
            char *cp2 = cp1+pwdlen;
            while((*cp1++ = *cp2++))
            {
                ;
            }
        }
    }
    return 1;
}


/*
 * the cd builtin utility (POSIX).
 * 
 * this function follows the POSIX algorithm almost to the letter.
 * you can check this by visiting this link:
 *       http://pubs.opengroup.org/onlinepubs/9699919799/utilities/cd.html
 */
int cd_builtin(int argc, char **argv)
{
    int     p_option = 0;
    int     v;
    char   *curpath   = NULL;
    char   *directory = NULL;
    char   *pwd       = getenv("PWD");
    size_t  pwdlen    = 0;
    int     print_cwd = 0;
    struct  symtab_entry_s *entry;

    if(pwd && *pwd)
    {
        pwdlen = strlen(pwd);
    }

    /*
     * in tcsh, cd accepts options similar to what dirs accept, namely: p, l, n, v.
     */
    int print_dirstack = 0;
    int flags = 0;
   
    /* is this shell restricted? */
    if(startup_finished && option_set('r'))
    {
        fprintf(stderr, "%s: can't change directory in a restricted shell", UTILITY);
        return 3;
    }
    
    /* loop on the arguments and parse the options, if any */
    for(v = 1; v < argc; v++)
    {
        /* options start with '-' */
        if(argv[v][0] == '-')
        {
            char *p = argv[v];

            /* stop parsing options when we hit '-' or '--' */
            if(strcmp(p, "-") == 0)
            {
                break;
            }

            if(strcmp(p, "--") == 0)
            {
                v++;
                break;
            }
            
            /* skip the '-' and parse the options string */
            p++;
            while(*p)
            {
                switch(*p)
                {
                    case 'h':
                        print_help(argv[0], REGULAR_BUILTIN_CD, 1, 0);
                        return 0;
                        
                    case 'L':
                        p_option = 0;
                        break;
                        
                    case 'P':
                        p_option = 1;
                        break;

                    /* tcsh extensions: -v, -p, -l, -n */
                    case 'v':
                        print_dirstack = 1;
                        flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                        flags |= FLAG_DIRSTACK_PRINT_INDEX;
                        break;
                
                    case 'p':
                        print_dirstack = 1;
                        flags |= FLAG_DIRSTACK_SEPARATE_LINES;
                        break;
                
                    case 'l':
                        print_dirstack = 1;
                        flags |= FLAG_DIRSTACK_FULL_PATHS;
                        break;
                        
                    case 'n':
                        print_dirstack = 1;
                        flags |= FLAG_DIRSTACK_WRAP_ENTRIES;
                        break;
                        
                    default:
                        fprintf(stderr, "%s: unknown option: %s\n", UTILITY, argv[v]);
                        return 2;
                }
                p++;
            }
        }
        else
        {
            /* first argument, stop paring options */
            break;
        }
    }
    
    if(v >= argc)
    {
        /* no dir argument. use $HOME */
        directory = get_home();
        if(directory)
        {
            curpath = __get_malloced_str(directory);
        }
    }
    else if(argv[v][0] == '-' && argv[v][1] == '\0')
    {
        /* the hyphen '-' argument */
        return cd_hyphen();
    }
    else if(argv[v][0] == '/' ||
            (argv[v][0] == '.' &&
             (argv[v][1] == '\0' || argv[v][1] == '/' || argv[v][1] == '.')))
    {
        /* first component is '/', '.' or '..' */
        directory = argv[v];
        curpath = __get_malloced_str(directory);
    }
    else
    {
        /* use the dir argument */
        directory = argv[v];

        /* search the $CDPATH, if needed */
        curpath = search_cdpath(directory, &print_cwd);
    }
    
start:
    if(!p_option)
    {
        /* if curpath is relative, attach it to the current working directory */
        if(curpath && *curpath != '/' && pwd && *pwd)
        {
            /* add 2 to the length for possible '/' */
            char *path = malloc(pwdlen+strlen(curpath)+2);
            if(path)
            {
                /* check for trailing '/' and add one if needed */
                if(path[pwdlen-1] != '/')
                {
                    sprintf(path, "%s/%s", pwd, curpath);
                }
                else
                {
                    sprintf(path, "%s%s", pwd, curpath);
                }
                free(curpath);
                curpath = path;
            }
        }

        /* now canonicalize (remove dot and dot-dot components and convert to an absolute path) */
        if(!absolute_pathname(curpath))
        {
            return 1;
        }
    
        /* is the path null now? */
        if(!*curpath)
        {
            free(curpath);
            return 0;
        }
    
        /* check the path's length (POSIX step 9) */
        if(!shorten_path(curpath, pwd, pwdlen))
        {
            return 2;
        }
    }
    
    /* now change to the new directory */
    if(curpath && chdir(curpath) != 0)
    {
        free(curpath);
        if(optionx_set(OPTION_CDABLE_VARS) && v < argc)
        {
            /* assume the argument is a variable name whose value is our dest dir */
            entry = get_symtab_entry(argv[v]);
            if(entry && entry->val)
            {
                directory = entry->val;
                if(strcmp(directory, "-") == 0)
                {
                    return cd_hyphen();
                }
                curpath = search_cdpath(directory, &print_cwd);
                /* try again with the value of the variable */
                goto start;
            }
        }
        fprintf(stderr, "%s: failed to change directory: %s\n", UTILITY, strerror(errno));
        return 3;
    }

    /* save the current working directory in $OLDPWD */
    setenv("OLDPWD", pwd, 1);
    entry = add_to_symtab("OLDPWD");
    if(entry)
    {
        symtab_entry_setval(entry, pwd);
    }

    /* POSIX says we must set PWD to the string that would be output by `pwd -P` */
    if(cwd)
    {
        free(cwd);
    }
    cwd = getcwd(NULL, 0);

    /* and also save it in the environment */
    setenv("PWD", cwd, 1);

    /* save the new current working directory in $PWD */
    entry = add_to_symtab("PWD");
    if(entry)
    {
        symtab_entry_setval(entry, cwd);
    }

    if(print_dirstack)
    {
        purge_dirstack(flags);
    }
    else if(print_cwd)
    {
        printf("%s\n", cwd);
    }
    free(curpath);

    /* in tcsh, special alias cwdcmd is run after cd changes the directory */
    do_cwdcmd();

    return 0;
}
