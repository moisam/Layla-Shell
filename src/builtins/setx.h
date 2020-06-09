/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019, 2020 (c)
 * 
 *    file: setx.h
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

#ifndef LSH_SETX
#define LSH_SETX

#include <sys/types.h>

extern __int64_t optionsx;

#define OPTION_INTERACTIVE_COMMENTS     0x01l           /* (1 << 0 ) */
#define OPTION_LAST_PIPE                0x02l           /* (1 << 1 ) */
#define OPTION_NOCASE_MATCH             0x04l           /* (1 << 2 ) */
#define OPTION_NULL_GLOB                0x08l           /* (1 << 3 ) */
#define OPTION_FAIL_GLOB                0x10l           /* (1 << 4 ) */
#define OPTION_NOCASE_GLOB              0x20l           /* (1 << 5 ) */
#define OPTION_DOT_GLOB                 0x40l           /* (1 << 6 ) */
#define OPTION_EXT_GLOB                 0x80l           /* (1 << 7 ) */
#define OPTION_HUP_ON_EXIT              0x100l          /* (1 << 8 ) */
#define OPTION_EXEC_FAIL                0x200l          /* (1 << 9 ) */
#define OPTION_AUTO_CD                  0x400l          /* (1 << 10) */
#define OPTION_CDABLE_VARS              0x800l          /* (1 << 11) */
#define OPTION_CHECK_HASH               0x1000l         /* (1 << 12) */
#define OPTION_CHECK_JOBS               0x2000l         /* (1 << 13) */
#define OPTION_CHECK_WINSIZE            0x4000l         /* (1 << 14) */
#define OPTION_CMD_HIST                 0x8000l         /* (1 << 15) */
#define OPTION_LIT_HIST                 0x10000l        /* (1 << 16) */
#define OPTION_COMPLETE_FULL_QUOTE      0x20000l        /* (1 << 17) */
#define OPTION_EXPAND_ALIASES           0x40000l        /* (1 << 18) */
#define OPTION_FORCE_FIGNORE            0x80000l        /* (1 << 19) */
#define OPTION_GLOB_ASCII_RANGES        0x100000l       /* (1 << 20) */
#define OPTION_HIST_APPEND              0x200000l       /* (1 << 21) */
#define OPTION_HOST_COMPLETE            0x400000l       /* (1 << 22) */
#define OPTION_INHERIT_ERREXIT          0x800000l       /* (1 << 23) */
#define OPTION_LOCAL_VAR_INHERIT        0x1000000l      /* (1 << 24) */
#define OPTION_LOCAL_VAR_UNSET          0x2000000l      /* (1 << 25) */
#define OPTION_LOGIN_SHELL              0x4000000l      /* (1 << 26) */
#define OPTION_MAIL_WARN                0x8000000l      /* (1 << 27) */
#define OPTION_PROMPT_VARS              0x10000000l     /* (1 << 28) */
#define OPTION_RESTRICTED_SHELL         0x20000000l     /* (1 << 29) */
#define OPTION_SHIFT_VERBOSE            0x40000000l     /* (1 << 30) */
#define OPTION_SOURCE_PATH              0x80000000l     /* (1 << 31) */
#define OPTION_XPG_ECHO                 0x100000000l    /* (1 << 32) */
#define OPTION_USER_COMPLETE            0x200000000l    /* (1 << 33) */
#define OPTION_CLEAR_SCREEN             0x400000000l    /* (1 << 34) */
#define OPTION_HIST_RE_EDIT             0x800000000l    /* (1 << 35) */
#define OPTION_HIST_VERIFY              0x1000000000l   /* (1 << 36) */
#define OPTION_LIST_JOBS_LONG           0x2000000000l   /* (1 << 37) -- tcsh-like extension */
#define OPTION_LIST_JOBS                0x4000000000l   /* (1 << 38) -- tcsh-like extension */
#define OPTION_PUSHD_TO_HOME            0x8000000000l   /* (1 << 39) -- tcsh-like extension */
#define OPTION_DUNIQUE                  0x10000000000l  /* (1 << 40) -- tcsh-like extension */
#define OPTION_DEXTRACT                 0x20000000000l  /* (1 << 41) -- tcsh-like extension */
#define OPTION_ADD_SUFFIX               0x40000000000l  /* (1 << 42) -- tcsh-like extension */
#define OPTION_PRINT_EXIT_VALUE         0x80000000000l  /* (1 << 43) -- tcsh-like extension */
#define OPTION_RECOGNIZE_ONLY_EXE       0x100000000000l /* (1 << 44) -- tcsh-like extension */
#define OPTION_SAVE_DIRS                0x200000000000l /* (1 << 45) -- tcsh-like extension */
#define OPTION_SAVE_HIST                0x400000000000l /* (1 << 46) -- tcsh-like extension */
#define OPTION_PROMPT_BANG              0x800000000000l /* (1 << 47) -- zsh-like extension */
#define OPTION_PROMPT_PERCENT           0x1000000000000l/* (1 << 48) -- zsh-like extension */
#define OPTION_CALLER_VERBOSE           0x2000000000000l/* (1 << 49) */

#define optionx_set(o)                  ((((optionsx) & (o)) == (o)) ? 1 : 0)

int       set_optionx(__int64_t op, int onoff);
__int64_t optionx_index(char *opname);
int       setx_builtin(int argc, char **argv);
void      purge_xoptions(char which, int formal);
void      disable_extended_options(void);

/* for compatibility with bash */
int       shopt_builtin(int argc, char **argv);

#endif
