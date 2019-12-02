/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: builtins.c
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

#include "../cmd.h"
#include "../backend/backend.h"
#include "setx.h"

/*
 * in this file, we define the builtin utility, which is used to display the list
 * of special and regular builtin utilities, as well as to run a builtin utility,
 * even if an external command or shell function exists with the same name.
 * we store special and regular builtins information in separate lists. each item
 * in the lists contain the utility's name (and its length), a short description
 * on what the utility does, and the function we should call when the utility is invoked.
 * additionally, and to assist the 'help' utility in printing useful help messages for
 * each utility, we store the synopsis (how to use or invoke the utility), and the number
 * of times the utility's name appears in the synopsis (so that 'help' can call printf()
 * with the right number of arguments). we then have the help message that 'help' prints
 * for the utility, and the utility's flags (is the utility enabled or not, does 'help'
 * print the default help message for the -v and -h options or not, ...).
 * 
 * see the definition of 'struct builtin_s' in ../cmd.h.
 */

/* POSIX Shell & Utilities volume (section 1.6): regular builtin utilities */

/*
 * TODO: those utilities should be implemented such that they can be invoked
 *       through exec() or directly by env, ...
 */

struct builtin_s regular_builtins[] =
{
    {
        "["      , 1, "test file attributes and compare strings",
        test_builtin, 1,       /* POSIX */
        "Usage: %s -option expression ]",
        "expression  conditional expression to test\n\n"
        "For the list of options and their meanings, run 'help [['\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "[["     , 2, "test file attributes and compare strings",
        test_builtin, 9,       /* POSIX */
        "Usage: %s -abcdefgGhkLNOprsSuwx file ]]\n"
        "       %s [-nz] string ]]\n"
        "       %s -o [?]op ]]\n"
        "       %s -t fd ]]\n"
        "       %s file1 op file2 ]]\n"
        "       %s expr1 -op expr2 ]]\n"
        "       %s !expr ]]\n"
        "       %s expr1 && expr2 ]]\n"
        "       %s expr1 || expr2 ]]",
        "file        file name or path\n"
        "string      character string to be tested. If string is supplied without \n"
        "              option, the result is true if it is not null, false otherwise\n"
        "op          single character or multi-character option name\n"
        "fd          open file descriptor\n\n"
        "File operators are:\n"
        "  -a        return true if file exists\n"
        "  -b        return true if file exists and is a block device\n"
        "  -c        return true if file exists and is a character device\n"
        "  -d        return true if file exists and is a directory\n"
        "  -e        similar to -a\n"
        "  -f        return true if file exists and is a regular file\n"
        "  -g        return true if file exists and its setgid bit is set\n"
        "  -G        return true if file exists and its gid matches egid of this \n"
        "              process\n"
        "  -h        return true if file exists and is a symbolic link\n"
        "  -k        return true if file exists and its sticky bit is set\n"
        "  -L        similar to -h\n"
        "  -N        return true if file exists and its mtime > atime\n"
        "  -O        return true if file exists and is owned by this process's euid\n"
        "  -p        return true if file exists and is a pipe or FIFO\n"
        "  -r        return true if file exists and is readable by this process\n"
        "  -s        return true if file exists and its size > 0\n"
        "  -S        return true if file exists and is a socket\n"
        "  -u        return true if file exists and its setuid bit is set\n"
        "  -w        return true if file exists and is writeable by this process\n"
        "  -x        return true if file exists and is executable by this process\n\n"
        "String length operators are:\n"
        "  -n        return true if string's length is non-zero\n"
        "  -z        return true if string's length is zero\n\n"
        "Option operators are:\n"
        "  -o op     return true if option op is set\n"
        "  -o ?op    return true if op is a valid option name\n\n"
        "File descriptor operators are:\n"
        "  -t        return true if fd is an open file descriptor and is associated\n"
        "              with a terminal device\n\n"
        "Comparison operators are:\n"
        "  file1 -ef file2    true if file1 and file2 exist and refer to the same file\n"
        "  file1 -nt file2    true if file1 exists and file2 doesn't, or file1 is newer\n"
        "                       than file2\n"
        "  file1 -ot file2    true if file2 exists and file1 doesn't, or file1 is older\n"
        "                       than file2\n"
        "  exp1 -eq exp2      true if exp1 is equal to exp2\n"
        "  exp1 -ge exp2      true if exp1 is greater than or equal to exp2\n"
        "  exp1 -gt exp2      true if exp1 is greater than exp2\n"
        "  exp1 -le exp2      true if exp1 is less than or equal to exp2\n"
        "  exp1 -lt exp2      true if exp1 is less than exp2\n"
        "  exp1 -ne exp2      true if exp1 is not equal to exp2\n"
        "  string == pattern  true if string matches pattern\n"
        "  string = pattern   similar to the above operator\n"
        "  string != pattern  true if string does not match pattern\n"
        "  string1 < string2  true if string1 comes before string2 based on ASCII value \n"
        "                       of their characters\n"
        "  string1 > string2  true if string1 comes after string2 based on ASCII value \n"
        "                       of their characters\n\n"
        "Other operators:\n"
        "  -v var             return true if var is a set shell variable (one with \n"
        "                       assigned value)\n"
        "  !expression        true if expression is false\n"
        "  expr1 && expr2     true if expr1 and expr2 are both true\n"
        "  expr1 || expr2     true if either expr1 or expr2 is true\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    { 
        "alias"  , 5, "define or display aliases",
        alias_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] [alias-name[=string] ...]",
        "alias-name    write alias definition to standard output\n"
        "alias-name=string\n"
        "              assign the value of string to alias-name\n\n"
        "Options:\n"
        "  -p        print all defined aliases and their values\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "bg"     , 2, "run jobs in the background",
        bg_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] [job_id...]",
        "job_id      specify the job to run as background job\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "bugreport", 9, "send bugreports to the shell's author(s)",
        bugreport_builtin, 1,       /* non-POSIX */
        "Usage: %s",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "builtin" , 7, "print the list of shell builtin utilities",
        builtin_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hvsra] [name [args]]",
        "name       the name of a shell builtin utility to invoke\n"
        "args       arguments to pass to the builtin utility\n\n"
        "Options:\n"
        "  -a        list both special and regular builtins\n"
        "  -r        list shell regular builtins only\n"
        "  -s        list shell special builtins only\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "caller"  , 6, "print the context of any active subroutine call",
        caller_builtin, 1,       /* non-POSIX */
        "Usage: %s [n]",
        "n          non-negative integer denoting one of the callframe in the current\n"
        "           call stack. The current frame is 0. Each call to a function or dot\n"
        "           script results in a new entry added to the call stack.\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "cd"     , 2, "change the working directory",
        cd_builtin, 2,       /* POSIX */
        "Usage: %s [-h] [-nplv] [-L|-P] [directory]\n"
        "       %s [-h] [-nplv] -",
        "directory   Directory path to go to\n\n"
        "Options:\n"
        "  -L        logically handle dot-dot\n"
        "  -P        physically handle dot-dot\n"
        "  -l|n|p|v  these options have the same meaning as when used with the dirs\n"
        "              builtin. They all imply -p\n",
        BUILTIN_PRINT_HOPTION  | BUILTIN_ENABLED,  /* print only the -h option (suppress printing the -v option) */
    },
    {
        "command", 7, "execute a simple command",
        command_builtin, 2,       /* POSIX */
        "Usage: %s [-hp] command_name [argument ...]\n"
        "       %s [-hp][-v|-V] command_name",
        "commmand    command to be executed\n\n"
        "Options:\n"
        "  -p        search command using a default value for PATH\n"
        "  -v        show the command (or pathname) to be used by the shell\n"
        "  -V        show how the shell will interpret 'command'\n",
        BUILTIN_PRINT_HOPTION  | BUILTIN_ENABLED,  /* print only the -h option (suppress printing the -v option) */
    },
    {
        "coproc"  , 6, "execute commands in a coprocess (subshell with pipe)",
        coproc_builtin, 1,       /* non-POSIX */
        "Usage: %s command [redirections]",
        "command       the command to be executed in the subshell.\n"
        "redirections  optional file redirections. A pipe is opened between the shell and the\n"
        "                coprocess before any redirections are performed. Shell variable $COPROC_PID contains\n"
        "                the PID of the coprocess. Shell variable $COPROC0 points to the reading end of the\n"
        "                pipe (connected to command's stdout), while variable $COPROC1 points to the writing\n"
        "                end of the pipe (connected to command's stdin).\n\n"
        "              you can feed output to the process by invoking:\n\n"
        "                $ cmd >&p\n\n"
        "              similarly, you can read the process's output by invoking:\n\n"
        "                $ cmd <&p\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "declare" , 7, "declare variables and give them attributes",
        declare_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hvfFgrxlut] [-p] [name=[value]...]",
        "name        variable to which an attribute or value is set\n"
        "value       the value to give to the variable called name\n\n"
        "Options:\n"
        "  -f        restrict output to shell functions\n"
        "  -F        don't print function definitions\n"
        "  -g        declare/modify variables at the global scope\n"
        "  -l        all characters in variable's value are converted to lowercase on assignment\n"
        "  -p        print the attributes and values of each name\n"
        "  -r        mark each name as readonly\n"
        "  -t        give functions the trace attribute (doesn't work on variables)\n"
        "  -u        all characters in variable's value are converted to uppercase on assignment\n"
        "  -x        mark each name as export\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "dirs"    , 4, "display the contents of the directory stack",
        dirs_builtin, 2,       /* non-POSIX */
        "Usage: %s [-hclpv] [+N | -N]\n"
        "       %s -S|-L [filename]",
        "+N          print the N-th directory from the top (the left side of the \n"
        "              printed list), counting from zero (which is the current working \n"
        "              directory)\n"
        "-N          print the N-th directory from the bottom (the right side of the \n"
        "              printed list), counting from zero (which is the first dir pushed \n"
        "              on the stack)\n"
        "filename    the file to save/load the directory stack to/from\n\n"
        "Options:\n"
        "  -c        clear the stack, i.e. remove all directories\n"
        "  -l        print full pathnames, don't use ~ to indicate the home directory\n"
        "  -L        load the directory stack from the given filename. If no filename is\n"
        "              supplied, use $DIRSFILE or default to ~/.lshdirs\n"
        "  -n        wrap entries before they reach edge of the screen\n"
        "  -p        print each directory on a separate line\n"
        "  -S        save the directory stack to the given filename. If no filename is\n"
        "              supplied, use $DIRSFILE or default to ~/.lshdirs\n"
        "  -v        print each directory with its index on a separate line\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "disown"  , 6, "not send HUP signal to jobs",
        disown_builtin, 1,       /* non-POSIX */
        "Usage: %s [-arsv] [-h] [job...]",
        "job        job ids of the jobs to prevent from receiving SIGHUP on exit\n\n"
        "Options:\n"
        "  -a        disown all jobs\n"
        "  -h        don't remove job from the jobs table, only mark it as disowned\n"
        "  -r        disown only running jobs\n"
        "  -s        disown only stopped jobs\n",
        BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,  /* print only the -v option */
    },
    {
        "dump"    , 4, "dump memory values of the passed arguments",
        dump_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hv] [argument ...]",
        "argument    can be one of the following:\n"
        "   symtab      will print the contents of the local symbol table\n"
        "   vars        will print out the shell variable list (similar to `declare -p`)\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "echo"    , 4, "echo arguments",
        echo_builtin, 1,       /* non-POSIX */
        "Usage: %s [-enE] [args...]",
        "args        strings to echo\n\n"
        "Options:\n"
        "  -e        allow escaped characters in arguments\n"
        "  -E        don't allow escaped characters in arguments\n"
        "  -n        suppress newline echoing\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "enable"  , 6, "enable/disable shell builtins",
        enable_builtin, 1,       /* non-POSIX */
        "Usage: %s [-ahnprsv] [name ...]",
        "name       the name of a shell builtin utility\n"
        "Options:\n"
        "  -a        print a list of all builtins, enabled and disabled\n"
        "  -n        disable each listed builtin\n"
        "  -p        print a list of enabled builtins\n"
        "  -r        print a list of enabled and disabled regular builtins\n"
        "  -s        print a list of enabled and disabled special builtins\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "false"  , 5, "return false value",
        false_builtin, 1,       /* POSIX */
        "Usage: %s",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "fc"     , 2, "process the command history list",
        fc_builtin, 3,       /* POSIX */
        "Usage: %s [-hvr] [-e editor] [first [last]]\n"
        "       %s -l [-hvnr] [first [last]]\n"
        "       %s -s [-hv] [old=new] [first]",
        "editor      editor to use in editing commands\n"
        "first,last  select commands to list or edit\n"
        "old=new     replace first occurence of old with new\n\n"
        "Options:\n"
        "  -e        specify the editor to use when editing commands\n"
        "  -l        list commands, don't invoke them\n"
        "  -n        suppress command numbers when listing\n"
        "  -r        reverse order of commands\n"
        "  -s        re-execute commands without invoking editor\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "fg"     , 2, "run jobs in the foreground",
        fg_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] [job_id]",
        "job_id      specify the job to run as foreground job\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "getopts", 7, "parse utility options",
        getopts_builtin, 1,       /* POSIX */
        "Usage: %s optstring name [arg...]",
        "optstring   string of option characters to be recognized\n"
        "name        shell variable to save in the found option\n"
        "arg...      list of arguments to parse instead of positional args\n",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "glob"   , 4, "echo arguments, delimited by NULL characters",
        glob_builtin, 1,       /* non-POSIX */
        "Usage: %s [-eE] [args...]",
        "args       strings to echo\n\n"
        "Options:\n"
        "  -e       allow escaped characters in arguments\n"
        "  -E       don't allow escaped characters in arguments\n",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "hash"   , 4, "remember or report utility locations",
        hash_builtin, 2,       /* POSIX */
        "Usage: %s [-hvld] [-p path] [-r] utility...\n"
        "       %s -a",
        "utility...  the name of a utility to search and add to the hashtable\n\n"
        "Options:\n"
        "  -a        forget, then re-search and re-hash all utilities whose names are\n"
        "              currently in the hashtable\n"
        "  -d        forget the location of each passed utility\n"
        "  -l        print the list of hashed utilities and their paths\n"
        "  -p        perform utility search using path instead of the $PATH variable\n"
        "  -r        forget all previously remembered utility locations\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "help"    , 4, "show help for builtin utilities and commands",
        help_builtin, 1,       /* non-POSIX */
        "Usage: %s [-ds] [command]",
        "command     the name of a builtin utility for which to print help\n\n"
        "Options:\n"
        "  -d        print a short description of each command\n"
        "  -s        print the usage or synopsis of each command\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "history" , 7, "print command history",
        history_builtin, 6,       /* non-POSIX */
        "Usage: %s [-hR] [n]\n"
        "       %s -c\n"
        "       %s -d offset\n"
        "       %s -d start-end\n"
        "       %s [-anrwSL] [filename]\n"
        "       %s -ps arg ...",
        "n            print only the last n lines\n\n"
        "Options:\n"
        "  -a         append the in-memory history list to filename. If filename is not\n"
        "               supplied, the default history file is used\n"
        "  -c         clear the history list\n"
        "  -d offset  delete history entry at position offset. Negative offsets count\n"
        "               from the end of the list; offset -1 is the last command entered\n"
        "  -d start-end\n"
        "             delete history entries between offsets start and end, which can be\n"
        "               negative, as described above\n"
        "  -h         print history entries without leading numbers\n"
        "  -L         equivalent to -r\n"
        "  -n         append the entries from filename to the in-memory list. If filename\n"
        "               is not supplied, the default history file is used\n"
        "  -p         perform history substitution on args and print the result on stdout\n"
        "  -r         read the history file and append the entries to the in-memory list\n"
        "  -R         reverse the listing order (most recent entries are printed first)\n"
        "  -s         add args to the end of the history list as one entry\n"
        "  -S         equivalent to -w\n"
        "  -w         write out the current in-memory list to filename. If filename is not\n"
        "               supplied, the default history file is used\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "hup"    , 3, "run a command, receiving SIGHUP",
        hup_builtin, 1,       /* non-POSIX */
        "Usage: %s [command]",
        "command     the command to run (must be an external command)\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "jobs"   , 4, "display status of jobs in the current session",
        jobs_builtin, 2,       /* POSIX */
        "Usage: %s [-hnrsv] [-l|-p] [job_id...]\n"
        "       %s -x command [argument...]",
        "job_id...      job ID(s) for which to display status\n"
        "command        command to run\n"
        "argument...    arguments to pass to command\n\n"
        "Options:\n"
        "  -l        provide more (long) information\n"
        "  -n        report only jobs that changed status since last notification\n"
        "  -p        display only process ID(s) of process group leaders\n"
        "  -r        report only running jobs\n"
        "  -s        report only stopped jobs\n"
        "  -x        replace all 'job_id's in 'command' and 'argument's with the\n"
        "              process group ID of the respective job, then run command, passing\n"
        "              it the given arguments\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "kill"   , 4, "terminate or signal processes",
        kill_builtin, 6,       /* POSIX */
        "Usage: %s [-hv]\n"
        "       %s -s signal_name pid...\n"
        "       %s -n signal_number pid...\n"
        "       %s [-l|-L] [exit_status]\n"
        "       %s [-signal_name] pid...\n"
        "       %s [-signal_number] pid...",
        "signal_name     symbolic name of the signal to send\n"
        "signal_number   non-negative number of the signal to send\n"
        "pid...          process ID or process group ID, or job ID number\n"
        "exit_status     signal number or exit status of a signaled process\n\n"
        "Options:\n"
        "  -l, -L    write values of all sig_names, or the sig_name associated with \n"
        "              the given exit_status (or sig_number)\n"
        "  -s        specity the symbolic name of the signal to send\n"
        "  -n        specity the signal number to send\n",        
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "let"    , 3, "evaluate arithmetic expressions",
        let_builtin, 1,       /* non-POSIX */
        "Usage: %s [args...]",
        "args        arithmetic expressions to evaluate\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "mailcheck", 9, "check for mail at specified intervals",
        mailcheck_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hvq]",
        "Options:\n"
        "  -q        do not output messages in case of error or no mail\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "memusage", 8, "show the shell's memory usage",
        memusage_builtin, 1,       /* non-POSIX */
        "Usage: %s arg...",
        "Arguments show the memory allocated for different shell internal structures:\n"
        "  aliases             show the memory allocated for alias names and values\n"
        "  cmdbuf, cmdbuffer   show the memory allocated for the command line buffer\n"
        "  dirstack            show the memory allocated for the directory stack\n"
        "  hash, hashtab       show the memory allocated for the commands hashtable\n"
        "  history             show the memory allocated for the command line history table\n"
        "  input               show the memory allocated for the currently executing translation unit\n"
        "  stack, symtabs      show the memory allocated for the symbol table stack\n"
        "  strbuf, strtab      show the memory allocated for the internal strings buffer\n"
        "  traps               show the memory allocated for the signal traps\n"
        "  vm                  show the memory usage of different segments (RSS, stack, data)\n\n"
        "Options:\n"
        "  -l        show long output (i.e. print more details)\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "newgrp" , 6, "change to a new group",
        newgrp_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] [-l] [group]",
        "group       group name (or ID) to which the real and effective group\n"
        "              IDs shall be set\n\n"
        "Options:\n"
        "  -l        change the environment to a login environment\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "nice"   , 4, "run a command with the given priority",
        nice_builtin, 2,       /* non-POSIX */
        "Usage: %s [+n] [command]\n"
        "       %s [-n] [command]",
        "+n          a positive nice priority to give to command, or the shell if no command\n"
        "              is given (the plus sign can be omitted)\n"
        "-n          a negative nice priority. only root can pass -ve nice values\n"
        "command     the command to run under priority n (must be an external command)\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "nohup"  , 5, "run a command, ignoring SIGHUP",
        hup_builtin, 1,       /* non-POSIX */
        "Usage: %s [command]",
        "command     the command to run (must be an external command)\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "notify" , 6, "notify immediately when jobs change status",
        notify_builtin, 1,       /* non-POSIX */
        "Usage: %s [job ...]",
        "job         the job id of the job to mark for immediate notification\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "popd"   , 4, "pop directories off the stack and cd to them",
        popd_builtin, 1,       /* non-POSIX */
        "Usage: %s [-chlnpsv] [+N | -N]",
        "+N          remove the N-th directory, counting from 0 from the left\n"
        "-N          remove the N-th directory, counting from 0 from the right\n\n"
        "If called without arguments, popd removes the top directory from the stack and calls \n"
        "cd to change the current working directory to the new top directory (equivalent to \n"
        "`popd +0`).\n\n"
        "Options:\n"
        "  -c        manipulate the stack, but don't cd to the directory\n"
        "  -s        don't output the dirstack after popping off it\n"
        "  -l|n|v|p  have the same meaning as for the dirs builtin (see `help dirs`)\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "printenv", 8, "print the names and values of environment variables",
        printenv_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hv0] [name ...]",
        "name        the name of an environment variable\n\n"
        "Options:\n"
        "  -0        terminate each entry with NULL instead of a newline character\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "pushd"  , 5, "push directories on the stack and cd to them",
        pushd_builtin, 1,       /* non-POSIX */
        "Usage: %s [-chlnpsv] [+N | -N | dir]",
        "+N          rotate the stack and bring the N-th directory, counting from 0 from the \n"
        "              left, to the top of the stack\n"
        "-N          rotate the stack and bring the N-th directory, counting from 0 from the \n"
        "              right, to the top of the stack\n"
        "dir         push dir on the stack and cd to it. If dir is dash '-', this equals the \n"
        "              previous working directory, as stored in $PWD\n\n"
        "Options:\n"
        "  -c        manipulate the stack, but don't cd to the directory\n"
        "  -s        don't output the dirstack after pushing the directory on it\n"
        "  -l|n|v|p  have the same meaning as for the dirs builtin (see `help dirs`)\n"
        "  -h        show utility help (this page)\n\n"
        "Notes:\n"
        "If called without arguments, pushd exchanges the top two directories on the stack and\n"
        "calls cd to change the current working directory to the new top directory.\n"
        "If the 'pushdtohome' extra option is set (by calling `setx -s pushdtohome`), pushd pushes\n"
        "the value of $HOME and cd's to it instead of exchanging the top two directories.\n"
        "If the 'dunique' extra option is set, pushd removes instances of dir from the stack\n"
        "before pushing it. If the 'dextract' extra option is set, pushd extracts the N-th directory\n"
        "and pushes it on top of the stack.\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "pwd"    , 3, "return working directory name",
        pwd_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] [-L|-P]",
        "Options:\n"
        "  -L        logically handle dot-dot\n"
        "  -P        physically handle dot-dot\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "read"   , 4, "read a line from standard input",
        read_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] [-rs] [-d delim] [-nN num] [-t secs] [-u fd] [-p msg] [var...]",
        "delim       read up to the first character of delim instead of a newline\n"
        "num         max number of bytes to read\n"
        "secs        timeout when reading from a terminal or pipe/fifo\n"
        "fd          file descriptor to use instead of stdin (0). fd should have \n"
        "              been open with an earlier invocation of exec\n"
        "var...      the name of shell variables to assign input to. If none is \n"
        "              supplied, environment variable $REPLY is used.\n"
        "msg         a string to be printed before reading input\n\n"
        "Options:\n"
        "  -d        read up to delim (instead of newline)\n"
        "  -n, -N    read a maximum of num bytes\n"
        "  -p        print argument msg before reading input\n"
        "  -s        save input as a new entry in the history file\n"
        "  -t        read fails if no input after secs seconds\n"
        "  -u        read fails if no input after secs seconds\n"
        "  -r        read from fd (instead of stdin)\n\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "setenv" , 6, "set environment variable values",
        setenv_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hv] [name[=value] ...]",
        "name        the environment variable to set\n"
        "value       the value to give to name, NULL if no value is given\n\n"
        "This utility sets both the environment variable and the shell variable with\n"
        "the same name. If no arguments are given, it prints the names and values of\n"
        "all the set environment variables.\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "stop"   , 4, "stop background jobs",
        stop_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hv] job",
        "job         the background job to stop\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "test"   , 4, "test file attributes and compare strings",
        test_builtin, 1,       /* POSIX */
        "Usage: %s -option expression",
        "For the list of options and their meanings, run 'help [['\n",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "true"   , 4, "return true value",
        true_builtin, 1,       /* POSIX */
        "Usage: %s",
        "",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "type"   , 4, "write a description of command type",
        type_builtin, 1,       /* POSIX */
        "Usage: %s name...",
        "command     the name of a command or function for which to write description\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,   /* print both -v and -h options */
    },
    {
        "ulimit" , 6, "set or report shell resource limits",
        ulimit_builtin, 1,       /* POSIX */
        "Usage: %s [-h] [-acdflmnpstuv] [limit]",
        "limit       the new limit for the given resource\n\n"
        "Options:\n"
        "  -a        report all current limits\n"
        "  -c        set/report the maximum size of core files created\n"
        "  -d        set/report the maximum size of a process's data segment\n"
        "  -e        set/report the maximum nice value (scheduling priority)\n"
        "  -f        set/report the maximum size of files written by a process\n"
        "  -i        set/report the maximum number of pending signals\n"
        "  -l        set/report the maximum size of memory a process may lock\n"
        "  -m        set/report the maximum resident set size (RSS)\n"
        "  -n        set/report the maximum number of open file descriptors\n"
        "  -p        set/report the pipe buffer size in kbytes\n"
        "  -q        set/report the maximum number of kbytes in POSIX message queues\n"
        "  -r        set/report the maximum real-time priority\n"
        "  -s        set/report the maximum stack size\n"
        "  -t        set/report the maximum amount of cpu time (seconds)\n"
        "  -u        set/report the maximum number of user processes\n"
        "  -v        set/report the size of virtual memory\n"
        "  -x        set/report the maximum number of file locks\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "umask"  , 5, "get or set the file mode creation mask",
        umask_builtin, 1,       /* POSIX */
        "Usage: %s [-hvp] [-S] [mask]",
        "mask        the new file mode creation mask\n\n"
        "Options:\n"
        "  -S        produce symbolic output\n"
        "  -p        print output that can be reused as shell input\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unalias", 7, "remove alias definitions",
        unalias_builtin, 2,       /* POSIX */
        "Usage: %s [-hv] alias-name...\n"
        "       %s [-hv] -a",
        "alias-name  the name of an alias to be removed\n\n"
        "Options:\n"
        "  -a        remove all alias definitions\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unlimit", 7, "remove limits on system resources",
        unlimit_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hHfSv] [limit ...]\n"
        "       %s [-HS] -a",
        "limit       the name of a system resource, which can be one of the following:\n"
        "  core    , -c        the maximum size of core files created\n"
        "  data    , -d        the maximum size of a process's data segment\n"
        "  nice    , -e        the maximum nice value (scheduling priority)\n"
        "  file    , -f        the maximum size of files written by a process\n"
        "  signal  , -i        the maximum number of pending signals\n"
        "  mlock   , -l        the maximum size of memory a process may lock\n"
        "  rss     , -m        the maximum resident set size (RSS)\n"
        "  fd      , -n        the maximum number of open file descriptors\n"
        "  buffer  , -p        the pipe buffer size in kbytes\n"
        "  message , -q        the maximum number of kbytes in POSIX message queues\n"
        "  rtprio  , -r        the maximum real-time priority\n"
        "  stack   , -s        the maximum stack size\n"
        "  cputime , -t        the maximum amount of cpu time (seconds)\n"
        "  userproc, -u        the maximum number of user processes\n"
        "  virtmem , -v        the size of virtual memory\n"
        "  flock   , -x        the maximum number of file locks\n"
        "  all     , -a        all the above\n\n"
        "Options and limit names must be passed separately. To remove all hard limits, invoke\n"
        "either of the following commands:\n"
        "  unlimit -H -a\n"
        "  unlimit -H all\n\n"
        "Options:\n"
        "  -a        remove limits on all resources\n"
        "  -f        ignore errors\n"
        "  -H        remove hard limits (only root can do this)\n"
        "  -S        remove soft limits (the default)\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unsetenv", 8, "unset environment variable values",
        unsetenv_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hv] [name ...]",
        "name        the environment variable to unset\n\n"
        "This utility unsets both the environment variable and the shell variable with\n"
        "the same name. If no arguments are given, nothing is done.\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "ver"     , 3, "show shell version",
        ver_builtin, 1,       /* non-POSIX */
        "Usage: %s",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "wait"   , 4, "await process completion",
        wait_builtin, 1,       /* POSIX */
        "Usage: %s [-hfnv] [pid...]",
        "pid...      process ID or Job ID to wait for\n\n"
        "Options:\n"
        "  -f        force jobs/processes to exit\n"
        "  -n        wait for any job or process\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "whence" , 6, "write a description of command type",
        whence_builtin, 1,       /* non-POSIX */
        "Usage: %s [-afhpv] name...",
        "name        the name of a command or function for which to write description\n\n"
        "Options:\n"
        "  -a        output all possible interpretations of the command\n"
        "  -f        don't search for functions\n"
        "  -p        perform path search even if command is an alias, keyword or function name\n"
        "  -v        verbose output (the default)\n",
        2 | BUILTIN_ENABLED,  /* print only the -h option */
    },
};
int regular_builtin_count = sizeof(regular_builtins)/sizeof(struct builtin_s);


/* POSIX section 2.14: special builtin utilities */
struct builtin_s special_builtins[] = 
{
    {
        "break"   , 5, "exit from for, while, or until loop",
        break_builtin, 1,       /* POSIX */
        "Usage: %s [n]",
        "n           exit the the n-th enclosing for, while, or until loop\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        ":"       , 1, "expand arguments (the null utility)",
        colon_builtin, 1,       /* POSIX */
        "Usage: %s [argument...]",
        "argument    command arguments to expand\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "continue", 8, "continue for, while, or until loop",
        continue_builtin, 1,       /* POSIX */
        "Usage: %s [n]",
        "n           return to the top of the n-th enclosing for, while, or until loop\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "."       , 1, "execute commands in the current environment",
        dot_builtin, 1,       /* POSIX */
        "Usage: %s file",
        "file        execute commands from this file in the current environment\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "eval"    , 4, "construct command by concatenating arguments",
        eval_builtin, 1,       /* POSIX */
        "Usage: %s [argument...]",
        "argument    construct a command by concatenating arguments together\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "exec"    , 4, "execute commands and open, close, or copy file descriptors",
        exec_builtin, 1,       /* POSIX */
        "Usage: %s [-cl] [-a name] [command [argument...]]",
        "command     path to the command to be executed\n"
        "argument    execute command with arguments and open, close, or copy file descriptors\n\n"
        "Options:\n"
        "  -a        set argv[0] to 'name' instead of 'command'\n"
        "  -c        clear the environment before performing exec\n"
        "  -l        place a dash in front of argv[0], just as the login utility does\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "exit"    , 4, "exit the shell",
        exit_builtin, 1,       /* POSIX */
        "Usage: %s [n]",
        "n           exit the shell returning n as the exit status code\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "export"  , 6, "set the export attribute for variables",
        export_builtin, 1,       /* POSIX */
        "Usage: %s [-hvn] [-p] [name[=word]...]",
        "name        set the export attribute to the variable name\n"
        "word        set the value of variable name to word\n\n"
        "Options:\n"
        "  -n        remove the export attribute of passed variable names\n"
        "  -p        print the names and values of all exported variables\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "local"   , 5, "define local variables",
        local_builtin, 1,       /* non-POSIX */
        "Usage: %s name[=word] ...",
        "name        set the local attribute to the variable 'name'\n"
        "word        set the value of the variable named 'name' to 'word'\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "logout"  , 6, "exit a login shell",
        logout_builtin, 1,       /* non-POSIX */
        "Usage: %s [n]",
        "n           exit a login shell returning n as the exit status code\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "readonly", 8, "set the readonly attribute for variables",
        readonly_builtin, 2,       /* POSIX */
        "Usage: %s name[=word]...\n"
        "       %s -p",
        "name        set the readonly attribute to the variable name\n"
        "word        set the value of variable name to word\n\n"
        "Options:\n"
        "  -p        print the names and values of all readonly variables\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "repeat"  , 6, "repeat a command count times",
        repeat_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hv] count command",
        "count       the number of times to repeat command\n"
        "command     the command to execute count times\n\n"
        "Options:\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "return"  , 6, "return from a function or dot script",
        return_builtin, 1,       /* POSIX */
        "Usage: %s [n]",
        "n           exit status to return\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "set"     , 3, "set or unset options and positional parameters",
        set_builtin, 5,       /* POSIX */
        "Usage: %s [-abCdeEfhHkmnprtTuvx] [-o option ...] [argument...]\n"
        "       %s [+abCdeEfhHkmnprtTuvx] [+o option ...] [argument...]\n"
        "       %s -- [argument...]\n"
        "       %s -o\n"
        "       %s +o",
        "--           used to delimit arguments if the first argument begins with '+' or '-',\n"
        "argument     values to set positional parameters to\n\n"
        "Options (leading '-' enables options, leading '+' disables them):\n"
        "  --         end of options\n"
        "  -a         mark all variables with the export attribute\n"
        "  -b         asynchronous notification of background job completions\n"
        "  -B         perform brace expansion, so \"{a,b}\" expands to \"a\" \"b\"\n"
        "  -C         don't overwrite existing files when using '>' for redirection\n"
        "  -d         dump the parser's Abstract Syntax Tree (AST) before executing commands\n"
        "  -e         exit shell on error\n"
        "  -E         ERR traps are inherited by shell functions, command substitutions and subshells\n"
        "  -f         disable pathname expansion\n"
        "  -h         remember utility locations when they are first invoked\n"
        "  -H         enable history substitution\n"
        "  -k         place all variable assignments in command environment (ignored)\n"
        "  -m         enable/disable the job control feature\n"
        "  -n         read commands but don't execute them (non-interactive shells only)\n"
        "  -o         print current options string to stdout\n"
        "  +o         print current options string in a format suitable for reinput to the shell\n"
        "  -o option  extended format for setting/unsetting options. Argument option can be:\n"
        "     allexport       equivalent to -a\n"
        "     braceexpand     equivalent to -B\n"
        "     errexit         equivalent to -e\n"
        "     errtrace        equivalent to -E\n"
        "     functrace       equivalent to -T\n"
        "     hashall         equivalent to -h\n"
        "     hashexpand      equivalent to -H\n"
        "     history         equivalent to -w\n"
        "     ignoreeof       prevent interactive shells from exiting on EOF\n"
        "     keyword         equivalent to -k\n"
        "     monitor         equivalent to -m\n"
        "     noclobber       equivalent to -C\n"
        "     noglob          equivalent to -f\n"
        "     noexec          equivalent to -n\n"
        "     nolog           don't save function definitions to command history list (ignored)\n"
        "     notify          equivalent to -b\n"
        "     nounset         equivalent to -u\n"
        "     onecmd          equivalent to -t\n"
        "     pipefail        pipeline's exit status is that of the rightmost command to exit with \n"
        "                       non-zero status, or zero if all exited successfully\n"
        "     privileged      equivalent to -p\n"
        "     verbose         equivalent to -v\n"
        "     vi              allow command line editing using the builtin vi editor\n"
        "     xtrace          equivalent to -x\n"
        "  -p         turn on privileged mode. $ENV file is not processed. $CDPATH and $GLOBIGNORE are\n"
        "               ignored. If -p is not passed to the shell, and the effective uid (gid) is not\n"
        "               equal to the real uid (gid), effective ids are reset to their real values\n"
        "  -r         enable the restricted shell. This option cannot be unset once set\n"
        "  -t         exit the shell after executing one command\n"
        "  -T         DEBUG and RETURN traps are inherited by shell functions, command substitutions\n"
        "               and subshells\n"
        "  -u         expanding unset parameters (except $@ and $*) results in error\n"
        "  -v         verbose mode (write input to stderr as it is read)\n"
        "  -x         write command trace to stderr before executing each command\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "setx"    , 4, "set/unset optional (extra) shell options",
        setx_builtin, 1,       /* non-POSIX */
        "Usage: %s [-hvpsuqo] option",
        "option      can be any of the following (the name inside brackets is the shell from\n"
        "            which the option was taken/based; 'int' means interactive shell, 'non-int'\n"
        "            means non-interactive shell):\n"
        "addsuffix          append space to file- and slash to dir-names on tab completion (tcsh)\n"
        "autocd             dirs passed as single-word commands are passed to 'cd' (bash int)\n"
        "cdable_vars        cd arguments can be variable names (bash)\n"
        "cdable-vars        same as the above\n"
        "checkhash          for hashed commands, check the file exists before exec'ing (bash)\n"
        "checkjobs          list stopped/running jobs and warn user before exit (bash int)\n"
        "checkwinsize       check window size after external cmds, updating $LINES/$COLUMNS (bash)\n"
        "clearscreen        clear the screen on shell's startup\n"
        "cmdhist            save multi-line command in a single history entry (bash)\n"
        "complete_fullquote quote metacharacters in filenames during completion (bash)\n"
        "complete-fullquote same as the above\n"
        "dextract           pushd extracts the given dir instead of rotating the stack (tcsh)\n"
        "dotglob            files starting with '.' are included in filename expansion (bash)\n"
        "dunique            pushd removes similar entries before pushing dir on the stack (tcsh)\n"
        "execfail           failing to exec a file doesn't exit the shell (bash non-int)\n"
        "expand_aliases     perform alias expansion (bash)\n"
        "expand-aliases     same as the above\n"
        "extglob            enable ksh-like extended pattern matching (bash)\n"
        "failglob           failing to match filenames to patterns result in expansion error (bash)\n"
        "force_fignore      $FIGNORE determines which words to ignore on word expansion (bash)\n"
        "force-fignore      same as the above\n"
        "globasciiranges    bracket pattern matching expressions use the C locale (bash)\n"
        "histappend         append (don't overwrite) the history list to $HISTFILE (bash)\n"
        "histreedit         enable the user to re-redit a failed history substitution (bash int)\n"
        "histverify         reload (instead of directly execute) history substitution results (bash int)\n"
        "hostcomplete       perform hostname completion for words containing '@' (bash int)\n"
        "huponexit          send SIGHUP to all jobs on exit (bash int login)\n"
        "inherit_errexit    command substitution subshells inherit the -e option (bash)\n"
        "inherit-errexit    same as the above\n"
        "interactive_comments\n"
        "                   recognize '#' as the beginning of a comment (bash int)\n"
        "interactive-comments\n"
        "                   same as the above\n"
        "lastpipe           last cmd of foreground pipeline is run in the current shell (bash)\n"
        "lithist            save multi-line commands with embedded newlines (bash with 'cmdhist' on)\n"
        "listjobs           list jobs when a job changes status (tcsh)\n"
        "listjobs_long      list jobs (detailed) when a job changes status (tcsh)\n"
        "listjobs-long      same as the above\n"
        "localvar_inherit   local vars inherit value/attribs from previous scopes (bash)\n"
        "localvar-inherit   same as the above\n"
        "localvar_unset     allow unsetting local vars in previous scopes (bash)\n"
        "localvar-unset     same as the above\n"
        "login_shell        indicates a login shell (cannot be changed) (bash)\n"
        "login-shell        same as the above\n"
        "mailwarn           warn about mail files that have already been read (bash)\n"
        "nocaseglob         perform case-insensitive filename expansion (bash)\n"
        "nocasematch        perform case-insensitive pattern matching (bash)\n"
        "nullglob           patterns expanding to 0 filenames expand to "" (bash)\n"
        "printexitvalue     output non-zero exit status for external commands (tcsh)\n"
        "progcomp           enable programmable completion (not yet implemented) (bash)\n"
        "progcomp_alias     allow alias expansion in completions (not yet implemented) (bash)\n"
        "promptvars         perform word expansion on prompt strings (bash)\n"
        "pushdtohome        pushd without arguments pushed ~ on the stack (tcsh)\n"
        "recognize_only_executables\n"
        "                   only executables are recognized in command completion (tcsh)\n"
        "recognize-only-executables\n"
        "                   same as the above\n"
        "restricted_shell   indicates a restricted shell (cannot be changed) (bash)\n"
        "restricted-shell   same as the above\n"
        "savedirs           save the directory stack when login shell exits (tcsh)\n"
        "savehist           save the history list when shell exits (tcsh)\n"
        "shift_verbose      allow the shift builtin to output err messages (bash)\n"
        "shift-verbose      same as the above\n"
        "sourcepath         the source builtin uses $PATH to find files (bash)\n"
        "usercomplete       perform hostname completion for words starting with '~'\n"
        "xpg_echo           echo expands backslash escape sequences by default (bash)\n"
        "xpg-echo           same as the above\n\n"
        "Options:\n"
        "  -o        restrict options to those recognized by `set -o`\n"
        "  -p        print output that can be re-input to the shell\n"
        "  -q        suppress normal output. the return status tells whether options are set or not\n"
        "  -s        set (enable) each passed option\n"
        "  -u        unset (disable) each passed option\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "shift"   , 5, "shift positional parameters",
        shift_builtin, 1,       /* POSIX */
        "Usage: %s [n]",
        "n           the value by which to shift positional parameters to the left.\n"
        "            parameter 1 becomes (1+n), parameters 2 becomes (2+n), and so on\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "source"  , 6, "execute commands in the current environment",
        source_builtin, 1,       /* POSIX */
        "Usage: %s [-hv] file",
        "file        execute commands from this file in the current environment\n\n"
        "This command is the same as dot or '.', except when the -h option is given, where\n"
        "file is read and the commands are added to the history list, which is identical to\n"
        "invoking `history -L`.\n\n"
        "Options:\n"
        "  -h        read file and add commands to the history list\n",
        BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "suspend" , 7, "suspend execution of the shell",
        suspend_builtin, 1,       /* non-POSIX */
        "Usage: %s [-fhv]",
        "Options:\n"
        "  -f        force suspend, even if the shell is a login shell\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "times"   , 5, "write process times",
        times_builtin, 1,       /* POSIX */
        "Usage: %s",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "trap"    , 4, "trap signals",
        trap_builtin, 2,       /* POSIX */
        "Usage: %s [-hvlp] n [condition...]\n"
        "       %s [-hvlp] [action condition...]",
        "n           treat all operands as conditions; reset each condition to the default value\n\n"
        "action      can be either:\n"
        "   -        reset each condition to the default value\n"
        "   \"\"       (empty string) ignore each condition if it arises\n"
        "   any other value will be read and executed by the shell when one of the corresponding\n"
        "   conditions arises.\n\n"
        "condition   can be either:\n"
        "   EXIT     set/reset exit traps\n"
        "   ERR      set/reset error traps\n"
        "   DEBUG    set/reset debug traps (not yet implemented!)\n"
        "   name     signal name without the SIG prefix\n\n"
        "Options:\n"
        "  -l        list all conditions and their signal numbers\n"
        "  -p        print the trap actions associated with each condition\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unset"   , 5, "unset values and attributes of variables and functions",
        unset_builtin, 1,       /* POSIX */
        "Usage: %s [-fv] name...",
        "name       names of variables/functions to unset and remove from the environment.\n"
        "           readonly variables cannot be unset.\n\n"
        "Options:\n"
        "  -f       treat each name as a function name\n"
        "  -v       treat each name as a variable name\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
};
int special_builtin_count = sizeof(special_builtins)/sizeof(struct builtin_s);


#define UTILITY             "builtin"


/*
 * return 1 if the given cmd name is a defined function, 0 otherwise.
 */
int is_function(char *cmd)
{
    return get_func(cmd) ? 1 : 0;
}


/*
 * return 1 if the given cmd name is a builtin utility, 0 otherwise.
 */
int is_builtin(char *cmd)
{
    return is_special_builtin(cmd) ? 1 : is_regular_builtin(cmd);
}


/*
 * return 1 if the given cmd name is an enabled special builtin utility, -1 if it
 * is an enabled regular builtin utility, 0 otherwise.
 */
int is_enabled_builtin(char *cmd)
{
    if(!cmd)
    {
        return 0;
    }
    int     j;
    for(j = 0; j < special_builtin_count; j++)
    {
        if(strcmp(special_builtins[j].name, cmd) == 0 &&
           flag_set(special_builtins[j].flags, BUILTIN_ENABLED))
        {
            return 1;
        }
    }
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(strcmp(regular_builtins[j].name, cmd) == 0 &&
           flag_set(regular_builtins[j].flags, BUILTIN_ENABLED))
        {
            return -1;
        }
    }
    return 0;
}


/*
 * return 1 if the given cmd name is a special builtin utility, 0 otherwise.
 */
int is_special_builtin(char *cmd)
{
    if(!cmd)
    {
        return 0;
    }
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < special_builtin_count; j++)
    {
        if(special_builtins[j].namelen != cmdlen)
        {
            continue;
        }
        if(strcmp(special_builtins[j].name, cmd) == 0)
        {
            return 1;
        }
    }
    return 0;
}


/*
 * return 1 if the given cmd name is a regular builtin utility, 0 otherwise.
 */
int is_regular_builtin(char *cmd)
{
    if(!cmd)
    {
        return 0;
    }
    size_t  cmdlen = strlen(cmd);
    int     j;
    for(j = 0; j < regular_builtin_count; j++)
    {
        if(regular_builtins[j].namelen != cmdlen)
        {
            continue;
        }
        if(strcmp(regular_builtins[j].name, cmd) == 0)
        {
            return 1;
        }
    }
    return 0;
}


/*
 * print the list of builtins. depending on the value of the 'which' parameter,
 * the function may print the list of special, regular, or all builtins.
 */
void list(char which)
{
    int i;
    switch(which)
    {
        case 's':
            printf("special shell builtins:\n");
            for(i = 0; i < special_builtin_count; i++)
            {
                printf("  %s%*s%s\n", special_builtins[i].name, (int)(10-special_builtins[i].namelen),
                       " ", special_builtins[i].explanation);
            }
            break;
            
        case 'r':
            printf("regular shell builtins:\n");
            for(i = 0; i < regular_builtin_count; i++)
            {
                printf("  %s%*s%s\n", regular_builtins[i].name, (int)(10-regular_builtins[i].namelen),
                       " ", regular_builtins[i].explanation);
            }
            break;
                
        default:
            printf("special shell builtins:\n");
            for(i = 0; i < special_builtin_count; i++)
            {
                printf("  %s%*s%s\n", special_builtins[i].name, (int)(10-special_builtins[i].namelen),
                       " ", special_builtins[i].explanation);
            }
            printf("\nregular shell builtins:\n");
            for(i = 0; i < regular_builtin_count; i++)
            {
                printf("  %s%*s%s\n", regular_builtins[i].name, (int)(10-regular_builtins[i].namelen),
                       " ", regular_builtins[i].explanation);
            }
            break;
    }
}


/*
 * the builtin utility (non-POSIX extension).. if called with arguments (but no options),
 * treats the arguments as a builtin utility name (and its arguments), and executs the
 * builtin utility.. otherwise, prints the list of builtin utilities. which utilities are
 * printed depends on the passed option (-s for special, -r for regular, -a for all utilities).
 *
 * returns non-zero if passed a name for an unknown builtin utility, or if an unknown
 * option was supplied. otherwise, returns the exit status of the builtin utility it executed,
 * or 0 if it only printed the list of builtins without executing any.
 * 
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help builtin` or `builtin -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int builtin_builtin(int argc, char **argv)
{
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvsra", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_BUILTIN, 1, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 's':
                list('s');
                return 0;
                
            case 'r':
                list('r');
                return 0;
                
            case 'a':
                list('a');
                return 0;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /* no arguments. print the list of all builtin utilities */
    if(v >= argc)
    {
        list('a');
        return 0;
    }
    
    /* run the shell builtin */
    argc -= v;
    if(!do_special_builtin(argc, &argv[v]))
    {
        if(!do_regular_builtin(argc, &argv[v]))
        {
            fprintf(stderr, "%s: not a shell builtin: %s\n", UTILITY, argv[v]);
            return 2;
        }
    }
    return exit_status;
}
