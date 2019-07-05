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
#include "../cmd.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY             "test"
#define MAXOPSTACK          64
#define MAXTESTSTACK        64
#define ZERO                "0"
#define ONE                 "1"

struct test_op_s *test_opstack[MAXOPSTACK];
int    test_nopstack  = 0;

char  *teststack[MAXTESTSTACK];
int    nteststack     = 0;
int    test_err       = 0;
char  *test_empty_str = "";

/* fnmatch() flags */
static int flags = FNM_NOESCAPE | FNM_PATHNAME | FNM_PERIOD;

struct stat statbuf ;
struct stat statbuf2;

#define FILES_EQUAL     1
#define FILE_NEWER_THAN 2
#define FILE_OLDER_THAN 3

/*
 * NOTE: in all of the upcoming comparisons in this file, remember that
 *       we always need to invert the result of any given test, so that 
 *       zero will indicate success and non-zero will indicate failure.
 */

char *compare_files(char *f1, char *f2, int op)
{
    memset((void *)&statbuf , 0, sizeof(struct stat));
    memset((void *)&statbuf2, 0, sizeof(struct stat));
    int res1 = lstat(f1, &statbuf );
    int res2 = lstat(f2, &statbuf2);
    switch(op)
    {
        case FILES_EQUAL:
            if(!res1 || !res2) return ONE;
            if(statbuf.st_ino == statbuf2.st_ino && 
               statbuf.st_dev == statbuf2.st_dev &&
               statbuf.st_rdev == statbuf2.st_rdev)
                return ZERO;
            return ONE;
            
        case FILE_NEWER_THAN:
            if(!res1) return ONE;
            if(!res2) return ZERO;
            return (statbuf.st_mtime <= statbuf2.st_mtime) ? ONE : ZERO;
            
        case FILE_OLDER_THAN:
            if(!res2) return ONE;
            if(!res1) return ZERO;
            return (statbuf.st_mtime >= statbuf2.st_mtime) ? ONE : ZERO;
    }
    return ONE;
}


#define ARITHM_EQ       1
#define ARITHM_GE       2
#define ARITHM_GT       3
#define ARITHM_LE       4
#define ARITHM_LT       5
#define ARITHM_NE       6

char *compare_exprs(char *__e1, char *__e2, int op)
{
    /*
     * NOTE: bash treats e1 and e2 as arithmetic expressions, which are evaluated
     *       as any other arithmetic expr enclosed in $(( )) or (( )).
     */
    char *strend = NULL, *err = NULL;
    char *e1 = __do_arithmetic(__e1), *e2 = NULL;
    if(!e1) { err = __e1; goto bailout; }

    long res1 = strtol(e1, &strend, 10);
    if(strend == e1) { err = __e1; goto bailout; }

    e2 = __do_arithmetic(__e2);
    if(!e2) { err = __e2; goto bailout; }
    long res2 = strtol(e2, &strend, 10);
    if(strend == e2) { err = __e2; goto bailout; }

    int res = 2;    /* 2 means invalid operator (general bash test_err code) */
    switch(op)
    {
        case ARITHM_EQ  : res = !(res1 == res2); break;
        case ARITHM_GE  : res = !(res1 >= res2); break;
        case ARITHM_GT  : res = !(res1  > res2); break;
        case ARITHM_LE  : res = !(res1 <= res2); break;
        case ARITHM_LT  : res = !(res1  < res2); break;
        case ARITHM_NE  : res = !(res1 != res2); break;
    }
    free(e1);
    free(e2);
    return res ? ONE : ZERO;
    
bailout:
    fprintf(stderr, "%s: invalid arithmetic expression: %s\r\n", UTILITY, err);
    if(e1) free(e1);
    if(e2) free(e2);
    test_err = 1;
    return NULL;
}

char *test_gt(char *a1, char *a2) { return compare_exprs(a1, a2, ARITHM_GT); }
char *test_lt(char *a1, char *a2) { return compare_exprs(a1, a2, ARITHM_LT); }
char *test_ge(char *a1, char *a2) { return compare_exprs(a1, a2, ARITHM_GE); }
char *test_le(char *a1, char *a2) { return compare_exprs(a1, a2, ARITHM_LE); }
char *test_eq(char *a1, char *a2) { return compare_exprs(a1, a2, ARITHM_EQ); }
char *test_ne(char *a1, char *a2) { return compare_exprs(a1, a2, ARITHM_NE); }

char *test_str_eq(char *a1, char *a2) { return fnmatch(a1, a2, flags) ? ONE : ZERO; }
char *test_str_ne(char *a1, char *a2) { return fnmatch(a1, a2, flags) ? ZERO : ONE; }
char *test_str_lt(char *a1, char *a2) { return (strcmp(a1, a2)   < 0) ? ZERO : ONE; }
char *test_str_gt(char *a1, char *a2) { return (strcmp(a1, a2)   > 0) ? ZERO : ONE; }

char *test_file_ef(char *a1, char *a2) { return compare_files(a1, a2, FILES_EQUAL    ); }
char *test_file_nt(char *a1, char *a2) { return compare_files(a1, a2, FILE_NEWER_THAN); }
char *test_file_ot(char *a1, char *a2) { return compare_files(a1, a2, FILE_OLDER_THAN); }

char *test_not(char *a1, char *a2 __attribute__ ((unused)) )
{
    return strcmp(a1, ZERO) == 0 ? ONE : ZERO;
}

char *test_and(char *a1, char *a2)
{
    if(strcmp(a1, ZERO) != 0 || strcmp(a2, ZERO) != 0) return ONE;
    return ZERO;
}

char *test_or(char *a1, char *a2)
{
    if(strcmp(a1, ZERO) == 0 || strcmp(a2, ZERO) == 0) return ZERO;
    return ONE;
}

char *test_file(char *arg, int op)
{
    uid_t euid = geteuid();
    gid_t egid = getegid();
    memset((void *)&statbuf, 0, sizeof(struct stat));
    int res = lstat(arg, &statbuf);
    switch(op)
    {
        case 'a':
        case 'e':
            if(res != 0) res = 1;
            res = 0;
            break;
        
        case 'b':
            if(res != 0) res = 1;
            else if(S_ISBLK(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
            
        case 'c':
            if(res != 0) res = 1;
            else if(S_ISCHR(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
            
        case 'd':
            if(res != 0) res = 1;
            else if(S_ISDIR(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
            
        case 'f':
            if(res != 0) res = 1;
            else if(S_ISREG(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
            
        case 'g':
            if(res != 0) res = 1;
            else if(statbuf.st_mode & S_ISGID) res = 0;
            else res = 1;
            break;
            
        case 'G':
            if(res != 0) res = 1;
            else if(statbuf.st_gid == egid) res = 0;
            else res = 1;
            break;
            
        case 'h':
        case 'L':
            if(res != 0) res = 1;
            else if(S_ISLNK(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
            
        case 'k':
            if(res != 0) res = 1;
            else if(statbuf.st_mode & S_ISVTX) res = 0;
            else res = 1;
            break;
            
        case 'N':
            if(res != 0) res = 1;
            else if(statbuf.st_mtime > statbuf.st_atime) res = 0;
            else res = 1;
            break;
            
        case 'O':
            if(res != 0) res = 1;
            else if(statbuf.st_uid == euid) res = 0;
            else res = 1;
            break;
            
        case 'p':
            if(res != 0) res = 1;
            else if(S_ISFIFO(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
        
        /*
         * if running in POSIX mode, tcsh doesn't use access() call to test rwx 
         * permissions, it uses the file permission bits.
         */
        case 'r':
            res = access(arg, R_OK);
            break;
            
        case 's':
            res = (statbuf.st_size) ? 0 : 1;
            break;
            
        case 'S':
            if(res != 0) res = 1;
            else if(S_ISSOCK(statbuf.st_mode)) res = 0;
            else res = 1;
            break;
            
        case 'u':
            if(res != 0) res = 1;
            else if(statbuf.st_mode & S_ISUID) res = 0;
            else res = 1;
            break;
                
        case 'w':
            res = access(arg, W_OK);
            break;
                
        case 'x':
            res = access(arg, X_OK);
            break;
            
        /*
         * csh uses this option to report executable files from $PATH and also builtin utilities.
         * thus '-X ls' gives true (0) result, but '-X /bin/ls' gives false (1) result. bash and ksh
         * don't have this option.
         */
        case 'X':
            if(is_enabled_builtin(arg)) res = 0;
            else
            {
                char *path = search_path(arg, NULL, 1);
                if(path)
                {
                    free_malloced_str(path);
                    res = 0;
                }
                else res = 1;
            }
            break;
    }
    return res ? ONE : ZERO;
}

char *test_file_term(char *a1, char *a2)
{
    char *s = a1;
    int res = strtol(a1, &s, 10);
    if(s == a1)
    {
        //fprintf(stderr, "%s: invalid file descriptor: %s\r\n", UTILITY, arg);
        res = 1;
    }
    else
    {
        res = !isatty(res);
    }
    return res ? ONE : ZERO;
}

char *test_file_exist (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'e'); }
char *test_file_blk   (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'b'); }
char *test_file_char  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'c'); }
char *test_file_dir   (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'd'); }
char *test_file_reg   (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'f'); }
char *test_file_sgid  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'g'); }
char *test_file_link  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'L'); }
char *test_file_sticky(char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'k'); }
char *test_file_pipe  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'p'); }
char *test_file_r     (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'r'); }
char *test_file_size  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 's'); }
char *test_file_suid  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'u'); }
char *test_file_w     (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'w'); }
char *test_file_x     (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'x'); }
char *test_file_gown  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'G'); }
char *test_file_new   (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'N'); }
char *test_file_uown  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'O'); }
char *test_file_sock  (char *a1, char *a2 __attribute__ ((unused)) ) { return test_file(a1, 'S'); }

char *test_opt_en(char *a1, char *a2 __attribute__ ((unused)) )
{
    /*
     * csh uses this option to test file ownership, while ksh and bash uses it to
     * test set options. we follow the latter.
     */
    int res = 1;
    if(*a1 == '?')
    {
        a1++;
        if(strlen(a1) == 1) res = !is_short_option(*a1);
        else res = !short_option(a1);
    }
    else
    {
        if(strlen(a1) == 1) res = !option_set(*a1);
        else res = !option_set(short_option(a1));
    }
    return res ? ONE : ZERO;
}

char *test_str_zero(char *a1, char *a2 __attribute__ ((unused)) )
{
    return (strlen(a1) == 0) ? ZERO : ONE;
}

char *test_str_nz(char *a1, char *a2 __attribute__ ((unused)) )
{
    return strlen(a1) ? ZERO : ONE;
}

char *test_var_def(char *a1, char *a2 __attribute__ ((unused)) )
{
    /* check if a shell var has been set and given a value (bash) */
    int res = 1;
    struct symtab_entry_s *entry = get_symtab_entry(a1);
    if(entry && entry->val) res = 0;
    else res = 1;
    return res ? ONE : ZERO;
}


/*
 * extended operator list. here we need to skip ASCII numbers for !, (, )
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

enum { ASSOC_NONE = 0, ASSOC_LEFT, ASSOC_RIGHT };

struct test_op_s
{
	char op;
	int  prec;
	int  assoc;
	char unary;
	char chars;
	char *(*test)(char *a1, char *a2);
} test_ops[] =
{
	{ '!'               , 1, ASSOC_RIGHT, 1, 1, test_not           },
	{ '('               , 0, ASSOC_NONE , 0, 1, NULL               },
	{ ')'               , 0, ASSOC_NONE , 0, 1, NULL               },
	{ TEST_AND          , 2, ASSOC_LEFT , 0, 2, test_and           },
	{ TEST_OR           , 2, ASSOC_LEFT , 0, 2, test_or            },
	{ TEST_GT           , 3, ASSOC_LEFT , 0, 3, test_gt            },
	{ TEST_LT           , 3, ASSOC_LEFT , 0, 3, test_lt            },
	{ TEST_GE           , 3, ASSOC_LEFT , 0, 3, test_ge            },
	{ TEST_LE           , 3, ASSOC_LEFT , 0, 3, test_le            },
	{ TEST_EQ           , 3, ASSOC_LEFT , 0, 3, test_eq            },
	{ TEST_NE           , 3, ASSOC_LEFT , 0, 3, test_ne            },
	{ TEST_STR_GT       , 3, ASSOC_LEFT , 0, 1, test_str_gt        },
	{ TEST_STR_LT       , 3, ASSOC_LEFT , 0, 1, test_str_lt        },
	{ TEST_STR_EQ1      , 3, ASSOC_LEFT , 0, 2, test_str_eq        },
	{ TEST_STR_EQ2      , 3, ASSOC_LEFT , 0, 1, test_str_eq        },
	{ TEST_STR_NE       , 3, ASSOC_LEFT , 0, 2, test_str_ne        },    
	{ TEST_STR_NZ       , 3, ASSOC_RIGHT, 1, 2, test_str_nz        },
	{ TEST_STR_ZERO     , 3, ASSOC_RIGHT, 1, 2, test_str_zero      },
	//{ TEST_VAR_REF      , 3, ASSOC_RIGHT, 1, 2, test_var_ref       },
	{ TEST_VAR_DEF      , 3, ASSOC_RIGHT, 1, 2, test_var_def       },
	{ TEST_OPT_EN       , 3, ASSOC_RIGHT, 1, 2, test_opt_en        },
	{ TEST_FILE_OT      , 3, ASSOC_LEFT , 0, 3, test_file_ot       },
	{ TEST_FILE_NT      , 3, ASSOC_LEFT , 0, 3, test_file_nt       },
	{ TEST_FILE_EF      , 3, ASSOC_LEFT , 0, 3, test_file_ef       },
	{ TEST_FILE_SOCK    , 3, ASSOC_RIGHT, 1, 2, test_file_sock     },
	{ TEST_FILE_UOWN    , 3, ASSOC_RIGHT, 1, 2, test_file_uown     },
	{ TEST_FILE_NEW     , 3, ASSOC_RIGHT, 1, 2, test_file_new      },
	{ TEST_FILE_LINK    , 3, ASSOC_RIGHT, 1, 2, test_file_link     },
	{ TEST_FILE_GOWN    , 3, ASSOC_RIGHT, 1, 2, test_file_gown     },
	{ TEST_FILE_EXE     , 3, ASSOC_RIGHT, 1, 2, test_file_x        },
	{ TEST_FILE_W       , 3, ASSOC_RIGHT, 1, 2, test_file_w        },
	{ TEST_FILE_SUID    , 3, ASSOC_RIGHT, 1, 2, test_file_suid     },
	{ TEST_FILE_TERM    , 3, ASSOC_RIGHT, 1, 2, test_file_term     },
	{ TEST_FILE_SIZE    , 3, ASSOC_RIGHT, 1, 2, test_file_size     },
	{ TEST_FILE_R       , 3, ASSOC_RIGHT, 1, 2, test_file_r        },
	{ TEST_FILE_PIPE    , 3, ASSOC_RIGHT, 1, 2, test_file_pipe     },
	{ TEST_FILE_STICKY  , 3, ASSOC_RIGHT, 1, 2, test_file_sticky   },
	{ TEST_FILE_SGID    , 3, ASSOC_RIGHT, 1, 2, test_file_sgid     },
	{ TEST_FILE_REG     , 3, ASSOC_RIGHT, 1, 2, test_file_reg      },
	{ TEST_FILE_EXIST   , 3, ASSOC_RIGHT, 1, 2, test_file_exist    },
	{ TEST_FILE_DIR     , 3, ASSOC_RIGHT, 1, 2, test_file_dir      },
	{ TEST_FILE_CHAR    , 3, ASSOC_RIGHT, 1, 2, test_file_char     },
	{ TEST_FILE_BLK     , 3, ASSOC_RIGHT, 1, 2, test_file_blk      },
};

struct test_op_s *TEST_OP_NOT         = &test_ops[0 ];
struct test_op_s *TEST_OP_LBRACE      = &test_ops[1 ];
struct test_op_s *TEST_OP_RBRACE      = &test_ops[2 ];
struct test_op_s *TEST_OP_AND         = &test_ops[3 ];
struct test_op_s *TEST_OP_OR          = &test_ops[4 ];
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


int is_str_op(struct test_op_s *op)
{
    switch(op->op)
    {
        case TEST_STR_GT :
        case TEST_STR_LT :
        case TEST_STR_EQ1:
        case TEST_STR_EQ2:
        case TEST_STR_NE :
            return 1;
    }
    return 0;
}


/*
 * in the 'old' test command (invoked as either 'test' or '[', but not '[['), these operators
 * work differently than when the 'new' test command (invoked as '[[') is used:
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
                        if(oldtest) return TEST_OP_AND;
                        /* fall through */
                        
                    case 'e':
                        return TEST_OP_FILE_EXIST;
                    
                    case 'h':
                    case 'L':
                        return TEST_OP_FILE_LINK;
                    
                    case 'b': return TEST_OP_FILE_BLK;
                    case 'c': return TEST_OP_FILE_CHAR;
                    case 'd': return TEST_OP_FILE_DIR;
                    case 'f': return TEST_OP_FILE_REG;
                    case 'g': return TEST_OP_FILE_SGID;
                    case 'G': return TEST_OP_FILE_GOWN;
                    case 'k': return TEST_OP_FILE_STICKY;
                    case 'n': return TEST_OP_STR_NZ;
                    case 'N': return TEST_OP_FILE_NEW;
                    
                    case 'o':
                        if(oldtest) return TEST_OP_OR;
                        return TEST_OP_OPT_EN;
                        
                    case 'O': return TEST_OP_FILE_UOWN;
                    case 'p': return TEST_OP_FILE_PIPE;
                    case 'r': return TEST_OP_FILE_R;
                    //case 'R': return TEST_OP_VAR_REF;
                    case 's': return TEST_OP_FILE_SIZE;
                    case 'S': return TEST_OP_FILE_SOCK;
                    case 't': return TEST_OP_FILE_TERM;
                    case 'u': return TEST_OP_FILE_SUID;
                    case 'w': return TEST_OP_FILE_W;
                    case 'x': return TEST_OP_FILE_EXE;
                    case 'v': return TEST_OP_VAR_DEF;
                    case 'z': return TEST_OP_STR_ZERO;
                }
            }
            else
            {
                if(strcmp(expr, "-ef") == 0) return TEST_OP_FILE_EF;
                if(strcmp(expr, "-nt") == 0) return TEST_OP_FILE_NT;
                if(strcmp(expr, "-ot") == 0) return TEST_OP_FILE_OT;
                if(strcmp(expr, "-eq") == 0) return TEST_OP_EQ;
                if(strcmp(expr, "-ne") == 0) return TEST_OP_NE;
                if(strcmp(expr, "-lt") == 0) return TEST_OP_LT;
                if(strcmp(expr, "-le") == 0) return TEST_OP_LE;
                if(strcmp(expr, "-gt") == 0) return TEST_OP_GT;
                if(strcmp(expr, "-ge") == 0) return TEST_OP_GE;
            }
            return NULL;
            
        case '>': return TEST_OP_STR_GT;
        case '<': return TEST_OP_STR_LT;

        case '!':
            if(expr[1] == '=') return TEST_OP_STR_NE;
            return TEST_OP_NOT;

        case '=':
            if(expr[1] == '=') return TEST_OP_STR_EQ1;
            return TEST_OP_STR_EQ2;
            
        case '&':
            if(expr[1] == '&') return TEST_OP_AND;
            break;
            
        case '|':
            if(expr[1] == '|') return TEST_OP_OR;
            break;
        
        case '(': return TEST_OP_LBRACE;
        case ')': return TEST_OP_RBRACE;
    }
    return NULL;
}

void test_push_opstack(struct test_op_s *op)
{
    if(test_nopstack > MAXOPSTACK-1)
    {
        fprintf(stderr, "%s: Operator stack overflow\n", SHELL_NAME);
        test_err = 1;
        return;
    }
    test_opstack[test_nopstack++] = op;
}

struct test_op_s *test_pop_opstack()
{
    if(!test_nopstack)
    {
        fprintf(stderr, "%s: Operator stack empty\n", SHELL_NAME);
        test_err = 1;
        return NULL;
    }
    return test_opstack[--test_nopstack];
}

void test_push_stack(char *val)
{
    if(!val)
    {
        test_err = 1;
        return;
    }

    if(nteststack > MAXTESTSTACK-1)
    {
        fprintf(stderr, "%s: Test stack overflow\n", SHELL_NAME);
        test_err = 1;
        return;
    }
    teststack[nteststack++] = val;
}

char *test_pop_stack()
{
    if(!nteststack)
    {
        fprintf(stderr, "%s: Test stack empty\n", SHELL_NAME);
        test_err = 1;
        return test_empty_str;
    }
    return teststack[--nteststack];
}


void test_shunt_op(struct test_op_s *op)
{
    struct test_op_s *pop;
    test_err = 0;
    if(op->op == '(')
    {
        test_push_opstack(op);
        return;
    }
    else if(op->op == ')')
    {
        while(test_nopstack > 0 && test_opstack[test_nopstack-1]->op != '(')
        {
            pop = test_pop_opstack();
            if(test_err) return;
            char *n1 = test_pop_stack();
            if(test_err) return;
            if(pop->unary) test_push_stack(pop->test(n1, 0));
            else
            {
                char *n2 = test_pop_stack();
                if(test_err) return;
                test_push_stack(pop->test(n2, n1));
                if(test_err) return;
            }
        }
        if(!(pop = test_pop_opstack()) || pop->op != '(')
        {
            fprintf(stderr, "%s: Stack test_err. No matching \'(\'\n", SHELL_NAME);
            test_err = 1;
        }
        return;
    }

    if(op->assoc == ASSOC_RIGHT)
    {
        while(test_nopstack && op->prec < test_opstack[test_nopstack-1]->prec)
        {
            pop = test_pop_opstack();
            if(test_err) return;
            char *n1 = test_pop_stack();
            if(test_err) return;
            if(pop->unary) test_push_stack(pop->test(n1, 0));
            else
            {
                char *n2 = test_pop_stack();
                if(test_err) return;
                test_push_stack(pop->test(n2, n1));
            }
            if(test_err) return;
        }
    }
    else
    {
        while(test_nopstack && op->prec <= test_opstack[test_nopstack-1]->prec)
        {
            pop = test_pop_opstack();
            if(test_err) return;
            char *n1 = test_pop_stack();
            if(test_err) return;
            if(pop->unary) test_push_stack(pop->test(n1, 0));
            else
            {
                char *n2 = test_pop_stack();
                if(test_err) return;
                test_push_stack(pop->test(n2, n1));
            }
            if(test_err) return;
        }
    }
    test_push_opstack(op);
}


/*
 * the test (or '[') builtin utility.
 */

int test(int argc, char **argv)
{
    char   *expr;
    char   *tstart       = NULL;
    struct  test_op_s startop = { 'X', 0, ASSOC_NONE, 0, 0, NULL };  /* Dummy operator to mark start */
    struct  test_op_s *op     = NULL;
    int     i = 1;
    int     oldtest = 1;
    struct  test_op_s *lastop = &startop;
    test_nopstack = 0; nteststack = 0; test_err = 0;

    /* the '[' and '[[' versions of test need closing ']' and ']]' respectively */
    if(strcmp(argv[0], "[") == 0)
    {
        if(strcmp(argv[argc-1], "]"))
        {
            fprintf(stderr, "%s: missing closing bracket: ']'\r\n", UTILITY);
            return 2;
        }
        argc--;
    }
    else if(strcmp(argv[0], "[[") == 0)
    {
        if(strcmp(argv[argc-1], "]]"))
        {
            fprintf(stderr, "%s: missing closing bracket: ']]'\r\n", UTILITY);
            return 2;
        }
        argc--;
        oldtest = 0;
    }
    
    for( ; i < argc; i++)
    {
        expr = argv[i];
        while(*expr && isspace(*expr)) expr++;
        if(!tstart)
        {
            if((op = test_getop(expr, oldtest)))
            {
                if(lastop && (lastop == &startop || lastop->op != ')'))
                {
                    if(op->op != '(' && !op->unary)
                    {
                        if(is_str_op(op)) test_push_stack(test_empty_str);
                        else
                        {
                            fprintf(stderr, "%s: illegal use of binary operator (%c)\n", SHELL_NAME, op->op);
                            return 2;
                        }
                    }
                }
                test_err = 0;
                test_shunt_op(op);
                if(test_err) return 2;
                lastop = op;
            }
            else tstart = expr;
        }
        else
        {
            if((op = test_getop(expr, oldtest)))
            {
                test_err = 0;
                test_push_stack(tstart);
                if(test_err) return 2;
                tstart = NULL;
                test_shunt_op(op);
                if(test_err) return 2;
                lastop = op;
            }
            else
            {
                test_err = 0;
                test_push_stack(tstart);
                if(test_err) return 2;
                tstart = NULL;
                lastop = NULL;
            }
        }
    }

    if(tstart)
    {
        test_err = 0;
        if(i == 2)
        {
            test_shunt_op(TEST_OP_STR_NZ);
            if(test_err) return 2;
        }
        test_push_stack(tstart);
        if(test_err) return 2;
    }

    while(test_nopstack)
    {
        test_err = 0;
        op = test_pop_opstack();
        if(test_err) return 2;
        char *n1 = test_pop_stack();
        if(test_err) return 2;
        if(op->unary) test_push_stack(op->test(n1, 0));
        else
        {
            char *n2 = test_pop_stack();
            if(test_err) return 2;
            test_push_stack(op->test(n2, n1));
        }
        if(test_err) return 2;
    }

    if(nteststack != 1)
    {
        fprintf(stderr, "%s: Test stack has %d elements after evaluation. Should be 1.\n", SHELL_NAME, nteststack);
        return 2;
    }

    return strcmp(teststack[0], "0") == 0 ? 0 : 1;
}
