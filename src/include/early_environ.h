/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: early_environ.h
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

#include <stddef.h>     /* size_t */

#include "cpu.h"
#include "ostype.h"
#include "comptype.h"

struct early_env_item_s
{
  char *name ;
  char *value;
};

/*
 * Some indices into the following array of structs. useful when accessing
 * array members from initsh().
 */
#define INDEX_COLUMNS       0
#define INDEX_EGID          3
#define INDEX_EUID          4
#define INDEX_GID           7
#define INDEX_HISTFILE      8
#define INDEX_HISTSIZE      10
#define INDEX_HISTCONTROL   11
#define INDEX_HOME          12
#define INDEX_HOST          13
#define INDEX_HOSTNAME      14
#define INDEX_LINES         17
#define INDEX_LOGNAME       18
#define INDEX_OLDPWD        21
#define INDEX_PATH          26
#define INDEX_PS1           28
#define INDEX_PS2           29
#define INDEX_PS3           30
#define INDEX_PS4           31
#define INDEX_PWD           32
#define INDEX_SHELL         33
#define INDEX_USER          34
#define INDEX_USERNAME      35
#define INDEX_UID           36
/* everything after this one will be readonly -- just out of laziness */
#define INDEX_MACHTYPE      39
// #define INDEX_COLUMNS   5
// #define INDEX_EGID      10
// #define INDEX_EUID      11
// #define INDEX_GID       18
// #define INDEX_HOME      22
// #define INDEX_HOST      23
// #define INDEX_LINES     29
// #define INDEX_LOGNAME   31
// #define INDEX_OLDPWD    55
// #define INDEX_PATH      60
// #define INDEX_PS1       65
// #define INDEX_PS2       66
// #define INDEX_PS3       67
// #define INDEX_PS4       68
// #define INDEX_PWD       69
// #define INDEX_USER      78
// #define INDEX_USERNAME  79
// #define INDEX_UID       80

extern char shell_ver[];

struct early_env_item_s early_environ[] =
{
  { "COLUMNS"     , ""                      },
  { "EDITOR"      , "vi"                    },
  { "ENV"         , "$HOME/.lshrc"          },
  { "EGID"        , ""                      },
  { "EUID"        , ""                      },
  { "FC"          , ""                      },
  { "FCEDIT"      , "vi"                    },
  { "GID"         , ""                      },
  { "HISTFILE"    , ""                      },
  { "HISTORY"     , ""                      },
  { "HISTSIZE"    , ""                      },
  { "HISTCONTROL" , "ignoredups"            },
  { "HOME"        , ""                      },   /* should be set by the login utility, not us */
  { "HOST"        , "localhost"             },
  { "HOSTNAME"    , "localhost"             },
  { "IFS"         , " \t\n"                 },   /* default POSIX field-splitting list */
  /* current line # within a script or a function, starting with 1 */
  { "LINENO"      , ""                      },
  { "LINES"       , ""                      },
  { "LOGNAME"     , ""                      },
  { "LSH_VERSION" , shell_ver               },
  { "MAILCHECK"   , "600"                   },
  { "OLDPWD"      , ""                      },
  { "OPTARG"      , ""                      },
  { "OPTERR"      , ""                      },
  { "OPTIND"      , ""                      },
  { "OPTSUB"      , ""                      },
  //{ "PATH"       , "/media/cdrom/bin:/bin" },
  { "PATH"        , "/bin:/usr/bin:/sbin:/usr/sbin" },
  /* parent process ID during shell initialization */
  { "PPID"        , ""                      },
  /* parsed and printed to STDERR every time a new prompt is due to be printed */
  { "PS1"         , "[\\# \\u \\W]\\$ "       },
  /* printed to STDERR whenever user presses ENTER before completing a command */
  { "PS2"         , "> "                    },
  { "PS3"         , "#? "                   },
  /* printed to STDERR when execution trace 'set -x' is on */
  { "PS4"         , "+ "                    },
  { "PWD"         , ""                      },
  { "SHELL"       , ""                      },	/* pathname to shell */
  { "USER"        , ""                      },
  { "USERNAME"    , ""                      },
  { "UID"         , ""                      },
  { "VISUAL"      , "vi"                    },
  { "NULLCMD"     , "cat"                   },
  
  /*
   * TODO: $MACHTYPE should be in the standard gnu cpu-company-system format
   * 
   * NOTE: All of the following variables will be given the FLAG_READONLY flag in
   *       initsh(). If you want to add another variable but not give it the 
   *       readonly flag, please do add it above this comment.
   */
  { "MACHTYPE"    , CPU_ARCH                },  
  { "HOSTTYPE"    , CPU_ARCH                },       /* for compatibility with bash */
  { "OSTYPE"      , OS_TYPE                 },
  { "COMPILERTYPE", COMPILER_TYPE           },
  { "COMPILERBUILD", COMPILER_BUILD         },
};

size_t early_environ_length = sizeof(early_environ)/sizeof(struct early_env_item_s);
