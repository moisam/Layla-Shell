/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
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
#include "../debug.h"
#include "builtins.h"
#include "setx.h"


/*
 * TODO: according to POSIX, regular builtin utilities should be implemented 
 *       such that they can be invoked through exec() or directly by env, ...
 * 
 * NOTE: the %% sequence in a utility's synopsis will be converted to the 
 *       utility's name when the synopsis is printed.
 */

struct builtin_s shell_builtins[] =
{
    {
        ".", "execute commands in the current environment",
        dot_builtin,       /* POSIX */
        "%% file",
        "file        execute commands from this file in the current environment\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        ":", "expand arguments (the null utility)",
        colon_builtin,       /* POSIX */
        "%% [argument...]",
        "argument    command arguments to expand\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "[", "test file attributes and compare strings",
        test_builtin,       /* POSIX */
        "%% -option expression ]",
        "expression  conditional expression to test\n\n"
        "For the list of options and their meanings, run `help [[`\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "[[", "test file attributes and compare strings",
        test_builtin,       /* POSIX */
        "%% -abcdefgGhkLNOprsSuwx file ]]\n"
        "%% [-nz] string ]]\n"
        "%% -o [?]op ]]\n"
        "%% -t fd ]]\n"
        "%% file1 op file2 ]]\n"
        "%% expr1 -op expr2 ]]\n"
        "%% !expr ]]\n"
        "%% expr1 && expr2 ]]\n"
        "%% expr1 || expr2 ]]",
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
        "alias", "define or display aliases",
        alias_builtin,       /* POSIX */
        "%% [-hvp] [alias-name[=string] ...]",
        "alias-name    write alias definition to standard output\n"
        "alias-name=string\n"
        "              assign the value of string to alias-name\n\n"
        "Options:\n"
        "  -p        print all defined aliases and their values\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    { 
        "bg", "run jobs in the background",
        bg_builtin,       /* POSIX */
        "%% [-hv] [job_id...]",
        "job_id      specify the job to run as background job\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "break", "exit from for, while, or until loop",
        break_builtin,       /* POSIX */
        "%% [n]",
        "n           exit the the n-th enclosing for, while, or until loop\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "bugreport", "send bugreports to the shell's author(s)",
        bugreport_builtin,       /* non-POSIX */
        "%%",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "builtin", "print the list of shell builtin utilities",
        builtin_builtin,       /* non-POSIX */
        "%% [-hvsra] [name [args]]",
        "name       the name of a shell builtin utility to invoke\n"
        "args       arguments to pass to the builtin utility\n\n"
        "Options:\n"
        "  -a        list both special and regular builtins\n"
        "  -r        list shell regular builtins only\n"
        "  -s        list shell special builtins only\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "caller", "print the context of any active subroutine call",
        caller_builtin,       /* non-POSIX */
        "%% [n]",
        "n          non-negative integer denoting one of the callframe in the current\n"
        "           call stack. The current frame is 0. Each call to a function or dot\n"
        "           script results in a new entry added to the call stack.\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "cd", "change the working directory",
        cd_builtin,       /* POSIX */
        "%% [-h] [-nplv] [-L|-P] [directory]\n"
        "%% [-h] [-nplv] -",
        "directory   Directory path to go to\n\n"
        "Options:\n"
        "  -L        logically handle dot-dot\n"
        "  -P        physically handle dot-dot\n"
        "  -l|n|p|v  these options have the same meaning as when used with the dirs\n"
        "              builtin. They all imply -p\n",
        BUILTIN_PRINT_HOPTION  | BUILTIN_ENABLED,  /* print only the -h option (suppress printing the -v option) */
    },
    {
        "command", "execute a simple command",
        command_builtin,       /* POSIX */
        "%% [-hp] command_name [argument ...]\n"
        "%% [-hp][-v|-V] command_name",
        "commmand    command to be executed\n\n"
        "Options:\n"
        "  -p        search command using a default value for PATH\n"
        "  -v        show the command (or pathname) to be used by the shell\n"
        "  -V        show how the shell will interpret 'command'\n",
        BUILTIN_PRINT_HOPTION  | BUILTIN_ENABLED,  /* print only the -h option (suppress printing the -v option) */
    },
    {
        "continue", "continue for, while, or until loop",
        continue_builtin,       /* POSIX */
        "%% [n]",
        "n           return to the top of the n-th enclosing for, while, or until loop\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
#if 0
    {
        "coproc", "execute commands in a coprocess (subshell with pipe)",
        coproc_builtin,       /* non-POSIX */
        "%% command [redirections]",
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
#endif
    {
        "declare", "declare variables and give them attributes",
        declare_builtin,       /* non-POSIX */
        "%% [-hvfFgrxlut] [-p] [name=[value]...]",
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
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "dirs", "display the contents of the directory stack",
        dirs_builtin,       /* non-POSIX */
        "%% [-hclpv] [+N | -N]\n"
        "%% -S|-L [filename]",
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
        "disown", "not send HUP signal to jobs",
        disown_builtin,       /* non-POSIX */
        "%% [-arsv] [-h] [job...]",
        "job        job ids of the jobs to prevent from receiving SIGHUP on exit\n\n"
        "Options:\n"
        "  -a        disown all jobs\n"
        "  -h        don't remove job from the jobs table, only mark it as disowned\n"
        "  -r        disown only running jobs\n"
        "  -s        disown only stopped jobs\n",
        BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,  /* print only the -v option */
    },
    {
        "dump", "dump memory values of the passed arguments",
        dump_builtin,       /* non-POSIX */
        "%% [-hv] [argument ...]",
        "argument    can be one of the following:\n"
        "   symtab      will print the contents of the local symbol table\n"
        "   vars        will print out the shell variable list (similar to `declare -p`)\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "echo", "echo arguments",
        echo_builtin,       /* non-POSIX */
        "%% [-enE] [args...]",
        "args        strings to echo\n\n"
        "Options:\n"
        "  -e        allow escaped characters in arguments\n"
        "  -E        don't allow escaped characters in arguments\n"
        "  -n        suppress newline echoing\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "enable", "enable/disable shell builtins",
        enable_builtin,       /* non-POSIX */
        "%% [-ahnprsv] [name ...]",
        "name       the name of a shell builtin utility\n"
        "Options:\n"
        "  -a        print a list of all builtins, enabled and disabled\n"
        "  -n        disable each listed builtin\n"
        "  -p        print a list of enabled builtins\n"
        "  -r        print a list of enabled and disabled regular builtins\n"
        "  -s        print a list of enabled and disabled special builtins\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "eval", "construct command by concatenating arguments",
        eval_builtin,       /* POSIX */
        "%% [argument...]",
        "argument    construct a command by concatenating arguments together\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "exec", "execute commands and open, close, or copy file descriptors",
        exec_builtin,       /* POSIX */
        "%% [-cl] [-a name] [command [argument...]]",
        "command     path to the command to be executed\n"
        "argument    execute command with arguments and open, close, or copy file descriptors\n\n"
        "Options:\n"
        "  -a        set argv[0] to 'name' instead of 'command'\n"
        "  -c        clear the environment before performing exec\n"
        "  -l        place a dash in front of argv[0], just as the login utility does\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "exit", "exit the shell",
        exit_builtin,       /* POSIX */
        "%% [n]",
        "n           exit the shell returning n as the exit status code\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "export", "set the export attribute for variables",
        export_builtin,       /* POSIX */
        "%% [-hvn] [-p] [name[=word]...]",
        "name        set the export attribute to the variable name\n"
        "word        set the value of variable name to word\n\n"
        "Options:\n"
        "  -n        remove the export attribute of passed variable names\n"
        "  -p        print the names and values of all exported variables\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,
    },
    {
        "false", "return false value",
        false_builtin,       /* POSIX */
        "%%",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "fc", "process the command history list",
        fc_builtin,       /* POSIX */
        "%% [-hvr] [-e editor] [first [last]]\n"
        "%% -l [-hvnr] [first [last]]\n"
        "%% -s [-hv] [old=new] [first]",
        "editor      editor to use in editing commands\n"
        "first,last  select commands to list or edit\n"
        "old=new     replace first occurence of old with new\n\n"
        "Options:\n"
        "  -e        specify the editor to use when editing commands\n"
        "  -l        list commands, don't invoke them\n"
        "  -n        suppress command numbers when listing\n"
        "  -r        reverse order of commands\n"
        "  -s        re-execute commands without invoking editor\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "fg", "run jobs in the foreground",
        fg_builtin,       /* POSIX */
        "%% [-hv] [job_id]",
        "job_id      specify the job to run as foreground job\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "getopts", "parse utility options",
        getopts_builtin,       /* POSIX */
        "%% optstring name [arg...]",
        "optstring   string of option characters to be recognized\n"
        "name        shell variable to save in the found option\n"
        "arg...      list of arguments to parse instead of positional args\n",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "glob", "echo arguments, delimited by NULL characters",
        glob_builtin,       /* non-POSIX */
        "%% [-eE] [args...]",
        "args       strings to echo\n\n"
        "Options:\n"
        "  -e       allow escaped characters in arguments\n"
        "  -E       don't allow escaped characters in arguments\n",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "hash", "remember or report utility locations",
        hash_builtin,       /* POSIX */
        "%% [-hvld] [-p path] [-r] utility...\n"
        "%% -a",
        "utility...  the name of a utility to search and add to the hashtable\n\n"
        "Options:\n"
        "  -a        forget, then re-search and re-hash all utilities whose names are\n"
        "              currently in the hashtable\n"
        "  -d        forget the location of each passed utility\n"
        "  -l        print the list of hashed utilities and their paths\n"
        "  -p        perform utility search using path instead of the $PATH variable\n"
        "  -r        forget all previously remembered utility locations\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "help", "show help for builtin utilities and commands",
        help_builtin,       /* non-POSIX */
        "%% [-ds] [command]",
        "command     the name of a builtin utility for which to print help\n\n"
        "Options:\n"
        "  -d        print a short description for each command\n"
        "  -m        print a manpage-like help page for each command\n"
        "  -s        print the usage or synopsis for each command\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "history", "print command history",
        history_builtin,       /* non-POSIX */
        "%% [-hR] [n]\n"
        "%% -c\n"
        "%% -d offset\n"
        "%% -d start-end\n"
        "%% [-anrwSL] [filename]\n"
        "%% -ps arg ...",
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
        "hup", "run a command, receiving SIGHUP",
        hup_builtin,       /* non-POSIX */
        "%% [command]",
        "command     the command to run (must be an external command)\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "jobs", "display status of jobs in the current session",
        jobs_builtin,       /* POSIX */
        "%% [-hnrsv] [-l|-p] [job_id...]\n"
        "%% -x command [argument...]",
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
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "kill", "terminate or signal processes",
        kill_builtin,       /* POSIX */
        "%% [-hv]\n"
        "%% -s signal_name pid...\n"
        "%% -n signal_number pid...\n"
        "%% [-l|-L] [exit_status]\n"
        "%% [-signal_name] pid...\n"
        "%% [-signal_number] pid...",
        "signal_name     symbolic name of the signal to send\n"
        "signal_number   non-negative number of the signal to send\n"
        "pid...          process ID or process group ID, or job ID number\n"
        "exit_status     signal number or exit status of a signaled process\n\n"
        "Options:\n"
        "  -l, -L    write values of all sig_names, or the sig_name associated with \n"
        "              the given exit_status (or sig_number)\n"
        "  -s        specity the symbolic name of the signal to send\n"
        "  -n        specity the signal number to send\n",        
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    { 
        "let", "evaluate arithmetic expressions",
        let_builtin,       /* non-POSIX */
        "%% [args...]",
        "args        arithmetic expressions to evaluate\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    { 
        "local", "define local variables",
        local_builtin,       /* non-POSIX */
        "%% name[=word] ...",
        "name        set the local attribute to the variable 'name'\n"
        "word        set the value of the variable named 'name' to 'word'\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "logout", "exit a login shell",
        logout_builtin,       /* non-POSIX */
        "%% [n]",
        "n           exit a login shell returning n as the exit status code\n\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "mailcheck", "check for mail at specified intervals",
        mailcheck_builtin,       /* non-POSIX */
        "%% [-hvq]",
        "Options:\n"
        "  -q        do not output messages in case of error or no mail\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "memusage", "show the shell's memory usage",
        memusage_builtin,       /* non-POSIX */
        "%% arg...",
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
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "newgrp", "change to a new group",
        newgrp_builtin,       /* POSIX */
        "%% [-hv] [-l] [group]",
        "group       group name (or ID) to which the real and effective group\n"
        "              IDs shall be set\n\n"
        "Options:\n"
        "  -l        change the environment to a login environment\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "nice", "run a command with the given priority",
        nice_builtin,       /* non-POSIX */
        "%% [+n] [command]\n"
        "%% [-n] [command]",
        "+n          a positive nice priority to give to command, or the shell if no command\n"
        "              is given (the plus sign can be omitted)\n"
        "-n          a negative nice priority. only root can pass -ve nice values\n"
        "command     the command to run under priority n (must be an external command)\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "nohup", "run a command, ignoring SIGHUP",
        hup_builtin,       /* non-POSIX */
        "%% [command]",
        "command     the command to run (must be an external command)\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "notify", "notify immediately when jobs change status",
        notify_builtin,       /* non-POSIX */
        "%% [job ...]",
        "job         the job id of the job to mark for immediate notification\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "popd", "pop directories off the stack and cd to them",
        popd_builtin,       /* non-POSIX */
        "%% [-chlnpsv] [+N | -N]",
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
        "printenv", "print the names and values of environment variables",
        printenv_builtin,       /* non-POSIX */
        "%% [-hv0] [name ...]",
        "name        the name of an environment variable\n\n"
        "Options:\n"
        "  -0        terminate each entry with NULL instead of a newline character\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "pushd", "push directories on the stack and cd to them",
        pushd_builtin,       /* non-POSIX */
        "%% [-chlnpsv] [+N | -N | dir]",
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
        "pwd", "return working directory name",
        pwd_builtin,       /* POSIX */
        "%% [-hv] [-L|-P]",
        "Options:\n"
        "  -L        logically handle dot-dot\n"
        "  -P        physically handle dot-dot\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "read", "read a line from standard input",
        read_builtin,       /* POSIX */
        "%% [-hv] [-rs] [-d delim] [-nN num] [-t secs] [-u fd] [-p msg] [var...]",
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
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "readonly", "set the readonly attribute for variables",
        readonly_builtin,       /* POSIX */
        "%% name[=word]...\n"
        "%% -p",
        "name        set the readonly attribute to the variable name\n"
        "word        set the value of variable name to word\n\n"
        "Options:\n"
        "  -p        print the names and values of all readonly variables\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "repeat", "repeat a command count times",
        repeat_builtin,       /* non-POSIX */
        "%% [-hv] count command",
        "count       the number of times to repeat command\n"
        "command     the command to execute count times\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "return", "return from a function or dot script",
        return_builtin,       /* POSIX */
        "%% [n]",
        "n           exit status to return\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "set", "set or unset options and positional parameters",
        set_builtin,       /* POSIX */
        "%% [-abCdeEfhHkmnprtTuvx] [-o option ...] [argument...]\n"
        "%% [+abCdeEfhHkmnprtTuvx] [+o option ...] [argument...]\n"
        "%% -- [argument...]\n"
        "%% -o\n"
        "%% +o",
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
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "setenv", "set environment variable values",
        setenv_builtin,       /* non-POSIX */
        "%% [-hv] [name[=value] ...]",
        "name        the environment variable to set\n"
        "value       the value to give to name, NULL if no value is given\n\n"
        "This utility sets both the environment variable and the shell variable with\n"
        "the same name. If no arguments are given, it prints the names and values of\n"
        "all the set environment variables.\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "setx", "set/unset optional (extra) shell options",
        setx_builtin,       /* non-POSIX */
        "%% [-hvpsuqo] option",
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
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "shift", "shift positional parameters",
        shift_builtin,       /* POSIX */
        "%% [n]",
        "n           the value by which to shift positional parameters to the left.\n"
        "            parameter 1 becomes (1+n), parameters 2 becomes (2+n), and so on\n\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "shopt", "set/unset optional (extra) shell options",
        setx_builtin,       /* non-POSIX */
        "%% [-hvpsuqo] option",
        "For an explanation of all the options and arguments, run `help setx`\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "source", "execute commands in the current environment",
        source_builtin,       /* POSIX */
        "%% [-hv] file",
        "file        execute commands from this file in the current environment\n\n"
        "This command is the same as dot or '.', except when the -h option is given, where\n"
        "file is read and the commands are added to the history list, which is identical to\n"
        "invoking `history -L`.\n\n"
        "Options:\n"
        "  -h        read file and add commands to the history list\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,  /* print only the -v option */
    },
    {
        "stop", "stop background jobs",
        stop_builtin,       /* non-POSIX */
        "%% [-hv] job",
        "job         the background job to stop\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "suspend", "suspend execution of the shell",
        suspend_builtin,       /* non-POSIX */
        "%% [-fhv]",
        "Options:\n"
        "  -f        force suspend, even if the shell is a login shell\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "test", "test file attributes and compare strings",
        test_builtin,       /* POSIX */
        "%% -option expression",
        "For the list of options and their meanings, run `help [[`\n",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "times", "write process times",
        times_builtin,       /* POSIX */
        "%%",
        "",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "trap", "trap signals",
        trap_builtin,       /* POSIX */
        "%% [-hvlp] n [condition...]\n"
        "%% [-hvlp] [action condition...]",
        "n           treat all operands as conditions; reset each condition to the default value\n\n"
        "action      can be either:\n"
        "   -        reset each condition to the default value\n"
        "   \"\"       (empty string) ignore each condition if it arises\n"
        "   any other value will be read and executed by the shell when one of the corresponding\n"
        "   conditions arises.\n\n"
        "condition   can be either:\n"
        "   EXIT     set/reset the exit trap\n"
        "   ERR      set/reset the error trap\n"
        "   CHLD     set/reset the child exit trap\n"
        "   DEBUG    set/reset the debug trap\n"
        "   RETURN   set/reset the return (from function or script) trap\n"
        "   name     signal name without the SIG prefix\n\n"
        "Options:\n"
        "  -l        list all conditions and their signal numbers\n"
        "  -p        print the trap actions associated with each condition\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,
    },
    {
        "true", "return true value",
        true_builtin,       /* POSIX */
        "%%",
        "",
        BUILTIN_ENABLED,       /* don't print neither the -v nor the -h options */
    },
    {
        "type", "write a description of command type",
        type_builtin,       /* POSIX */
        "%% name...",
        "command     the name of a command or function for which to write description\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "typeset", "declare variables and give them attributes",
        declare_builtin,       /* non-POSIX */
        "%% [-hvfFgrxlut] [-p] [name=[value]...]",
        "For an explanation of all the options and arguments, run `help declare`\n",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "ulimit", "set or report shell resource limits",
        ulimit_builtin,       /* POSIX */
        "%% [-h] [-acdflmnpstuv] [limit]",
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
        "umask", "get or set the file mode creation mask",
        umask_builtin,       /* POSIX */
        "%% [-hvp] [-S] [mask]",
        "mask        the new file mode creation mask\n\n"
        "Options:\n"
        "  -S        produce symbolic output\n"
        "  -p        print output that can be reused as shell input\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "unalias", "remove alias definitions",
        unalias_builtin,       /* POSIX */
        "%% [-hv] alias-name...\n"
        "%% [-hv] -a",
        "alias-name  the name of an alias to be removed\n\n"
        "Options:\n"
        "  -a        remove all alias definitions\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "unlimit", "remove limits on system resources",
        unlimit_builtin,       /* non-POSIX */
        "%% [-hHfSv] [limit ...]\n"
        "%% [-HS] -a",
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
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "unset", "unset values and attributes of variables and functions",
        unset_builtin,       /* POSIX */
        "%% [-fv] name...",
        "name       names of variables/functions to unset and remove from the environment.\n"
        "           readonly variables cannot be unset.\n\n"
        "Options:\n"
        "  -f       treat each name as a function name\n"
        "  -v       treat each name as a variable name\n",
        BUILTIN_SPECIAL_BUILTIN | BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "unsetenv", "unset environment variable values",
        unsetenv_builtin,       /* non-POSIX */
        "%% [-hv] [name ...]",
        "name        the environment variable to unset\n\n"
        "This utility unsets both the environment variable and the shell variable with\n"
        "the same name. If no arguments are given, nothing is done.\n\n"
        "Options:\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    {
        "ver", "show shell version",
        ver_builtin,       /* non-POSIX */
        "%%",
        "",
        BUILTIN_ENABLED,      /* don't print neither the -v nor the -h options */
    },
    {
        "wait", "await process completion",
        wait_builtin,       /* POSIX */
        "%% [-hfnv] [pid...]",
        "pid...      process ID or Job ID to wait for\n\n"
        "Options:\n"
        "  -f        force jobs/processes to exit\n"
        "  -n        wait for any job or process\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,
    },
    { 
        "whence", "write a description of command type",
        whence_builtin,       /* non-POSIX */
        "%% [-afhpv] name...",
        "name        the name of a command or function for which to write description\n\n"
        "Options:\n"
        "  -a        output all possible interpretations of the command\n"
        "  -f        don't search for functions\n"
        "  -p        perform path search even if command is an alias, keyword or function name\n"
        "  -v        verbose output (the default)\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        NULL, NULL, NULL, NULL, NULL, 0,
    },
};


#define UTILITY             "builtin"


/*
 * return 1 if the given cmd name is a defined function, 0 otherwise.
 */
int is_function(char *cmd)
{
    return get_func(cmd) ? 1 : 0;
}


/*
 * if cmd is a builtin utility, return the utility's struct builtin_s, or
 * NULL otherwise.
 */
struct builtin_s *is_builtin(char *cmd)
{
    if(!cmd)
    {
        return NULL;
    }

    struct builtin_s *u = shell_builtins;
    for( ; u->name; u++)
    {
        if(strcmp(u->name, cmd) == 0)
        {
            return u;
        }
    }
    return NULL;
}


/*
 * return 1 if the given cmd name is an enabled special builtin utility, -1 if it
 * is an enabled regular builtin utility, 0 otherwise.
 */
struct builtin_s *is_enabled_builtin(char *cmd)
{
    struct builtin_s *builtin = is_builtin(cmd);

    if(builtin && flag_set(builtin->flags, BUILTIN_ENABLED))
    {
        return builtin;
    }
    
    return NULL;
}


/*
 * return 1 if the given cmd name is a special builtin utility, 0 otherwise.
 */
struct builtin_s *is_special_builtin(char *cmd)
{
    struct builtin_s *builtin = is_builtin(cmd);

    if(builtin && flag_set(builtin->flags, BUILTIN_SPECIAL_BUILTIN))
    {
        return builtin;
    }
    
    return NULL;
}


/*
 * return 1 if the given cmd name is a regular builtin utility, 0 otherwise.
 */
struct builtin_s *is_regular_builtin(char *cmd)
{
    struct builtin_s *builtin = is_builtin(cmd);

    if(builtin && !flag_set(builtin->flags, BUILTIN_SPECIAL_BUILTIN))
    {
        return builtin;
    }
    
    return NULL;
}


/*
 * search the list of builtin utilities looking for a utility whose name matches
 * argv[0] of the passed **argv list.. if found, we execute the builtin utility,
 * passing it the **argv list as if we're executing an external command, then 
 * we return 1.. otherwise, we return 0 to indicate we failed in finding a 
 * builtin utility whose name matches the command we are meant to execute (that
 * is, argv[0]).. if the 'special_utility' flag is non-zero, we search for a special
 * builtin utility with the given name, otherwise we search for a regular one.
 * 
 * returns 1 if the builtin utility is executed, otherwise 0.
 */
int do_builtin(int argc, char **argv, int special_utility)
{
    char   *cmd = argv[0];
    struct builtin_s *utility = special_utility ?
                                is_special_builtin(cmd) : is_regular_builtin(cmd);
                                  
    if(utility && flag_set(utility->flags, BUILTIN_ENABLED))
    {
        int (*func)(int, char **) = (int (*)(int, char **))utility->func;
        int status = do_exec_cmd(argc, argv, NULL, func);
        set_internal_exit_status(status);
        return 1;
    }

    return 0;
}


/*
 * execute a builtin utility internally from within the shell.. we first reset
 * $OPTIND, so that the builtin utility can call getopts() to parse its options..
 * we then call the utlity, and reset $OPTIND to its previous value so that user
 * commands won't be distrubted by our execution of the utility.
 * 
 * returns the exit status of the executed utility.
 */
int do_builtin_internal(int (*builtin)(int, char **), int argc, char **argv)
{
    /*
     * all builtins (except getopts) may change $OPTIND, so save it and reset
     * its value to NULL.
     */
    if(builtin != getopts_builtin)
    {
        save_OPTIND();
    }

    /* execute the builtin */
    int res = builtin(argc, argv);

    /* reset $OPTIND to its previous value */
    if(builtin != getopts_builtin)
    {
        reset_OPTIND();
    }
    return res;
}


/*
 * this function does what its name says it does.
 */
void disable_nonposix_builtins(void)
{
    BUGREPORT_BUILTIN.flags &= ~BUILTIN_ENABLED;
    BUILTIN_BUILTIN.flags   &= ~BUILTIN_ENABLED;
    CALLER_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    COPROC_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    DECLARE_BUILTIN.flags   &= ~BUILTIN_ENABLED;
    DIRS_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    DISOWN_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    DUMP_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    ECHO_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    GLOB_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    HISTORY_BUILTIN.flags   &= ~BUILTIN_ENABLED;
    HUP_BUILTIN.flags       &= ~BUILTIN_ENABLED;
    LET_BUILTIN.flags       &= ~BUILTIN_ENABLED;
    LOCAL_BUILTIN.flags     &= ~BUILTIN_ENABLED;
    LOGOUT_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    MAILCHECK_BUILTIN.flags &= ~BUILTIN_ENABLED;
    MEMUSAGE_BUILTIN.flags  &= ~BUILTIN_ENABLED;
    NICE_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    NOHUP_BUILTIN.flags     &= ~BUILTIN_ENABLED;
    NOTIFY_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    POPD_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    PRINTENV_BUILTIN.flags  &= ~BUILTIN_ENABLED;
    PUSHD_BUILTIN.flags     &= ~BUILTIN_ENABLED;
    REPEAT_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    SETENV_BUILTIN.flags    &= ~BUILTIN_ENABLED;
    SETX_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    SHOPT_BUILTIN.flags     &= ~BUILTIN_ENABLED;
    STOP_BUILTIN.flags      &= ~BUILTIN_ENABLED;
    SUSPEND_BUILTIN.flags   &= ~BUILTIN_ENABLED;
    TYPESET_BUILTIN.flags   &= ~BUILTIN_ENABLED;
    UNLIMIT_BUILTIN.flags   &= ~BUILTIN_ENABLED;
    UNSETENV_BUILTIN.flags  &= ~BUILTIN_ENABLED;
    VER_BUILTIN.flags       &= ~BUILTIN_ENABLED;
    WHENCE_BUILTIN.flags    &= ~BUILTIN_ENABLED;

    /*
     * we won't disable the 'enable' builtin so the user can selectively enable builtins
     * when they're in the POSIX mode. if you insist on disabling ALL non-POSIX builtins,
     * uncomment the next line.
     */

    /*
     * the 'help' builtin should also be available, even in POSIX mode. if you want to
     * disable it in POSIX mode, uncomment the next line.
     */
}


/*
 * helper function to print the list of builtin utilities.. the function
 * has a 'special_list' argument, which contains 1 if the list to be printed is
 * the special builtin utilities list, 0 if its the regular builtin utilities list.
 */
void __list(int special_list)
{
    struct builtin_s *u = shell_builtins;
    for( ; u->name; u++)
    {
        int is_special = flag_set(u->flags, BUILTIN_SPECIAL_BUILTIN);
        if(special_list == is_special)
        {
            printf("  %s%*s%s\n", u->name, (int)(10-strlen(u->name)), " ", u->explanation);
        }
    }
}


/*
 * print the list of builtins. depending on the value of the 'which' parameter,
 * the function may print the list of special, regular, or all builtins.
 */
void list(char which)
{
    switch(which)
    {
        case 's':
            printf("special shell builtins:\n");
            __list(1);
            break;
            
        case 'r':
            printf("regular shell builtins:\n");
            __list(0);
            break;
                
        default:
            printf("special shell builtins:\n");
            __list(1);
            printf("\nregular shell builtins:\n");
            __list(0);
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
    int which = 0;

    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvsra", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], &BUILTIN_BUILTIN, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 's':
                if(which == 'r' || which == 'a')
                {
                    /* list all (special + regular) utilities */
                    which = 'a';
                }
                else
                {
                    /* list only special utilities */
                    which = 's';
                }
                break;
                
            case 'r':
                if(which == 's' || which == 'a')
                {
                    /* list all (special + regular) utilities */
                    which = 'a';
                }
                else
                {
                    /* list only regular utilities */
                    which = 'r';
                }
                break;
                
            case 'a':
                which = 'a';
                break;
        }
    }

    /* unknown option */
    if(c == -1)
    {
        return 2;
    }
    
    /* no arguments. print the list of all builtin utilities */
    if(which || v >= argc)
    {
        list(which ? which : 'a');
        return 0;
    }
    
    /* run the shell builtin */
    argc -= v;
    if(!do_builtin(argc, &argv[v], 1))
    {
        if(!do_builtin(argc, &argv[v], 0))
        {
            PRINT_ERROR("%s: not a shell builtin: %s\n", UTILITY, argv[v]);
            return 2;
        }
    }
    return exit_status;
}
