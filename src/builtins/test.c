/*
 * Copyright (c) 2012 the authors listed at the following URL, and/or
 * the authors of referenced articles or incorporated external code:
 * http://en.literateprograms.org/Shunting_yard_algorithm_(C)?action=history&offset=20080201043325
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *  
 * Retrieved from: http://en.literateprograms.org/Shunting_yard_algorithm_(C)?oldid=12454
 * 
 * 
 * Copyright (c) 2019 Mohammed Isam [mohammed_isam1984@yahoo.com]
 * 
 * Extensive modifications have been applied to this file to include most of the C
 * language operators and to make this file usable as part of the Layla shell.
 * Please compare with the original file at the above link to see the differences.
 * 
 * UPDATE: the original file doesn't seem to be available anymore, but the archived
 * version can be accessed from here:
 * https://web.archive.org/web/20110718214204/http://en.literateprograms.org/Shunting_yard_algorithm_(C)
 * 
 * For more information, see: https://en.wikipedia.org/wiki/Shunting-yard_algorithm
 * 
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <fnmatch.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "builtins.h"
#include "../cmd.h"
#include "../backend/backend.h"     /* match_pattern() */
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY             "test"

#define MAXOPSTACK          64      /* max operators allowed on the stack */
#define MAXTESTSTACK        64      /* max operands allowed on the stack */
#define ZERO                "0"
#define ONE                 "1"

/* the operators stack */
struct test_op_s **test_opstack;
// struct test_op_s *test_opstack[MAXOPSTACK];

/* the count of operators pushed on the operators stack */
int    test_nopstack  = 0;

/* the operands stack */
char  **teststack;
// char  *teststack[MAXTESTSTACK];

/* the count of operands pushed on the operands stack */
int    nteststack     = 0;

/* flag to set when there is an error pushing, popping or performing some other operation */
int    test_err       = 0;

// static int flags = FNM_NOESCAPE | FNM_PATHNAME | FNM_PERIOD;

/* buffers we will use to stat files when comparing their status */
struct stat statbuf ;
struct stat statbuf2;

#define FILES_EQUAL     1   /* the -eq file comparison operator */
#define FILE_NEWER_THAN 2   /* the -nt file comparison operator */
#define FILE_OLDER_THAN 3   /* the -ot file comparison operator */

/*
 * NOTE: in all of the upcoming comparisons in this file, remember that
 *       we always need to invert the result of any given test, so that 
 *       zero will indicate success and non-zero will indicate failure.
 */


#if 0
/*
 * recursively call test_builtin() to perform the test on a a sub-expression.
 */
int test_recursive(char *str)
{
    /* save our pointers */
    struct test_op_s **opstack2 = test_opstack;
    char **numstack2 = teststack;
    int nopstack2 = test_nopstack, nnumstack2 = nteststack, error2 = test_err;

    /* perform arithmetic expansion on the sub-expression */
    int res = test_builtin(2, (char *[]){ "test", str, NULL });

    /* restore our pointers */
    test_opstack = opstack2;
    teststack = numstack2;
    test_nopstack = nopstack2;
    nteststack = nnumstack2;
    test_err = error2;
    
    return res;
}
#endif


/*
 * compare two files, f1 and f2, using the operator op, which can be the
 * newer-than operator (FILE_NEWER_THAN), the older-than operator
 * (FILE_OLDER_THAN), or the equality operator (FILE_EQUAL).
 *
 * returns "0" if the result of comparison is true, "1" if it is false.
 */
char *compare_files(char *f1, char *f2, int op)
{
    /* stat the two files (lstat returns 0 on success, -1 on error) */
    memset(&statbuf , 0, sizeof(struct stat));
    memset(&statbuf2, 0, sizeof(struct stat));
    int res1 = lstat(f1, &statbuf );
    int res2 = lstat(f2, &statbuf2);

    /* perform the comparison operator */
    switch(op)
    {
        /* check if f1 and f2 refer to the same file */
        case FILES_EQUAL:
            /* one file does not exist, so f1 != f2 */
            if(res1 || res2)
            {
                return ONE;
            }

            /* compare f1 and f2's inode, device and rdev numbers */
            if(statbuf.st_ino == statbuf2.st_ino && 
               statbuf.st_dev == statbuf2.st_dev &&
               statbuf.st_rdev == statbuf2.st_rdev)
            {
                /* f1 and f2 match */
                return ZERO;
            }
            
            /* f1 and f2 do not match */
            return ONE;
            
        /* check if f1 is newer than f2 */
        case FILE_NEWER_THAN:
            /* f1 does not exist, so the result is false */
            if(res1)
            {
                return ONE;
            }
            
            /* f2 does not exist, so the result is true */
            if(res2)
            {
                return ZERO;
            }
            
            /* both exist. compare their modification times */
            return (statbuf.st_mtime <= statbuf2.st_mtime) ? ONE : ZERO;

        /* check if f1 is older than f2 */
        case FILE_OLDER_THAN:
            /* f2 does not exist, so the result is false */
            if(res2)
            {
                return ONE;
            }
            
            /* f1 does not exist, so the result is true */
            if(res1)
            {
                return ZERO;
            }
            
            /* both exist. compare their modification times */
            return (statbuf.st_mtime >= statbuf2.st_mtime) ? ONE : ZERO;
    }
    
    /* unknown operator. return false */
    return ONE;
}


#define ARITHM_EQ       1   /* -eq */
#define ARITHM_GE       2   /* -ge */
#define ARITHM_GT       3   /* -gt */
#define ARITHM_LE       4   /* -le */
#define ARITHM_LT       5   /* -lt */
#define ARITHM_NE       6   /* -ne */


/*
 * compare the two arithmetic expressions given as __e1 and __e2, using the
 * comparsion operator op, which can be one of the six operator we defined as
 * macros above (ARITHM_EQ et al).
 *
 * returns "0" if the result of comparison is true, "1" if it is false.
 */
char *compare_exprs(char *__e1, char *__e2, int op)
{
    /*
     * NOTE: bash treats e1 and e2 as arithmetic expressions, which are evaluated
     *       as any other arithmetic expr enclosed in $(( )) or (( )).
     */
    static char *zero = "0";
    char *strend = NULL;

    /* perform arithmetic expansion on e1 */
    char *e1 = (*__e1) ? arithm_expand(__e1) : zero;
    if(!e1)
    {
        e1 = zero;
    }

    /* get the numeric value of e1 */
    long res1 = strtol(e1, &strend, 10);
    if(*strend)
    {
        PRINT_ERROR("%s: invalid arithmetic expression: %s\n", UTILITY, __e1);
        if(e1 && e1 != zero)
        {
            free(e1);
        }
        return ONE;
    }

    /* perform arithmetic expansion on e2 */
    char *e2 = (*__e2) ? arithm_expand(__e2) : zero;
    if(!e2)
    {
        e2 = zero;
    }
    
    /* get the numeric value of e2 */
    long res2 = strtol(e2, &strend, 10);
    if(*strend)
    {
        PRINT_ERROR("%s: invalid arithmetic expression: %s\n", UTILITY, __e2);
        if(e2 && e2 != zero)
        {
            free(e2);
        }
        return ONE;
    }

    /* perform the comparison */
    int res = 2;    /* 2 means invalid operator (general bash error code for internal errors) */
    switch(op)
    {
        case ARITHM_EQ  : res = !(res1 == res2); break;
        case ARITHM_GE  : res = !(res1 >= res2); break;
        case ARITHM_GT  : res = !(res1  > res2); break;
        case ARITHM_LE  : res = !(res1 <= res2); break;
        case ARITHM_LT  : res = !(res1  < res2); break;
        case ARITHM_NE  : res = !(res1 != res2); break;
    }

    if(e1 != zero)
    {
        free(e1);
    }
    
    if(e2 != zero)
    {
        free(e2);
    }
    
    return res ? ONE : ZERO;
}

/*
 * test if a1 is greater-than a2 and return "0" if true, "1" if false.
 */
char *test_gt(char *a1, char *a2)
{
    return compare_exprs(a1, a2, ARITHM_GT);
}

/*
 * test if a1 is less-than a2 and return "0" if true, "1" if false.
 */
char *test_lt(char *a1, char *a2)
{
    return compare_exprs(a1, a2, ARITHM_LT);
}

/*
 * test if a1 is greater-than-or-equal-to a2 and return "0" if true, "1" if false.
 */
char *test_ge(char *a1, char *a2)
{
    return compare_exprs(a1, a2, ARITHM_GE);
}

/*
 * test if a1 is less-than-or-equal-to a2 and return "0" if true, "1" if false.
 */
char *test_le(char *a1, char *a2)
{
    return compare_exprs(a1, a2, ARITHM_LE);
}

/*
 * test if a1 is equal-to a2 and return "0" if true, "1" if false.
 */
char *test_eq(char *a1, char *a2)
{
    return compare_exprs(a1, a2, ARITHM_EQ);
}

/*
 * test if a1 is not-equal-to a2 and return "0" if true, "1" if false.
 */
char *test_ne(char *a1, char *a2)
{
    return compare_exprs(a1, a2, ARITHM_NE);
}


/*
 * String comparison operators.
 */

char *str_remove_quotes(char *str, int *was_quoted)
{
    struct word_s *w = make_word(str);
    if(!w)
    {
        return str;
    }
    remove_quotes(w);
    (*was_quoted) = flag_set(w->flags, FLAG_WORD_HAD_QUOTES);
    
    char *str2 = w->data;
    free(w);
    
    return str2;
}

#define STR_EQ      1
#define STR_NEQ     2
#define STR_EQX     3

/*
 * test if string a1 is equal to a2 and return "0" if true, "1" if false.
 */
char *do_test_str(char *a1, char *a2, int op)
{
    int q1 = 0, q2 = 0;
    char *a3 = str_remove_quotes(a1, &q1);
    char *a4 = str_remove_quotes(a2, &q2);
    debug ("a3 = '%s', a4 = '%s'\n", a3, a4);
    //debug ("a3 = '%s', a2 = '%s'\n", a3, a2);
    int i = 0;
    char *res = NULL;
    
    /*
    if(op == STR_EQX)
    {
        i = match_pattern_ext(a2, a3);
    }
    else
    {
        i = match_pattern(a2, a3);
    }
    */
    
    if(q2)
    {
        i = !strcmp(a4, a3);
    }
    else if(op == STR_EQX)
    {
        i = match_pattern_ext(a4, a3);
    }
    else
    {
        i = match_pattern(a4, a3);
    }
    
    if(a3 != a1)
    {
        free(a3);
    }
    
    if(a4 != a2)
    {
        free(a4);
    }
    
    switch(op)
    {
        case STR_EQ :
        case STR_EQX:
            res = i ? ZERO : ONE;
            break;
            
        case STR_NEQ:
            res = i ? ONE : ZERO;
            break;
    }
    
    debug ("res = %s\n", res);
    return res;
}

/*
 * test if string a1 is equal to a2 and return "0" if true, "1" if false.
 */
char *test_str_eq(char *a1, char *a2)
{
    return do_test_str(a1, a2, STR_EQ);
}

/*
 * test if string a1 is equal to a2 using POSIX extended regex syntax.
 * returns "0" if we have a match, "1" if we don't have a match.
 */
char *test_str_eq_ext(char *a1, char *a2)
{
    return do_test_str(a1, a2, STR_EQX);
}

/*
 * test if string a1 is not-equal-to a2 and return "0" if true, "1" if false.
 */
char *test_str_ne(char *a1, char *a2)
{
    return do_test_str(a1, a2, STR_NEQ);
}

/*
 * test if string a1 is less-than a2 and return "0" if true, "1" if false.
 */
char *test_str_lt(char *a1, char *a2)
{
    return (strcmp(a1, a2) < 0) ? ZERO : ONE;
}

/*
 * test if string a1 is greater-than a2 and return "0" if true, "1" if false.
 */
char *test_str_gt(char *a1, char *a2)
{
    return (strcmp(a1, a2) > 0) ? ZERO : ONE;
}

/*
 * File comparison operators.
 */

/*
 * test if file a1 is the same as a2 and return "0" if true, "1" if false.
 */
char *test_file_ef(char *a1, char *a2)
{
    return compare_files(a1, a2, FILES_EQUAL);
}

/*
 * test if file a1 is newer-than a2 and return "0" if true, "1" if false.
 */
char *test_file_nt(char *a1, char *a2)
{
    return compare_files(a1, a2, FILE_NEWER_THAN);
}

/*
 * test if file a1 is older-than a2 and return "0" if true, "1" if false.
 */
char *test_file_ot(char *a1, char *a2)
{
    return compare_files(a1, a2, FILE_OLDER_THAN);
}

/*
 * perform the logical NOT operation on a1 and return the result.
 * remember that [[ ! x ]] is equivalent to [[ ! -n x ]].
 */
char *test_not(char *a1, char *a2 __attribute__ ((unused)) )
{
    /* zero-length string, return 0 */
    if(!*a1)
    {
        return ZERO;
    }
    
    /* non-numeric, non-zero-length string, return 1 */
    if(!is_num(a1))
    {
        return ONE;
    }
    
    return strcmp(a1, ZERO) == 0 ? ONE : ZERO;
}

/*
 * perform the logical AND operation on a1 and a2 and return the result
 * (0 for true, 1 for false).
 */
char *test_and(char *a1, char *a2)
{
    return (strcmp(a1, ZERO) == 0 && strcmp(a2, ZERO) == 0) ? ZERO : ONE;
}

/*
 * perform the logical OR operation on a1 and a2 and return the result
 * (0 for true, 1 for false).
 */
char *test_or(char *a1, char *a2)
{
    return (strcmp(a1, ZERO) == 0 || strcmp(a2, ZERO) == 0) ? ZERO : ONE;
}

/*
 * check if a file is readable, writeable or executable by the current process.
 *
 * if running in --posix mode, tcsh doesn't use access() call to test r/w/x
 * permissions, it uses the file permission bits.
 *
 * returns 0 if we have the requested r/w/x permission on the file, -1 otherwise.
 */
int test_file_permission(char *path, struct stat *statbuf, char which)
{
    /* check if we are running in --posix mode */
    if(option_set('P'))
    {
        /* determine the permission argument we will pass to access() */
        int uperm, gperm, operm;
        switch(which)
        {
            case 'r':
                uperm = S_IRUSR;
                gperm = S_IRGRP;
                operm = S_IROTH;
                break;

            case 'w':
                uperm = S_IWUSR;
                gperm = S_IWGRP;
                operm = S_IWOTH;
                break;

            case 'x':
                uperm = S_IXUSR;
                gperm = S_IXGRP;
                operm = S_IXOTH;
                break;
        }

        /*
         *  now check the permissions without calling access().
         */
        /* if the user is the file's owner, check for user permissions */
        if((statbuf->st_uid == geteuid()) && (statbuf->st_mode & uperm))
        {
            return 0;
        }
        /* if the user is in the file's group, check for group permissions */
        if((statbuf->st_gid == getegid()) && (statbuf->st_mode & gperm))
        {
            return 0;
        }
        /* check for other users' permissions */
        if(statbuf->st_mode & operm)
        {
            return 0;
        }
        /* we don't have permissions to r/w/x that file */
        return -1;
    }
    else
    {
        /* determine the permission argument we will pass to access() */
        int perm;
        switch(which)
        {
            case 'r':
                perm = R_OK;
                break;

            case 'w':
                perm = W_OK;
                break;

            case 'x':
                perm = X_OK;
                break;
        }
        return access(path, perm);
    }
}

/*
 * perform different tests on arg, which represents a filename.
 *
 * returns "0" if the result of the test is true, "1" if it is false.
 */
char *test_file(char *arg, int op)
{
    uid_t euid = geteuid();
    gid_t egid = getegid();
    memset(&statbuf, 0, sizeof(struct stat));

    /* stat the file */
    int res = lstat(arg, &statbuf);
    
    /* check for stat error */
    if(res != 0 && (op != 'r' && op != 's' && op != 'w' && op != 'x' && op != 'X'))
    {
        res = 1;
    }
    else
    {
        /* perform the test */
        switch(op)
        {
            /* test if the file exists */
            case 'a':
            case 'e':
                res = 0;
                break;
        
            /* is it a block device */
            case 'b':
                res = !(S_ISBLK(statbuf.st_mode));
                break;
            
            /* is it a char device */
            case 'c':
                res = !(S_ISCHR(statbuf.st_mode));
                break;
            
            /* is it a directory */
            case 'd':
                res = !(S_ISDIR(statbuf.st_mode));
                break;
            
            /* is it a regular file */
            case 'f':
                res = !(S_ISREG(statbuf.st_mode));
                break;
            
            /* check if the setgid bit is set */
            case 'g':
                res = !(statbuf.st_mode & S_ISGID);
                break;
            
            /* check if our egid == creator's gid */
            case 'G':
                res = !(statbuf.st_gid == egid);
                break;
            
            /* is it a soft link */
            case 'h':
            case 'L':
                res = !(S_ISLNK(statbuf.st_mode));
                break;
            
            /* check if the sticky bit is set */
            case 'k':
                res = !(statbuf.st_mode & S_ISVTX);
                break;
            
            /* is f1 newer than f2 */
            case 'N':
                res = !(statbuf.st_mtime > statbuf.st_atime);
                break;
            
            /* check if our euid == creator's uid */
            case 'O':
                res = !(statbuf.st_uid == euid);
                break;
            
            /* is it a FIFO/named-pipe */
            case 'p':
                res = !(S_ISFIFO(statbuf.st_mode));
                break;
        
            /* check if the file is readable */
            case 'r':
                res = test_file_permission(arg, &statbuf, 'r');
                break;
            
            /* check if the file size > 0 */
            case 's':
                res = (statbuf.st_size) ? 0 : 1;
                break;
            
            /* is it a socket */
            case 'S':
                res = !(S_ISSOCK(statbuf.st_mode));
                break;
            
            /* check if the setuid bit is set */
            case 'u':
                res = !(statbuf.st_mode & S_ISUID);
                break;
                
            /* check if the file is writeable */
            case 'w':
                res = test_file_permission(arg, &statbuf, 'w');
                break;
                
            /* check if the file is executable */
            case 'x':
                res = test_file_permission(arg, &statbuf, 'x');
                break;
            
            /*
             * csh uses this option to report executable files from $PATH and also builtin utilities.
             * thus '-X ls' gives true (0) result, but '-X /bin/ls' gives false (1) result. bash and ksh
             * don't have this option.
            */
            case 'X':
                if(is_enabled_builtin(arg))
                {
                    res = 0;
                }
                else
                {
                    char *path = search_path(arg, NULL, 1);
                    if(path)
                    {
                        free_malloced_str(path);
                        res = 0;
                    }
                    else
                    {
                        res = 1;
                    }
                }
                break;
        }
    }
    
    /* return the result */
    return res ? ONE : ZERO;
}

/*
 * check if the file descriptor specified in a1 refers to a terminal device.
 */
char *test_file_term(char *a1, char *a2 __attribute__((unused)))
{
    char *s;
    int res = strtol(a1, &s, 10);
    if(*s)
    {
        res = 1;
    }
    else
    {
        res = !isatty(res);
    }
    return res ? ONE : ZERO;
}

/*
 * test if file a1 exists and return "0" if true, "1" if false.
 */
char *test_file_exist (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'e');
}

/*
 * test if file a1 is a block device and return "0" if true, "1" if false.
 */
char *test_file_blk   (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'b');
}

/*
 * test if file a1 is a char device and return "0" if true, "1" if false.
 */
char *test_file_char  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'c');
}

/*
 * test if file a1 is a directory and return "0" if true, "1" if false.
 */
char *test_file_dir   (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'd');
}

/*
 * test if file a1 is a regular file and return "0" if true, "1" if false.
 */
char *test_file_reg   (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'f');
}

/*
 * test if file a1 has its setgid bit set and return "0" if true, "1" if false.
 */
char *test_file_sgid  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'g');
}

/*
 * test if file a1 is a soft link and return "0" if true, "1" if false.
 */
char *test_file_link  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'L');
}

/*
 * test if file a1 has its sticky bit set and return "0" if true, "1" if false.
 */
char *test_file_sticky(char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'k');
}

/*
 * test if file a1 is a FIFO (named pipe) and return "0" if true, "1" if false.
 */
char *test_file_pipe  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'p');
}

/*
 * test if file a1 is readable and return "0" if true, "1" if false.
 */
char *test_file_r     (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'r');
}

/*
 * test if file a1 has a size > 0 and return "0" if true, "1" if false.
 */
char *test_file_size  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 's');
}

/*
 * test if file a1 has its setuid bit set and return "0" if true, "1" if false.
 */
char *test_file_suid  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'u');
}

/*
 * test if file a1 is writeable and return "0" if true, "1" if false.
 */
char *test_file_w     (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'w');
}

/*
 * test if file a1 is executable and return "0" if true, "1" if false.
 */
char *test_file_x     (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'x');
}

/*
 * test if file a1 is owned by this process's group and return "0" if true, "1" if false.
 */
char *test_file_gown  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'G');
}

/*
 * test if file a1 exists and has been modified since its creation and return "0" if true, "1" if false.
 */
char *test_file_new   (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'N');
}

/*
 * test if file a1 is owned by this process's user and return "0" if true, "1" if false.
 */
char *test_file_uown  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'O');
}

/*
 * test if file a1 is a socket and return "0" if true, "1" if false.
 */
char *test_file_sock  (char *a1, char *a2 __attribute__ ((unused)) )
{
    return test_file(a1, 'S');
}

/*
 * check if an option is enabled (the -o test).
 * csh uses this option (-o) to test file ownership, while ksh and bash use it to
 * test set options. we follow the latter.
 */
char *test_opt_en(char *a1, char *a2 __attribute__ ((unused)) )
{
    int res = 1;
    exit_gracefully(0, 0);
    if(*a1 == '?')
    {
        a1++;
        if(strlen(a1) == 1)
        {
            res = !is_short_option(*a1);
        }
        else
        {
            res = !short_option(a1);
        }
    }
    else
    {
        if(strlen(a1) == 1)
        {
            res = !option_set(*a1);
        }
        else
        {
            res = !option_set(short_option(a1));
        }
    }
    return res ? ONE : ZERO;
}

/*
 * test if the length of string a1 is zero and return "0" if true, "1" if false.
 */
char *test_str_zero(char *a1, char *a2 __attribute__ ((unused)) )
{
    return (strlen(a1) == 0) ? ZERO : ONE;
}

/*
 * test if the length of string a1 is not zero and return "0" if true, "1" if false.
 */
char *test_str_nz(char *a1, char *a2 __attribute__ ((unused)) )
{
    return strlen(a1) ? ZERO : ONE;
}

/*
 *  test if the shell varariable a1 has been set and has been given a value (bash).
 */
char *test_var_def(char *a1, char *a2 __attribute__ ((unused)) )
{
    int res = 1;
    struct symtab_entry_s *entry = get_symtab_entry(a1);
    if(entry && entry->val)
    {
        res = 0;
    }
    else
    {
        res = 1;
    }
    return res ? ONE : ZERO;
}


/*
 * extended operator list. here we need to skip the numbers referring to ASCII
 * chars !, (, and ).
 */
#define TEST_GT                1       /* arithm greater than */
#define TEST_LT                2       /* arithm lesser than */
#define TEST_GE                3       /* arithm greater than or equals */
#define TEST_LE                4       /* arithm lesser than or equals */
#define TEST_NE                5       /* arithm not equals */
#define TEST_EQ                6       /* arithm equals */
#define TEST_STR_GT            7       /* string greater than */
#define TEST_STR_LT            8       /* string lesser than */
#define TEST_STR_NE            9       /* string not equals */
#define TEST_STR_EQ1           10      /* string equals '==' */
#define TEST_STR_EQ2           11      /* string equals '=' */
#define TEST_STR_NZ            12      /* string length not zero */
#define TEST_STR_ZERO          13      /* string length is zero */
#define TEST_VAR_REF           14      /* variable reference defined (not currently used) */
#define TEST_VAR_DEF           15      /* variable defined */
#define TEST_OPT_EN            16      /* option enabled */
#define TEST_FILE_OT           17      /* file1 older than file2 */
#define TEST_FILE_NT           18      /* file1 newer than file2 */
#define TEST_FILE_EF           19      /* file1 equals file2 */
#define TEST_FILE_SOCK         20      /* file is socket */
#define TEST_FILE_UOWN         21      /* file is user owned */
#define TEST_FILE_NEW          22      /* file is modified */
#define TEST_FILE_LINK         23      /* file is symlink */
#define TEST_FILE_GOWN         24      /* file is group owned */
#define TEST_FILE_EXE          25      /* file is executable */
#define TEST_FILE_W            26      /* file is writeable */
#define TEST_FILE_SUID         27      /* file has setuid bit set */
#define TEST_FILE_TERM         28      /* file is a terminal */
#define TEST_FILE_SIZE         29      /* file size not zero */
#define TEST_FILE_R            30      /* file is readable */
#define TEST_FILE_PIPE         31      /* file is pipe */
#define TEST_FILE_STICKY       32      /* file has sticky bit set */
#define TEST_FILE_SGID         34      /* file has setgid bit set */
#define TEST_FILE_REG          35      /* file is regular */
#define TEST_FILE_EXIST        36      /* file exists */
#define TEST_FILE_DIR          37      /* file is a dir */
#define TEST_FILE_CHAR         39      /* file is a char dev */
#define TEST_FILE_BLK          42      /* file is a block dev */
#define TEST_AND               43      /* logical AND */
#define TEST_OR                44      /* logical OR */
#define TEST_STR_EQ3           45      /* string equals '=~' */

enum { ASSOC_NONE = 0, ASSOC_LEFT, ASSOC_RIGHT };

/*
 * structure to represent the test operators we recognize.
 */
struct test_op_s
{
	char op;        /* unique number we assign to each operator */
	int  prec;      /* the operator's precedence (see below) */
	int  assoc;     /* the operator's associativity */
	char unary;     /* is it unary or binary operator */
	char chars;     /* how many chars are in the operator (e.g. -eq has 3 chars) */
	char *(*test)(char *a1, char *a2);  /* the function that implements the operator (see above) */
} test_ops[] =
{
	{ '!'               , 1, ASSOC_RIGHT, 1, 1, test_not           },
	{ '('               , 0, ASSOC_NONE , 0, 1, NULL               },
	{ ')'               , 0, ASSOC_NONE , 0, 1, NULL               },
	{ TEST_OR           , 2, ASSOC_LEFT , 0, 2, test_or            },
	{ TEST_AND          , 3, ASSOC_LEFT , 0, 2, test_and           },
	{ TEST_GT           , 4, ASSOC_LEFT , 0, 3, test_gt            },
	{ TEST_LT           , 4, ASSOC_LEFT , 0, 3, test_lt            },
	{ TEST_GE           , 4, ASSOC_LEFT , 0, 3, test_ge            },
	{ TEST_LE           , 4, ASSOC_LEFT , 0, 3, test_le            },
	{ TEST_EQ           , 4, ASSOC_LEFT , 0, 3, test_eq            },
	{ TEST_NE           , 4, ASSOC_LEFT , 0, 3, test_ne            },
	{ TEST_STR_GT       , 4, ASSOC_LEFT , 0, 1, test_str_gt        },
	{ TEST_STR_LT       , 4, ASSOC_LEFT , 0, 1, test_str_lt        },
	{ TEST_STR_EQ1      , 4, ASSOC_LEFT , 0, 2, test_str_eq        },
	{ TEST_STR_EQ2      , 4, ASSOC_LEFT , 0, 1, test_str_eq        },
	{ TEST_STR_NE       , 4, ASSOC_LEFT , 0, 2, test_str_ne        },    
	{ TEST_STR_NZ       , 4, ASSOC_RIGHT, 1, 2, test_str_nz        },
	{ TEST_STR_ZERO     , 4, ASSOC_RIGHT, 1, 2, test_str_zero      },
	//{ TEST_VAR_REF      , 4, ASSOC_RIGHT, 1, 2, test_var_ref       },
	{ TEST_VAR_DEF      , 4, ASSOC_RIGHT, 1, 2, test_var_def       },
	{ TEST_OPT_EN       , 4, ASSOC_RIGHT, 1, 2, test_opt_en        },
	{ TEST_FILE_OT      , 4, ASSOC_LEFT , 0, 3, test_file_ot       },
	{ TEST_FILE_NT      , 4, ASSOC_LEFT , 0, 3, test_file_nt       },
	{ TEST_FILE_EF      , 4, ASSOC_LEFT , 0, 3, test_file_ef       },
	{ TEST_FILE_SOCK    , 4, ASSOC_RIGHT, 1, 2, test_file_sock     },
	{ TEST_FILE_UOWN    , 4, ASSOC_RIGHT, 1, 2, test_file_uown     },
	{ TEST_FILE_NEW     , 4, ASSOC_RIGHT, 1, 2, test_file_new      },
	{ TEST_FILE_LINK    , 4, ASSOC_RIGHT, 1, 2, test_file_link     },
	{ TEST_FILE_GOWN    , 4, ASSOC_RIGHT, 1, 2, test_file_gown     },
	{ TEST_FILE_EXE     , 4, ASSOC_RIGHT, 1, 2, test_file_x        },
	{ TEST_FILE_W       , 4, ASSOC_RIGHT, 1, 2, test_file_w        },
	{ TEST_FILE_SUID    , 4, ASSOC_RIGHT, 1, 2, test_file_suid     },
	{ TEST_FILE_TERM    , 4, ASSOC_RIGHT, 1, 2, test_file_term     },
	{ TEST_FILE_SIZE    , 4, ASSOC_RIGHT, 1, 2, test_file_size     },
	{ TEST_FILE_R       , 4, ASSOC_RIGHT, 1, 2, test_file_r        },
	{ TEST_FILE_PIPE    , 4, ASSOC_RIGHT, 1, 2, test_file_pipe     },
	{ TEST_FILE_STICKY  , 4, ASSOC_RIGHT, 1, 2, test_file_sticky   },
	{ TEST_FILE_SGID    , 4, ASSOC_RIGHT, 1, 2, test_file_sgid     },
	{ TEST_FILE_REG     , 4, ASSOC_RIGHT, 1, 2, test_file_reg      },
	{ TEST_FILE_EXIST   , 4, ASSOC_RIGHT, 1, 2, test_file_exist    },
	{ TEST_FILE_DIR     , 4, ASSOC_RIGHT, 1, 2, test_file_dir      },
	{ TEST_FILE_CHAR    , 4, ASSOC_RIGHT, 1, 2, test_file_char     },
	{ TEST_FILE_BLK     , 4, ASSOC_RIGHT, 1, 2, test_file_blk      },
	{ TEST_STR_EQ3      , 4, ASSOC_LEFT , 0, 1, test_str_eq_ext    },
};

/* pointers to the operators' structs (see above) */
struct test_op_s *TEST_OP_NOT         = &test_ops[0 ];
struct test_op_s *TEST_OP_LBRACE      = &test_ops[1 ];
struct test_op_s *TEST_OP_RBRACE      = &test_ops[2 ];
struct test_op_s *TEST_OP_OR          = &test_ops[3 ];
struct test_op_s *TEST_OP_AND         = &test_ops[4 ];
struct test_op_s *TEST_OP_GT          = &test_ops[5 ];
struct test_op_s *TEST_OP_LT          = &test_ops[6 ];
struct test_op_s *TEST_OP_GE          = &test_ops[7 ];
struct test_op_s *TEST_OP_LE          = &test_ops[8 ];
struct test_op_s *TEST_OP_EQ          = &test_ops[9 ];
struct test_op_s *TEST_OP_NE          = &test_ops[10];
struct test_op_s *TEST_OP_STR_GT      = &test_ops[11];
struct test_op_s *TEST_OP_STR_LT      = &test_ops[12];
struct test_op_s *TEST_OP_STR_EQ1     = &test_ops[13];
struct test_op_s *TEST_OP_STR_EQ2     = &test_ops[14];
struct test_op_s *TEST_OP_STR_NE      = &test_ops[15];
struct test_op_s *TEST_OP_STR_NZ      = &test_ops[16];
struct test_op_s *TEST_OP_STR_ZERO    = &test_ops[17];
//struct test_op_s *TEST_OP_VAR_REF     = &test_ops[18];
struct test_op_s *TEST_OP_VAR_DEF     = &test_ops[18];
struct test_op_s *TEST_OP_OPT_EN      = &test_ops[19];
struct test_op_s *TEST_OP_FILE_OT     = &test_ops[20];
struct test_op_s *TEST_OP_FILE_NT     = &test_ops[21];
struct test_op_s *TEST_OP_FILE_EF     = &test_ops[22];
struct test_op_s *TEST_OP_FILE_SOCK   = &test_ops[23];
struct test_op_s *TEST_OP_FILE_UOWN   = &test_ops[24];
struct test_op_s *TEST_OP_FILE_NEW    = &test_ops[25];
struct test_op_s *TEST_OP_FILE_LINK   = &test_ops[26];
struct test_op_s *TEST_OP_FILE_GOWN   = &test_ops[27];
struct test_op_s *TEST_OP_FILE_EXE    = &test_ops[28];
struct test_op_s *TEST_OP_FILE_W      = &test_ops[29];
struct test_op_s *TEST_OP_FILE_SUID   = &test_ops[30];
struct test_op_s *TEST_OP_FILE_TERM   = &test_ops[31];
struct test_op_s *TEST_OP_FILE_SIZE   = &test_ops[32];
struct test_op_s *TEST_OP_FILE_R      = &test_ops[33];
struct test_op_s *TEST_OP_FILE_PIPE   = &test_ops[34];
struct test_op_s *TEST_OP_FILE_STICKY = &test_ops[35];
struct test_op_s *TEST_OP_FILE_SGID   = &test_ops[36];
struct test_op_s *TEST_OP_FILE_REG    = &test_ops[37];
struct test_op_s *TEST_OP_FILE_EXIST  = &test_ops[38];
struct test_op_s *TEST_OP_FILE_DIR    = &test_ops[39];
struct test_op_s *TEST_OP_FILE_CHAR   = &test_ops[40];
struct test_op_s *TEST_OP_FILE_BLK    = &test_ops[41];
struct test_op_s *TEST_OP_STR_EQ3     = &test_ops[42];


/*
 * return 1 if the operator op is a string operator, i.e. >, <, =, ==, =~, or !=.
 */
int is_str_op(struct test_op_s *op)
{
    switch(op->op)
    {
        case TEST_STR_GT :
        case TEST_STR_LT :
        case TEST_STR_EQ1:
        case TEST_STR_EQ2:
        case TEST_STR_EQ3:
        case TEST_STR_NE :
            return 1;
    }
    return 0;
}


/*
 * test the first chars of 'expr' to see if they represent one of the test operators
 * listed above.. if that is the case, return the operator struct representing that
 * operator.. otherwise, return NULL.
 *
 * in the 'old' test command (invoked as either 'test' or '[', but not '[['), these operators
 * work differently than when the 'new' test command (invoked as '[[') is used:
 *
 *    -a means logical AND (&&), not test if file is available.
 *    -o means logical OR  (||), not test if option is set.
 * 
 * the argument oldtest tells us if we were called using the old test command or not.
 */
struct test_op_s *test_getop(char *expr, int oldtest)
{
    switch(*expr)
    {
        case '-':
            if(!expr[1]) return NULL;
            if(expr[2] == '\0')
            {
                switch(expr[1])
                {
                    case 'a':
                        if(oldtest)
                        {
                            return TEST_OP_AND;
                        }
                        /* fall through */
                        __attribute__((fallthrough));
                        
                    case 'e':
                        return TEST_OP_FILE_EXIST;
                    
                    case 'h':
                    case 'L':
                        return TEST_OP_FILE_LINK;
                    
                    case 'b':
                        return TEST_OP_FILE_BLK;

                    case 'c':
                        return TEST_OP_FILE_CHAR;

                    case 'd':
                        return TEST_OP_FILE_DIR;

                    case 'f':
                        return TEST_OP_FILE_REG;

                    case 'g':
                        return TEST_OP_FILE_SGID;

                    case 'G':
                        return TEST_OP_FILE_GOWN;

                    case 'k':
                        return TEST_OP_FILE_STICKY;

                    case 'n':
                        return TEST_OP_STR_NZ;

                    case 'N':
                        return TEST_OP_FILE_NEW;
                    
                    case 'o':
                        if(oldtest)
                        {
                            return TEST_OP_OR;
                        }
                        return TEST_OP_OPT_EN;
                        
                    case 'O':
                        return TEST_OP_FILE_UOWN;

                    case 'p':
                        return TEST_OP_FILE_PIPE;

                    case 'r':
                        return TEST_OP_FILE_R;

                    //case 'R': return TEST_OP_VAR_REF;

                    case 's':
                        return TEST_OP_FILE_SIZE;

                    case 'S':
                        return TEST_OP_FILE_SOCK;

                    case 't':
                        return TEST_OP_FILE_TERM;

                    case 'u':
                        return TEST_OP_FILE_SUID;

                    case 'w':
                        return TEST_OP_FILE_W;

                    case 'x':
                        return TEST_OP_FILE_EXE;

                    case 'v':
                        return TEST_OP_VAR_DEF;

                    case 'z':
                        return TEST_OP_STR_ZERO;
                }
            }
            else
            {
                if(strcmp(expr, "-ef") == 0)
                {
                    return TEST_OP_FILE_EF;
                }
                if(strcmp(expr, "-nt") == 0)
                {
                    return TEST_OP_FILE_NT;
                }
                if(strcmp(expr, "-ot") == 0)
                {
                    return TEST_OP_FILE_OT;
                }
                if(strcmp(expr, "-eq") == 0)
                {
                    return TEST_OP_EQ;
                }
                if(strcmp(expr, "-ne") == 0)
                {
                    return TEST_OP_NE;
                }
                if(strcmp(expr, "-lt") == 0)
                {
                    return TEST_OP_LT;
                }
                if(strcmp(expr, "-le") == 0)
                {
                    return TEST_OP_LE;
                }
                if(strcmp(expr, "-gt") == 0)
                {
                    return TEST_OP_GT;
                }
                if(strcmp(expr, "-ge") == 0)
                {
                    return TEST_OP_GE;
                }
            }
            return NULL;
            
        case '>':
            return TEST_OP_STR_GT;

        case '<':
            return TEST_OP_STR_LT;

        case '!':
            if(expr[1] == '=')
            {
                return TEST_OP_STR_NE;
            }
            return TEST_OP_NOT;

        case '=':
            if(expr[1] == '=')
            {
                return TEST_OP_STR_EQ1;
            }
            else if(expr[1] == '~')
            {
                return TEST_OP_STR_EQ3;
            }
            return TEST_OP_STR_EQ2;
            
        case '&':
            if(expr[1] == '&')
            {
                return TEST_OP_AND;
            }
            break;
            
        case '|':
            if(expr[1] == '|')
            {
                return TEST_OP_OR;
            }
            break;
        
        case '(':
            if(expr[1] == '\0')
            {
                return TEST_OP_LBRACE;
            }
            break;

        case ')':
            if(expr[1] == '\0')
            {
                return TEST_OP_RBRACE;
            }
            break;
    }
    return NULL;
}

/*
 * push the operator op on the operator stack.
 */
void test_push_opstack(struct test_op_s *op)
{
    if(test_nopstack > MAXOPSTACK-1)
    {
        PRINT_ERROR("%s: operator stack overflow\n", UTILITY);
        test_err = 1;
        return;
    }
    test_opstack[test_nopstack++] = op;
}


/*
 * pop the last operator off the operator stack.
 */
struct test_op_s *test_pop_opstack(void)
{
    if(!test_nopstack)
    {
        PRINT_ERROR("%s: operator stack empty\n", UTILITY);
        test_err = 1;
        return NULL;
    }
    return test_opstack[--test_nopstack];
}


/*
 * push the value val on the operands stack.
 */
void test_push_stack(char *val)
{
    if(!val)
    {
        test_err = 1;
        return;
    }

    if(nteststack > MAXTESTSTACK-1)
    {
        PRINT_ERROR("%s: test stack overflow\n", UTILITY);
        test_err = 1;
        return;
    }
    teststack[nteststack++] = get_malloced_str(val);
}


/*
 * pop the last operand off the operands stack.
 */
char *test_pop_stack(void)
{
    if(!nteststack)
    {
        PRINT_ERROR("%s: test stack empty\n", UTILITY);
        test_err = 1;
        return "";
    }
    return teststack[--nteststack];
}


/*
 * perform the shunt operation (see the Wikipedia link on top of this page for
 * details on what this entails).
 */
void test_shunt_op(struct test_op_s *op)
{
    struct test_op_s *pop;
    test_err = 0;

    /* operator is an opening brace */
    if(op->op == '(')
    {
        test_push_opstack(op);
        return;
    }
    /* operator is a closing brace */
    else if(op->op == ')')
    {
        while(test_nopstack > 0 && test_opstack[test_nopstack-1]->op != '(')
        {
            pop = test_pop_opstack();
            if(test_err)
            {
                return;
            }

            char *n1 = test_pop_stack();
            if(test_err)
            {
                return;
            }
            
            /* check if its unary or binary operator */
            if(pop->unary)
            {
                test_push_stack(pop->test(n1, 0));
            }
            else
            {
                char *n2 = test_pop_stack();
                if(test_err)
                {
                    free_malloced_str(n1);
                    return;
                }

                test_push_stack(pop->test(n2, n1));
                
                if(test_err)
                {
                    free_malloced_str(n1);
                    free_malloced_str(n2);
                    return;
                }
            }
        }
        
        if(!(pop = test_pop_opstack()) || pop->op != '(')
        {
            PRINT_ERROR("%s: test stack error: no matching \'(\'\n", UTILITY);
            test_err = 1;
        }
        return;
    }
    
    /* check for operators with right-associativity */
    if(op->assoc == ASSOC_RIGHT)
    {
        while(test_nopstack && op->prec < test_opstack[test_nopstack-1]->prec)
        {
            pop = test_pop_opstack();
            if(test_err)
            {
                return;
            }
            
            char *n1 = test_pop_stack();
            if(test_err)
            {
                return;
            }
            
            /* check if its unary or binary operator */
            if(pop->unary)
            {
                test_push_stack(pop->test(n1, 0));
            }
            else
            {
                char *n2 = test_pop_stack();
                if(test_err)
                {
                    free_malloced_str(n1);
                    return;
                }
                test_push_stack(pop->test(n2, n1));
                free_malloced_str(n2);
            }
                    
            free_malloced_str(n1);
            
            if(test_err)
            {
                return;
            }
        }
    }
    else
    {
        while(test_nopstack && op->prec <= test_opstack[test_nopstack-1]->prec)
        {
            pop = test_pop_opstack();
            if(test_err)
            {
                return;
            }
            
            char *n1 = test_pop_stack();
            if(test_err)
            {
                return;
            }
            
            if(pop->unary)
            {
                test_push_stack(pop->test(n1, 0));
            }
            else
            {
                char *n2 = test_pop_stack();
                if(test_err)
                {
                    free_malloced_str(n1);
                    return;
                }
                test_push_stack(pop->test(n2, n1));
                free_malloced_str(n2);
            }

            free_malloced_str(n1);
            
            if(test_err)
            {
                return;
            }
        }
    }
    
    test_push_opstack(op);
}


#define TEST_ERR_RETURN(val)                \
do                                          \
{                                           \
    int v;                                  \
    for(v = 0; v < nteststack; v++)         \
    {                                       \
        free_malloced_str(teststack[v]);    \
    }                                       \
    return val;                             \
} while(0)


#define CHECK_ERR_FLAG()                    \
if(test_err)                                \
{                                           \
    TEST_ERR_RETURN(2);                     \
}


/*
 * the test (or '[') builtin utility (non-POSIX).. used to test conditional expressions.
 *
 * returns 0 if all conditions evaluated as true, 1 otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help test` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int test_builtin(int argc, char **argv)
{
    /* our stacks */
    struct test_op_s *__test_opstack[MAXOPSTACK];
    char  *__teststack[MAXTESTSTACK];
    test_opstack = __test_opstack;
    teststack = __teststack;

    char   *expr;
    char   *tstart = NULL;
    int     i = 1;
    int     oldtest = 1;
    /* dummy operator to mark start */
    struct  test_op_s startop = { 'X', 0, ASSOC_NONE, 0, 0, NULL };
    /* current operator being parsed */
    struct  test_op_s *op     = NULL;
    /* last operator parsed */
    struct  test_op_s *lastop = &startop;

    /* empty the stacks */
    test_nopstack = 0;
    nteststack = 0;

    /* reset the error flag */
    test_err = 0;

    /* the '[' and '[[' versions of test need closing ']' and ']]' respectively */
    if(strcmp(argv[0], "[") == 0)
    {
        if(strcmp(argv[argc-1], "]"))
        {
            PRINT_ERROR("%s: missing closing bracket: ']'\n", UTILITY);
            return 2;
        }
        argc--;
    }
    else if(strcmp(argv[0], "[[") == 0)
    {
        if(strcmp(argv[argc-1], "]]"))
        {
            PRINT_ERROR("%s: missing closing bracket: ']]'\n", UTILITY);
            return 2;
        }
        argc--;
        oldtest = 0;
    }
    
    /* parse the arguments */
    for( ; i < argc; i++)
    {
        expr = argv[i];
        debug ("expr = '%s'\n", expr);

        /* skip leading whitespaces */
        while(*expr && isspace(*expr))
        {
            expr++;
        }
        
#if 0
        /* special handling of braced operators */
        size_t len = strlen(expr);
        if(*expr == '(' && expr[len-1] == ')')
        {
            char *subexpr = get_malloced_strl(expr, 1, len-2);
            int res = test_recursive(subexpr);
            free_malloced_str(subexpr);
            
            char buf[16];
            sprintf(buf, "%d", res);
            debug ("buf = '%s'\n", buf);
            test_push_stack(buf);
            CHECK_ERR_FLAG();
            tstart = NULL;
            lastop = NULL;
            continue;
        }
#endif

        /* get the next operand or operator */
        if(!tstart)
        {
            if((op = test_getop(expr, oldtest)))
            {
                if(lastop && (lastop == &startop || lastop->op != ')'))
                {
                    if(op->op != '(' && !op->unary)
                    {
                        if(is_str_op(op))
                        {
                            test_push_stack("");
                        }
                        else
                        {
                            PRINT_ERROR("%s: illegal use of binary operator (%c)\n", UTILITY, op->op);
                            TEST_ERR_RETURN(2);
                        }
                    }
                }
                test_err = 0;
                test_shunt_op(op);
                CHECK_ERR_FLAG();
                lastop = op;
            }
            else
            {
                tstart = expr;
            }
        }
        else
        {
            if((op = test_getop(expr, oldtest)))
            {
                test_err = 0;
                test_push_stack(tstart);
                CHECK_ERR_FLAG();
                tstart = NULL;
                test_shunt_op(op);
                CHECK_ERR_FLAG();
                lastop = op;
            }
            else
            {
                test_err = 0;
                test_push_stack(tstart);
                CHECK_ERR_FLAG();
                tstart = NULL;
                lastop = NULL;
            }
        }
    }

    /* we have one last operand */
    if(tstart)
    {
        test_err = 0;
        if(i == 2)
        {
            test_shunt_op(TEST_OP_STR_NZ);
            CHECK_ERR_FLAG();
        }
        test_push_stack(tstart);
        CHECK_ERR_FLAG();
    }
    
    /* treat an isolated operand 'x' as testing for '-n x' */
    if(!test_nopstack && nteststack)
    {
        return atoi(test_str_nz(teststack[0], NULL));
    }

    /* now pop the operators off the stack and perform each one of them */
    while(test_nopstack)
    {
        test_err = 0;
        op = test_pop_opstack();
        CHECK_ERR_FLAG();

        char *n1 = test_pop_stack();
        CHECK_ERR_FLAG();

        if(op->unary)
        {
            test_push_stack(op->test(n1, 0));
        }
        else
        {
            char *n2 = test_pop_stack();
            if(test_err)
            {
                free_malloced_str(n1);
                TEST_ERR_RETURN(2);
            }
            test_push_stack(op->test(n2, n1));
            free_malloced_str(n2);
        }

        free_malloced_str(n1);
        CHECK_ERR_FLAG();
    }

    /* at the end, we should have only one value remaining on the operands stack */
    if(nteststack != 1)
    {
        PRINT_ERROR("%s: test stack has %d elements after evaluation (should be 1)\n", UTILITY, nteststack);
        return 2;
    }

    debug (" -- %s\n", teststack[0]);
    return atoi(teststack[0]);
}
