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

/* macro definitions needed to use setenv() */
#define _POSIX_C_SOURCE 200112L

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
 * tcsh's manpage says that performing the cwdcmd special alias result in an
 * infinite loop if the alias contained cd, pushd or popd (makes sense). we
 * try to detect this condition early by scanning the alias (if it is defined)
 * for these words. Of course, these words might appear in the alias AFTER it is
 * expanded, and so we check AFTER the word expansion.
 */
void do_cwdcmd()
{
    static char *wordlist[] = { "cd", "popd", "pushd", "cwdcmd" };
    int wordcount = 4;
    char *cmd = __parse_alias("cwdcmd");
    char *p;
    if(cmd && *cmd)
    {
        cmd = word_expand_to_str(cmd);
        if(cmd)
        {
            while(wordcount--)
            {
                if((p = strstr(cmd, wordlist[wordcount])))
                {
                    /* bailout if the word is followed by whitespace or NULL */
                    char c = p[strlen(wordlist[wordcount])];
                    if(!c || isspace(c)) return;
                }
            }
            eval(2, (char *[]){ "eval", cmd, NULL });
            free(cmd);
        }
    }
}


/* 'cd -' : According to POSIX, this shall be equivalent to:
 *          cd "$OLDPWD" && pwd.
 */
int cd_hyphen()
{
    struct symtab_entry_s *entry = get_symtab_entry("OLDPWD");
    if(!entry || !entry->val)
    {
        fprintf(stderr, "%s: failed to change directory: $OLDPWD is not set\r\n", UTILITY);
        return 3;
    }
    char   *pwd = __get_malloced_str(entry->val);
    if(!pwd) return 3;
    if(chdir(pwd) != 0)
    {
        fprintf(stderr, "%s: failed to change directory: %s\r\n", UTILITY, strerror(errno));
        return 3;
    }
    setenv("PWD", pwd, 1);
    struct symtab_entry_s *entry2 = get_symtab_entry("PWD");
    setenv("OLDPWD", __get_malloced_str(entry2->val), 1);
    symtab_entry_setval(entry, entry2->val);
    symtab_entry_setval(entry2, pwd);
    printf("%s\r\n", pwd);
    if(cwd) free(cwd);
    cwd = pwd;
    /* in tcsh, special alias cwdcmd is run after cd changes the directory */
    do_cwdcmd();
    return 0;
}


char *get_home()
{
    char *home = get_shell_varp("HOME", NULL);
    /* In this case, behaviour is implementation-defined, as per POSIX.
     * we try to read home directory from the user database.
     */
    if(!home || home[0] == '\0')
    {
        struct passwd *pw = getpwuid(getpid());
        if(!pw) return 0;
        home = pw->pw_dir;
    }
    return home;
}


/*
 * NOTE: this function follows the POSIX algorithm almost to the letter,
 *       which you can view from this link:
 *              http://pubs.opengroup.org/onlinepubs/9699919799/utilities/cd.html
 * 
 *       but this function is a horrible, horrible example of spaghetti code!
 * 
 * TODO: fix this monstrosity!
 */
int cd(int argc, char *argv[])
{
    int     /* L_option = 1, */ P_option = 0;
    int     v;
    char   *curpath   = NULL;
    char   *finalpath = NULL;
    char   *directory = NULL;
    struct  stat st;
    char   *pwd       = get_shell_varp("PWD", NULL);
    size_t  pwdlen    = 0;
    int     print_cwd = 0;
    if(pwd && *pwd) pwdlen = strlen(pwd);
    struct symtab_entry_s *entry;
    /*
     * in tcsh, cd accepts options similar to what dirs accept, namely: p, l, n, v.
     */
    int print_dirstack = 0;
    int flags = 0;
   
    /* is this shell restricted? */
    if(option_set('r'))
    {
        fprintf(stderr, "%s: can't change directory in a restricted shell", UTILITY);
        return 3;
    }
  
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
                        print_help(argv[0], REGULAR_BUILTIN_CD, 1, 0);
                        return 0;
                    
                    /*
                    case 'v':
                        printf("%s", shell_ver);
                        return 0;
                    */
                        
                    case 'L':
                        /* L_option = 1; */ P_option = 0  ;
                        break;
                        
                    case 'P':
                        P_option = 1; /* L_option = 0  ; */
                        break;

                    /* tcsh extension */
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
                        fprintf(stderr, "%s: unknown option: %s\r\n", UTILITY, argv[v]);
                        return 2;
                }
                p++;
            }
        }
        else break;
    }
    
    if(v >= argc)
    {
        directory = get_home();
    }
    else
    {
        directory = argv[v];
start:
        if(strcmp(directory, "-") == 0) return cd_hyphen();
        if(directory[0] == '/')
        {
            curpath = __get_malloced_str(directory);
            goto step7;
        }
        else if(directory[0] == '.')
        {
            /* first component is dot */
            if(directory[1] == '\0' || directory[1] == '/' ) goto step6;
            /* first component is dot-dot */
            if(directory[1] == '.' &&
              (directory[2] == '\0' || directory[2] == '/')) goto step6;
        }
        char *cdpath = get_shell_varp("CDPATH", NULL);
        if(cdpath)
        {
            char   *cp        = cdpath;
            size_t  dirlen    = strlen(directory);
read_cdpath:
            if(!*cdpath) goto step6;
            while(*cp && *cp != ':') cp++;
            size_t pathlen = (cp-cdpath);
            /* we add three for \0 terminator, possible /, and leading dot
             * (in case the path is NULL and we need to append ./)
             */
            char path[pathlen+dirlen+3];
            if(pathlen == 0) strcpy(path, "./");
            else
            {
                strncpy(path, cdpath, pathlen);
                path[pathlen] = '\0';
                if(path[pathlen-1] != '/') strcat(path, "/");
                print_cwd = 1;
            }
            strcat(path, directory);
            if(stat(path, &st) == 0)
            {
                if(S_ISDIR(st.st_mode))
                {
                    curpath = __get_malloced_str(path);
                    goto step7;
                }
            }
            print_cwd = 0;
            cdpath = ++cp;
            goto read_cdpath;
        }
    }
    
step6:
    curpath = __get_malloced_str(directory);
    
step7:
    if(P_option) goto step10;
    if(*curpath != '/')
    {
        if(pwd && *pwd)
        {
            /* add 2 to the length for possible '/' */
            char *path = (char *)malloc(pwdlen+strlen(curpath)+2);
            if(path)
            {
                strcpy(path, pwd);
                if(path[pwdlen-1] != '/') strcat(path, "/");
                strcat(path, curpath);
                free(curpath);
                curpath = path;
            }
        }
    }

    /* now canonicalize */
    char *cp1 = curpath;
    char *cp2 = cp1;
    
loop:
    if(!*cp1) goto finish;
    
    if(strcmp(cp1, "..") == 0)
    {
        /*****************************
         * next component is dot-dot
         *****************************/
        if(cp1[2] == '\0' || cp1[2] == '/')
        {
            /* is there a preceding component? */
            if(cp1 != curpath)
            {
                char *pp = cp1-1;   /* this points to the slash before current component */
                while(pp >= curpath && *pp == '/') pp--;  /* skip all slashes */
                if(pp <= curpath)   /* prev is root */
                {
                    cp2 = cp1;
                    goto cont;
                }
                /* prev is not root */
                char *pp2 = pp;
                while(pp >= curpath && *pp != '/') pp--;  /* skip to prev's head */
                if(*pp == '/') pp++;
                if(strncmp(pp, "..", 2) == 0 && (pp[2] == '\0' || pp[2] == '/'))
                {
                    /* prev is dot-dot */
                    cp2 = cp1;
                    goto cont;
                }
                /* POSIX Step 8.b.i: check if preceding component is a dir */
                size_t l = pp2-curpath+1;
                char   prev[l+1];
                strncpy(prev, curpath, l);
                prev[l] = '\0';
                if(stat(prev, &st) != 0 || !S_ISDIR(st.st_mode))
                {
                    fprintf(stderr, "%s: not a directory: %s\r\n", UTILITY, prev);
                    return 1;
                }
                /* now remove prev and cur components */
                pp2 = cp1+2;    /* skip current dot-dot */
                while(*pp2 && *pp2 == '/') pp2++;   /* skip all slashes after dot-dot */
                cp2 = pp;
                while((*pp++ = *pp2++)) ;
                goto cont2;
            }
        }
        else
        {
            cp2 = cp1;
            goto cont;
        }
    }
    else if(strcmp(cp1, ".") == 0)
    {
        cp2 = cp1;
        /*****************************
         * next component is dot
         *****************************/
        if(cp1[1] == '\0' || cp1[1] == '/')
        {
            delete_char_at(cp1, 0);
            while(*cp1 == '/') delete_char_at(cp1, 0);
            goto cont2;
        }
        else
            goto cont;
    }
    
cont:
    /* skip component */
    while(*cp2 && *cp2 != '/') cp2++;
    /* skip slashes */
    while(*cp2 && *cp2 == '/') cp2++;
    
cont2:
    cp1 = cp2;
    goto loop;
    
finish:
    /* (1) replace leading 3+ slashes with a single slash */
    if(strncmp(curpath, "///", 3) == 0)
    {
        cp1 = curpath+1;
        cp2 = cp1;
        while(*cp2++ == '/') ;
        while((*cp1++ = *cp2++)) ;
    }
    /* (2) look for trailing slashes ... */
    cp1 = curpath+strlen(curpath);
    cp2 = cp1-1;
    while(cp1 >= curpath && *--cp1 == '/') ;
    /* ... and remove them */
    if(cp1 >= curpath && cp1 != cp2-1) *++cp1 = '\0';
    /* (3) replace non-leading multiple slashes with a single slash */
    cp1 = curpath;
    while(*++cp1)
    {
        if(*cp1 == '/')
        {
            cp2 = cp1+1;
            while(*cp2 == '/') delete_char_at(cp2, 0);
        }
    }
    
    /* is the path null now? */
    if(!*curpath) return 0;
    
    /* check path length next (POSIX step 9) */
    int path_max = sysconf(_PC_PATH_MAX);
    if(path_max <= 0) path_max = DEFAULT_PATH_MAX;
    if(strlen(curpath) >= path_max)
    {
        int can_do = 0;
        if(!pwd || !*pwd)
        {
            fprintf(stderr, "%s: $PWD environment variable is not set\r\n", UTILITY);
            return 2;
        }
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
            finalpath = __get_malloced_str(curpath);
            cp1 = curpath;
            cp2 = cp1+pwdlen;
            while((*cp1++ = *cp2++)) ;
        }
    }
    
step10:
    if(chdir(curpath) != 0)
    {
        if(optionx_set(OPTION_CDABLE_VARS) && v < argc)
        {
            /* assume the argument is a variable name whose value is our dest dir */
            entry = get_symtab_entry(argv[v]);
            if(entry && entry->val)
            {
                directory = entry->val;
                goto start;
            }
        }
        fprintf(stderr, "%s: failed to change directory: %s\r\n", UTILITY, strerror(errno));
        if(finalpath) free(finalpath);
        return 3;
    }

    setenv("OLDPWD", pwd, 1);
    entry = add_to_symtab("OLDPWD");
    if(entry) symtab_entry_setval(entry, pwd);
    entry = add_to_symtab("PWD"   );
    /* TODO: POSIX says we must set PWD to the string that would be output
     *       by pwd -P.
     */
    if(cwd) free(cwd);
    cwd = getcwd(NULL, 0);
    setenv("PWD", cwd, 1);
    if(entry) symtab_entry_setval(entry, cwd);
    if(print_dirstack)
    {
        purge_dirstack(flags);
    }
    else if(print_cwd) printf("%s\r\n", cwd);
    //free(cwd);
    if(finalpath) free(finalpath);
    free(curpath);
    /* in tcsh, special alias cwdcmd is run after cd changes the directory */
    do_cwdcmd();
    return 0;
}
