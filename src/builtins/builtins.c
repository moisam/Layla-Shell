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

/* defined in coproc.c */
int    coproc(int argc, char **argv, struct io_file_s *io_files);

/* POSIX section 1.6: regular builtin utilities */

/* TODO: those should be implemented such that they can be invoked
 *       through exec() or directly by env, ...
 */

struct builtin_s regular_builtins[] =
{
    {
        "["      , 1, "test file attributes and compare strings"                  , test   , 1,       /* POSIX */
        "Usage: %s -option expression ]",
        "expression  conditional expression to test\r\n\n"
        "For the list of options and their meanings, run 'help [['\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "[["     , 2, "test file attributes and compare strings"                  , test   , 9,       /* POSIX */
        "Usage: %s -abcdefgGhkLNOprsSuwx file ]]\r\n"
        "       %s [-nz] string ]]\r\n"
        "       %s -o [?]op ]]\r\n"
        "       %s -t fd ]]\r\n"
        "       %s file1 op file2 ]]\r\n"
        "       %s expr1 -op expr2 ]]\r\n"
        "       %s !expr ]]\r\n"
        "       %s expr1 && expr2 ]]\r\n"
        "       %s expr1 || expr2 ]]",
        "file        file name or path\r\n"
        "string      character string to be tested. If string is supplied without \r\n"
        "              option, the result is true if it is not null, false otherwise\r\n"
        "op          single character or multi-character option name\r\n"
        "fd          open file descriptor\r\n\n"
        "File operators are:\r\n"
        "  -a        return true if file exists\r\n"
        "  -b        return true if file exists and is a block device\r\n"
        "  -c        return true if file exists and is a character device\r\n"
        "  -d        return true if file exists and is a directory\r\n"
        "  -e        similar to -a\r\n"
        "  -f        return true if file exists and is a regular file\r\n"
        "  -g        return true if file exists and its setgid bit is set\r\n"
        "  -G        return true if file exists and its gid matches egid of this \r\n"
        "              process\r\n"
        "  -h        return true if file exists and is a symbolic link\r\n"
        "  -k        return true if file exists and its sticky bit is set\r\n"
        "  -L        similar to -h\r\n"
        "  -N        return true if file exists and its mtime > atime\r\n"
        "  -O        return true if file exists and is owned by this process's euid\r\n"
        "  -p        return true if file exists and is a pipe or FIFO\r\n"
        "  -r        return true if file exists and is readable by this process\r\n"
        "  -s        return true if file exists and its size > 0\r\n"
        "  -S        return true if file exists and is a socket\r\n"
        "  -u        return true if file exists and its setuid bit is set\r\n"
        "  -w        return true if file exists and is writeable by this process\r\n"
        "  -x        return true if file exists and is executable by this process\r\n\n"
        "String length operators are:\r\n"
        "  -n        return true if string's length is non-zero\r\n"
        "  -z        return true if string's length is zero\r\n\n"
        "Option operators are:\r\n"
        "  -o op     return true if option op is set\r\n"
        "  -o ?op    return true if op is a valid option name\r\n\n"
        "File descriptor operators are:\r\n"
        "  -t        return true if fd is an open file descriptor and is associated\r\n"
        "              with a terminal device\r\n\n"
        "Comparison operators are:\r\n"
        "  file1 -ef file2    true if file1 and file2 exist and refer to the same file\r\n"
        "  file1 -nt file2    true if file1 exists and file2 doesn't, or file1 is newer\r\n"
        "                       than file2\r\n"
        "  file1 -ot file2    true if file2 exists and file1 doesn't, or file1 is older\r\n"
        "                       than file2\r\n"
        "  exp1 -eq exp2      true if exp1 is equal to exp2\r\n"
        "  exp1 -ge exp2      true if exp1 is greater than or equal to exp2\r\n"
        "  exp1 -gt exp2      true if exp1 is greater than exp2\r\n"
        "  exp1 -le exp2      true if exp1 is less than or equal to exp2\r\n"
        "  exp1 -lt exp2      true if exp1 is less than exp2\r\n"
        "  exp1 -ne exp2      true if exp1 is not equal to exp2\r\n"
        "  string == pattern  true if string matches pattern\r\n"
        "  string = pattern   similar to the above operator\r\n"
        "  string != pattern  true if string does not match pattern\r\n"
        "  string1 < string2  true if string1 comes before string2 based on ASCII value \r\n"
        "                       of their characters\r\n"
        "  string1 > string2  true if string1 comes after string2 based on ASCII value \r\n"
        "                       of their characters\r\n\n"
        "Other operators:\r\n"
        "  -v var             return true if var is a set shell variable (one with \r\n"
        "                       assigned value)\r\n"
        "  !expression        true if expression is false\r\n"
        "  expr1 && expr2     true if expr1 and expr2 are both true\r\n"
        "  expr1 || expr2     true if either expr1 or expr2 is true\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    { 
        "alias"  , 5, "define or display aliases"                                 , alias  , 1,       /* POSIX */
        "Usage: %s [-hv] [alias-name[=string] ...]",
        "alias-name    write alias definition to standard output\r\n"
        "alias-name=string\r\n"
        "              assign the value of string to alias-name\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "bg"     , 2, "run jobs in the background"                                , bg     , 1,       /* POSIX */
        "Usage: %s [-hv] [job_id...]",
        "job_id      specify the job to run as background job\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "bugreport", 9, "send bugreports to the shell's author(s)"                , bugreport, 1,       /* non-POSIX */
        "Usage: %s",
        "",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "builtin" , 7, "print the list of shell builtin utilities"                , builtin, 1,       /* non-POSIX */
        "Usage: %s [-hvsra] [name [args]]",
        "name       the name of a shell builtin utility to invoke\r\n"
        "args       arguments to pass to the builtin utility\r\n\n"
        "Options:\r\n"
        "  -a        list both special and regular builtins\r\n"
        "  -r        list shell regular builtins only\r\n"
        "  -s        list shell special builtins only\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "caller"  , 6, "print the context of any active subroutine call"          , caller, 1,       /* non-POSIX */
        "Usage: %s [n]",
        "n          non-negative integer denoting one of the callframe in the current\r\n"
        "           call stack. The current frame is 0. Each call to a function or dot\r\n"
        "           script results in a new entry added to the call stack.\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "cd"     , 2, "change the working directory"                              , cd     , 2,       /* POSIX */
        "Usage: %s [-h] [-nplv] [-L|-P] [directory]\r\n"
        "       %s [-h] [-nplv] -",
        "directory   Directory path to go to\r\n\n"
        "Options:\r\n"
        "  -L        logically handle dot-dot\r\n"
        "  -P        physically handle dot-dot\r\n"
        "  -l|n|p|v  these options have the same meaning as when used with the dirs\r\n"
        "              builtin. They all imply -p\r\n",
        BUILTIN_PRINT_HOPTION  | BUILTIN_ENABLED,  /* print only the -h option (suppress printing the -v option) */
    },
    {
        "command", 7, "execute a simple command"                                  , command, 2,       /* POSIX */
        "Usage: %s [-hp] command_name [argument ...]\r\n"
        "       %s [-hp][-v|-V] command_name",
        "commmand    command to be executed\r\n\n"
        "Options:\r\n"
        "  -p        search command using a default value for PATH\r\n"
        "  -v        show the command (or pathname) to be used by the shell\r\n"
        "  -V        show how the shell will interpret 'command'\r\n",
        BUILTIN_PRINT_HOPTION  | BUILTIN_ENABLED,  /* print only the -h option (suppress printing the -v option) */
    },
    {
        "coproc"  , 6, "execute commands in a coprocess (subshell with pipe)"      , coproc   , 1,       /* non-POSIX */
        "Usage: %s command [redirections]",
        "command       the command to be executed in the subshell.\r\n"
        "redirections  optional file redirections. A pipe is opened between the shell and the\r\n"
        "                coprocess before any redirections are performed. Shell variable $COPROC_PID contains\r\n"
        "                the PID of the coprocess. Shell variable $COPROC0 points to the reading end of the\r\n"
        "                pipe (connected to command's stdout), while variable $COPROC1 points to the writing\r\n"
        "                end of the pipe (connected to command's stdin).\r\n\n"
        "              you can feed output to the process by invoking:\r\n\n"
        "                $ cmd >&p\r\n\n"
        "              similarly, you can read the process's output by invoking:\r\n\n"
        "                $ cmd <&p\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "declare" , 7, "declare variables and give them attributes"                , declare  , 1,       /* non-POSIX */
        "Usage: %s [-hvfFgrxlut] [-p] [name=[value]...]",
        "name        variable to which an attribute or value is set\r\n"
        "value       the value to give to the variable called name\r\n\n"
        "Options:\r\n"
        "  -f        restrict output to shell functions\r\n"
        "  -F        don't print function definitions\r\n"
        "  -g        declare/modify variables at the global scope\r\n"
        "  -l        all characters in variable's value are converted to lowercase on assignment\r\n"
        "  -p        print the attributes and values of each name\r\n"
        "  -r        mark each name as readonly\r\n"
        "  -t        give functions the trace attribute (doesn't work on variables)\r\n"
        "  -u        all characters in variable's value are converted to uppercase on assignment\r\n"
        "  -x        mark each name as export\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "dirs"    , 4, "display the contents of the directory stack"              , dirs   , 2,       /* non-POSIX */
        "Usage: %s [-hclpv] [+N | -N]\r\n"
        "       %s -S|-L [filename]",
        "+N          print the N-th directory from the top (the left side of the \r\n"
        "              printed list), counting from zero (which is the current working \r\n"
        "              directory)\r\n"
        "-N          print the N-th directory from the bottom (the right side of the \r\n"
        "              printed list), counting from zero (which is the first dir pushed \r\n"
        "              on the stack)\r\n"
        "filename    the file to save/load the directory stack to/from\r\n\n"
        "Options:\r\n"
        "  -c        clear the stack, i.e. remove all directories\r\n"
        "  -l        print full pathnames, don't use ~ to indicate the home directory\r\n"
        "  -L        load the directory stack from the given filename. If no filename is\r\n"
        "              supplied, use $DIRSFILE or default to ~/.lshdirs\r\n"
        "  -n        wrap entries before they reach edge of the screen\r\n"
        "  -p        print each directory on a separate line\r\n"
        "  -S        save the directory stack to the given filename. If no filename is\r\n"
        "              supplied, use $DIRSFILE or default to ~/.lshdirs\r\n"
        "  -v        print each directory with its index on a separate line\r\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "disown"  , 6, "not send HUP signal to jobs"                              , disown , 1,       /* non-POSIX */
        "Usage: %s [-arsv] [-h] [job...]",
        "job        job ids of the jobs to prevent from receiving SIGHUP on exit\r\n\n"
        "Options:\r\n"
        "  -a        disown all jobs\r\n"
        "  -h        don't remove job from the jobs table, only mark it as disowned\r\n"
        "  -r        disown only running jobs\r\n"
        "  -s        disown only stopped jobs\r\n",
        BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,  /* print only the -v option */
    },
    {
        "dump"    , 4, "dump memory values of the passed arguments"               , dump   , 1,       /* non-POSIX */
        "Usage: %s [-hv] [argument ...]",
        "argument    can be one of the following:\r\n"
        "   symtab      will print the contents of the local symbol table\r\n"
        "   vars        will print out the shell variable list (similar to `declare -p`)\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "echo"    , 4, "echo arguments"                                           , echo   , 1,       /* non-POSIX */
        "Usage: %s [-enE] [args...]",
        "args        strings to echo\r\n\n"
        "Options:\r\n"
        "  -e        allow escaped characters in arguments\r\n"
        "  -E        don't allow escaped characters in arguments\r\n"
        "  -n        suppress newline echoing\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "enable"  , 6, "enable/disable shell builtins"                            , enable , 1,       /* non-POSIX */
        "Usage: %s [-ahnprsv] [name ...]",
        "name       the name of a shell builtin utility\r\n"
        "Options:\r\n"
        "  -a        print a list of all builtins, enabled and disabled\r\n"
        "  -n        disable each listed builtin\r\n"
        "  -p        print a list of enabled builtins\r\n"
        "  -r        print a list of enabled and disabled regular builtins\r\n"
        "  -s        print a list of enabled and disabled special builtins\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "false"  , 5, "return false value"                                        , false  , 1,       /* POSIX */
        "Usage: %s",
        "",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "fc"     , 2, "process the command history list"                          , fc     , 3,       /* POSIX */
        "Usage: %s [-hvr] [-e editor] [first [last]]\r\n"
        "       %s -l [-hvnr] [first [last]]\r\n"
        "       %s -s [-hv] [old=new] [first]",
        "editor      editor to use in editing commands\r\n"
        "first,last  select commands to list or edit\r\n"
        "old=new     replace first occurence of old with new\r\n\n"
        "Options:\r\n"
        "  -e        specify the editor to use when editing commands\r\n"
        "  -l        list commands, don't invoke them\r\n"
        "  -n        suppress command numbers when listing\r\n"
        "  -r        reverse order of commands\r\n"
        "  -s        re-execute commands without invoking editor\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "fg"     , 2, "run jobs in the foreground"                                , fg     , 1,       /* POSIX */
        "Usage: %s [-hv] [job_id]",
        "job_id      specify the job to run as foreground job\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "getopts", 7, "parse utility options"                                     , getopts, 1,       /* POSIX */
        "Usage: %s optstring name [arg...]",
        "optstring   string of option characters to be recognized\r\n"
        "name        shell variable to save in the found option\r\n"
        "arg...      list of arguments to parse instead of positional args\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "glob"   , 4, "echo arguments, delimited by NULL characters"              , __glob , 1,       /* non-POSIX */
        "Usage: %s [-eE] [args...]",
        "args       strings to echo\r\n\n"
        "Options:\r\n"
        "  -e       allow escaped characters in arguments\r\n"
        "  -E       don't allow escaped characters in arguments\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "hash"   , 4, "remember or report utility locations"                      , hash   , 2,       /* POSIX */
        "Usage: %s [-hvld] [-p path] [-r] utility...\r\n"
        "       %s -a",
        "utility...  the name of a utility to search and add to the hashtable\r\n\n"
        "Options:\r\n"
        "  -a        forget, then re-search and re-hash all utilities whose names are\r\n"
        "              currently in the hashtable\r\n"
        "  -d        forget the location of each passed utility\r\n"
        "  -l        print the list of hashed utilities and their paths\r\n"
        "  -p        perform utility search using path instead of the $PATH variable\r\n"
        "  -r        forget all previously remembered utility locations\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "help"    , 4, "show help for builtin utilities and commands"             , help   , 1,       /* non-POSIX */
        "Usage: %s [-ds] [command]",
        "command     the name of a builtin utility for which to print help\r\n\n"
        "Options:\r\n"
        "  -d        print a short description of each command\r\n"
        "  -s        print the usage or synopsis of each command\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "history" , 7, "print command history"                                    , history, 6,       /* non-POSIX */
        "Usage: %s [-hR] [n]\r\n"
        "       %s -c\r\n"
        "       %s -d offset\r\n"
        "       %s -d start-end\r\n"
        "       %s [-anrwSL] [filename]\r\n"
        "       %s -ps arg ...",
        "n            print only the last n lines\r\n\n"
        "Options:\r\n"
        "  -a         append the in-memory history list to filename. If filename is not\r\n"
        "               supplied, the default history file is used\r\n"
        "  -c         clear the history list\r\n"
        "  -d offset  delete history entry at position offset. Negative offsets count\r\n"
        "               from the end of the list; offset -1 is the last command entered\r\n"
        "  -d start-end\r\n"
        "             delete history entries between offsets start and end, which can be\r\n"
        "               negative, as described above\r\n"
        "  -h         print history entries without leading numbers\r\n"
        "  -L         equivalent to -r\r\n"
        "  -n         append the entries from filename to the in-memory list. If filename\r\n"
        "               is not supplied, the default history file is used\r\n"
        "  -p         perform history substitution on args and print the result on stdout\r\n"
        "  -r         read the history file and append the entries to the in-memory list\r\n"
        "  -R         reverse the listing order (most recent entries are printed first)\r\n"
        "  -s         add args to the end of the history list as one entry\r\n"
        "  -S         equivalent to -w\r\n"
        "  -w         write out the current in-memory list to filename. If filename is not\r\n"
        "               supplied, the default history file is used\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "hup"    , 3, "run a command, receiving SIGHUP"                           , hup    , 1,       /* non-POSIX */
        "Usage: %s [command]",
        "command     the command to run (must be an external command)\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "jobs"   , 4, "display status of jobs in the current session"             , jobs   , 2,       /* POSIX */
        "Usage: %s [-hnrsv] [-l|-p] [job_id...]\r\n"
        "       %s -x command [argument...]",
        "job_id...      job ID(s) for which to display status\r\n"
        "command        command to run\r\n"
        "argument...    arguments to pass to command\r\n\n"
        "Options:\r\n"
        "  -l        provide more (long) information\r\n"
        "  -n        report only jobs that changed status since last notification\r\n"
        "  -p        display only process ID(s) of process group leaders\r\n"
        "  -r        report only running jobs\r\n"
        "  -s        report only stopped jobs\r\n"
        "  -x        replace all 'job_id's in 'command' and 'argument's with the\r\n"
        "              process group ID of the respective job, then run command, passing\r\n"
        "              it the given arguments\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "kill"   , 4, "terminate or signal processes"                             , __kill , 6,       /* POSIX */
        "Usage: %s [-hv]\r\n"
        "       %s -s signal_name pid...\r\n"
        "       %s -n signal_number pid...\r\n"
        "       %s [-l|-L] [exit_status]\r\n"
        "       %s [-signal_name] pid...\r\n"
        "       %s [-signal_number] pid...",
        "signal_name     symbolic name of the signal to send\r\n"
        "signal_number   non-negative number of the signal to send\r\n"
        "pid...          process ID or process group ID, or job ID number\r\n"
        "exit_status     signal number or exit status of a signaled process\r\n\n"
        "Options:\r\n"
        "  -l, -L    write values of all sig_names, or the sig_name associated with \r\n"
        "              the given exit_status (or sig_number)\r\n"
        "  -s        specity the symbolic name of the signal to send\r\n"
        "  -n        specity the signal number to send\r\n",        
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "let"    , 3, "evaluate arithmetic expressions"                           , let    , 1,       /* non-POSIX */
        "Usage: %s [args...]",
        "args        arithmetic expressions to evaluate\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "mailcheck", 9, "check for mail at specified intervals"                    , mail   , 1,       /* non-POSIX */
        "Usage: %s [-hvq]",
        "Options:\r\n"
        "  -q        do not output messages in case of error or no mail\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "memusage", 8, "show the shell's memory usage"                             , memusage  , 1,       /* non-POSIX */
        "Usage: %s arg...",
        "Arguments show the memory allocated for different shell internal structures:\r\n"
        "  aliases             show the memory allocated for alias names and values\r\n"
        "  cmdbuf, cmdbuffer   show the memory allocated for the command line buffer\r\n"
        "  dirstack            show the memory allocated for the directory stack\r\n"
        "  hash, hashtab       show the memory allocated for the commands hashtable\r\n"
        "  history             show the memory allocated for the command line history table\r\n"
        "  input               show the memory allocated for the currently executing translation unit\r\n"
        "  stack, symtabs      show the memory allocated for the symbol table stack\r\n"
        "  strbuf, strtab      show the memory allocated for the internal strings buffer\r\n"
        "  traps               show the memory allocated for the signal traps\r\n"
        "  vm                  show the memory usage of different segments (RSS, stack, data)\r\n\n"
        "Options:\r\n"
        "  -l        show long output (i.e. print more details)\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "newgrp" , 6, "change to a new group"                                     , newgrp , 1,       /* POSIX */
        "Usage: %s [-hv] [-l] [group]",
        "group       group name (or ID) to which the real and effective group\r\n"
        "              IDs shall be set\r\n\n"
        "Options:\r\n"
        "  -l        change the environment to a login environment\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "nice"   , 4, "run a command with the given priority"                     , __nice , 2,       /* non-POSIX */
        "Usage: %s [+n] [command]\r\n"
        "       %s [-n] [command]",
        "+n          a positive nice priority to give to command, or the shell if no command\r\n"
        "              is given (the plus sign can be omitted)\r\n"
        "-n          a negative nice priority. only root can pass -ve nice values\r\n"
        "command     the command to run under priority n (must be an external command)\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "nohup"  , 5, "run a command, ignoring SIGHUP"                            , hup    , 1,       /* non-POSIX */
        "Usage: %s [command]",
        "command     the command to run (must be an external command)\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "notify" , 6, "notify immediately when jobs change status"                , notify , 1,       /* non-POSIX */
        "Usage: %s [job ...]",
        "job         the job id of the job to mark for immediate notification\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "popd"   , 4, "pop directories off the stack and cd to them"              , popd   , 1,       /* non-POSIX */
        "Usage: %s [-chlnpsv] [+N | -N]",
        "+N          remove the N-th directory, counting from 0 from the left\r\n"
        "-N          remove the N-th directory, counting from 0 from the right\r\n\n"
        "If called without arguments, popd removes the top directory from the stack and calls \r\n"
        "cd to change the current working directory to the new top directory (equivalent to \r\n"
        "`popd +0`).\r\n\n"
        "Options:\r\n"
        "  -c        manipulate the stack, but don't cd to the directory\r\n"
        "  -s        don't output the dirstack after popping off it\r\n"
        "  -l|n|v|p  have the same meaning as for the dirs builtin (see `help dirs`)\r\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "printenv", 8, "print the names and values of environment variables"      , printenv, 1,       /* non-POSIX */
        "Usage: %s [-hv0] [name ...]",
        "name        the name of an environment variable\r\n\n"
        "Options:\r\n"
        "  -0        terminate each entry with NULL instead of a newline character\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "pushd"  , 5, "push directories on the stack and cd to them"              , pushd  , 1,       /* non-POSIX */
        "Usage: %s [-chlnpsv] [+N | -N | dir]",
        "+N          rotate the stack and bring the N-th directory, counting from 0 from the \r\n"
        "              left, to the top of the stack\r\n"
        "-N          rotate the stack and bring the N-th directory, counting from 0 from the \r\n"
        "              right, to the top of the stack\r\n"
        "dir         push dir on the stack and cd to it. If dir is dash '-', this equals the \r\n"
        "              previous working directory, as stored in $PWD\r\n\n"
        "Options:\r\n"
        "  -c        manipulate the stack, but don't cd to the directory\r\n"
        "  -s        don't output the dirstack after pushing the directory on it\r\n"
        "  -l|n|v|p  have the same meaning as for the dirs builtin (see `help dirs`)\r\n"
        "  -h        show utility help (this page)\r\n\n"
        "Notes:\r\n"
        "If called without arguments, pushd exchanges the top two directories on the stack and\r\n"
        "calls cd to change the current working directory to the new top directory.\r\n"
        "If the 'pushdtohome' extra option is set (by calling `setx -s pushdtohome`), pushd pushes\r\n"
        "the value of $HOME and cd's to it instead of exchanging the top two directories.\r\n"
        "If the 'dunique' extra option is set, pushd removes instances of dir from the stack\r\n"
        "before pushing it. If the 'dextract' extra option is set, pushd extracts the N-th directory\r\n"
        "and pushes it on top of the stack.\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "pwd"    , 3, "return working directory name"                             , pwd    , 1,       /* POSIX */
        "Usage: %s [-hv] [-L|-P]",
        "Options:\r\n"
        "  -L        logically handle dot-dot\r\n"
        "  -P        physically handle dot-dot\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "read"   , 4, "read a line from standard input"                           , __read , 1,       /* POSIX */
        "Usage: %s [-hv] [-rs] [-d delim] [-nN num] [-t secs] [-u fd] [-p msg] [var...]",
        "delim       read up to the first character of delim instead of a newline\r\n"
        "num         max number of bytes to read\r\n"
        "secs        timeout when reading from a terminal or pipe/fifo\r\n"
        "fd          file descriptor to use instead of stdin (0). fd should have \r\n"
        "              been open with an earlier invocation of exec\r\n"
        "var...      the name of shell variables to assign input to. If none is \r\n"
        "              supplied, environment variable $REPLY is used.\r\n"
        "msg         a string to be printed before reading input\r\n\n"
        "Options:\r\n"
        "  -d        read up to delim (instead of newline)\r\n"
        "  -n, -N    read a maximum of num bytes\r\n"
        "  -p        print argument msg before reading input\r\n"
        "  -s        save input as a new entry in the history file\r\n"
        "  -t        read fails if no input after secs seconds\r\n"
        "  -u        read fails if no input after secs seconds\r\n"
        "  -r        read from fd (instead of stdin)\r\n\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "setenv" , 6, "set environment variable values"                           , __setenv, 1,       /* non-POSIX */
        "Usage: %s [-hv] [name[=value] ...]",
        "name        the environment variable to set\r\n"
        "value       the value to give to name, NULL if no value is given\r\n\n"
        "This utility sets both the environment variable and the shell variable with\r\n"
        "the same name. If no arguments are given, it prints the names and values of\r\n"
        "all the set environment variables.\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "stop"   , 4, "stop background jobs"                                      , stop    , 1,       /* non-POSIX */
        "Usage: %s [-hv] job",
        "job         the background job to stop\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "test"   , 4, "test file attributes and compare strings"                  , test    , 1,       /* POSIX */
        "Usage: %s -option expression",
        "For the list of options and their meanings, run 'help [['\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "true"   , 4, "return true value"                                         , true   , 1,       /* POSIX */
        "Usage: %s",
        "",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
#if 0
        "Usage: %s [-hv]",
        "Options:\r\n",
        3,  /* print both -v and -h options */
#endif
    },
    {
        "type"   , 4, "write a description of command type"                       , type   , 1,       /* POSIX */
        "Usage: %s name...",
        "command     the name of a command or function for which to write description\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "ulimit" , 6, "set or report shell resource limits"                       , __ulimit, 1,       /* POSIX */
        "Usage: %s [-h] [-acdflmnpstuv] [limit]",
        "limit       the new limit for the given resource\r\n\n"
        "Options:\r\n"
        "  -a        report all current limits\r\n"
        "  -c        set/report the maximum size of core files created\r\n"
        "  -d        set/report the maximum size of a process's data segment\r\n"
        "  -e        set/report the maximum nice value (scheduling priority)\r\n"
        "  -f        set/report the maximum size of files written by a process\r\n"
        "  -i        set/report the maximum number of pending signals\r\n"
        "  -l        set/report the maximum size of memory a process may lock\r\n"
        "  -m        set/report the maximum resident set size (RSS)\r\n"
        "  -n        set/report the maximum number of open file descriptors\r\n"
        "  -p        set/report the pipe buffer size in kbytes\r\n"
        "  -q        set/report the maximum number of kbytes in POSIX message queues\r\n"
        "  -r        set/report the maximum real-time priority\r\n"
        "  -s        set/report the maximum stack size\r\n"
        "  -t        set/report the maximum amount of cpu time (seconds)\r\n"
        "  -u        set/report the maximum number of user processes\r\n"
        "  -v        set/report the size of virtual memory\r\n"
        "  -x        set/report the maximum number of file locks\r\n",
        BUILTIN_PRINT_HOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "umask"  , 5, "get or set the file mode creation mask"                    , __umask, 1,       /* POSIX */
        "Usage: %s [-hvp] [-S] [mask]",
        "mask        the new file mode creation mask\r\n\n"
        "Options:\r\n"
        "  -S        produce symbolic output\r\n"
        "  -p        print output that can be reused as shell input\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unalias", 7, "remove alias definitions"                                  , unalias, 2,       /* POSIX */
        "Usage: %s [-hv] alias-name...\r\n"
        "       %s [-hv] -a",
        "alias-name  the name of an alias to be removed\r\n\n"
        "Options:\r\n"
        "  -a        remove all alias definitions\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unlimit", 7, "remove limits on system resources"                         , unlimit, 1,       /* non-POSIX */
        "Usage: %s [-hHfSv] [limit ...]\r\n"
        "       %s [-HS] -a",
        "limit       the name of a system resource, which can be one of the following:\r\n"
        "  core    , -c        the maximum size of core files created\r\n"
        "  data    , -d        the maximum size of a process's data segment\r\n"
        "  nice    , -e        the maximum nice value (scheduling priority)\r\n"
        "  file    , -f        the maximum size of files written by a process\r\n"
        "  signal  , -i        the maximum number of pending signals\r\n"
        "  mlock   , -l        the maximum size of memory a process may lock\r\n"
        "  rss     , -m        the maximum resident set size (RSS)\r\n"
        "  fd      , -n        the maximum number of open file descriptors\r\n"
        "  buffer  , -p        the pipe buffer size in kbytes\r\n"
        "  message , -q        the maximum number of kbytes in POSIX message queues\r\n"
        "  rtprio  , -r        the maximum real-time priority\r\n"
        "  stack   , -s        the maximum stack size\r\n"
        "  cputime , -t        the maximum amount of cpu time (seconds)\r\n"
        "  userproc, -u        the maximum number of user processes\r\n"
        "  virtmem , -v        the size of virtual memory\r\n"
        "  flock   , -x        the maximum number of file locks\r\n"
        "  all     , -a        all the above\r\n\n"
        "Options and limit names must be passed separately. To remove all hard limits, invoke\r\n"
        "either of the following commands:\r\n"
        "  unlimit -H -a\r\n"
        "  unlimit -H all\r\n\n"
        "Options:\r\n"
        "  -a        remove limits on all resources\r\n"
        "  -f        ignore errors\r\n"
        "  -H        remove hard limits (only root can do this)\r\n"
        "  -S        remove soft limits (the default)\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unsetenv", 8, "unset environment variable values"                        , __unsetenv, 1,       /* non-POSIX */
        "Usage: %s [-hv] [name ...]",
        "name        the environment variable to unset\r\n\n"
        "This utility unsets both the environment variable and the shell variable with\r\n"
        "the same name. If no arguments are given, nothing is done.\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "ver"     , 3, "show shell version"                                       , ver    , 1,       /* non-POSIX */
        "Usage: %s",
        "",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "wait"   , 4, "await process completion"                                  , __wait , 1,       /* POSIX */
        "Usage: %s [-hfnv] [pid...]",
        "pid...      process ID or Job ID to wait for\r\n\n"
        "Options:\r\n"
        "  -f        force jobs/processes to exit\r\n"
        "  -n        wait for any job or process\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "whence" , 6, "write a description of command type"                       , whence , 1,       /* non-POSIX */
        "Usage: %s [-afhpv] name...",
        "name        the name of a command or function for which to write description\r\n\n"
        "Options:\r\n"
        "  -a        output all possible interpretations of the command\r\n"
        "  -f        don't search for functions\r\n"
        "  -p        perform path search even if command is an alias, keyword or function name\r\n"
        "  -v        verbose output (the default)\r\n",
        2 | BUILTIN_ENABLED,  /* print only the -h option */
    },
    
};
int regular_builtin_count = sizeof(regular_builtins)/sizeof(struct builtin_s);


/* POSIX section 2.14: special builtin utilities */
struct builtin_s special_builtins[] = 
{
    {
        "break"   , 5, "exit from for, while, or until loop"                       , __break   , 1,       /* POSIX */
        "Usage: %s [n]",
        "n           exit the the n-th enclosing for, while, or until loop\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        ":"       , 1, "expand arguments (the null utility)"                       , colon     , 1,       /* POSIX */
        "Usage: %s [argument...]",
        "argument    command arguments to expand\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "continue", 8, "continue for, while, or until loop"                        , __continue, 1,       /* POSIX */
        "Usage: %s [n]",
        "n           return to the top of the n-th enclosing for, while, or until loop\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "."       , 1, "execute commands in the current environment"               , dot       , 1,       /* POSIX */
        "Usage: %s file",
        "file        execute commands from this file in the current environment\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "eval"    , 4, "construct command by concatenating arguments"              , eval      , 1,       /* POSIX */
        "Usage: %s [argument...]",
        "argument    construct a command by concatenating arguments together\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "exec"    , 4, "execute commands and open, close, or copy file descriptors", exec      , 1,       /* POSIX */
        "Usage: %s [-cl] [-a name] [command [argument...]]",
        "command     path to the command to be executed\r\n"
        "argument    execute command with arguments and open, close, or copy file descriptors\r\n\n"
        "Options:\r\n"
        "  -a        set argv[0] to 'name' instead of 'command'\r\n"
        "  -c        clear the environment before performing exec\r\n"
        "  -l        place a dash in front of argv[0], just as the login utility does\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "exit"    , 4, "exit the shell"                                            , __exit    , 1,       /* POSIX */
        "Usage: %s [n]",
        "n           exit the shell returning n as the exit status code\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "export"  , 6, "set the export attribute for variables"                    , export    , 1,       /* POSIX */
        "Usage: %s [-hvn] [-p] [name[=word]...]",
        "name        set the export attribute to the variable name\r\n"
        "word        set the value of variable name to word\r\n\n"
        "Options:\r\n"
        "  -n        remove the export attribute of passed variable names\r\n"
        "  -p        print the names and values of all exported variables\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    { 
        "local"   , 5, "define local variables"                                    , local    , 1,       /* non-POSIX */
        "Usage: %s name[=word] ...",
        "name        set the local attribute to the variable 'name'\r\n"
        "word        set the value of the variable named 'name' to 'word'\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "logout"  , 6, "exit a login shell"                                        , logout    , 1,       /* non-POSIX */
        "Usage: %s [n]",
        "n           exit a login shell returning n as the exit status code\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "readonly", 8, "set the readonly attribute for variables"                  , readonly  , 2,       /* POSIX */
        "Usage: %s name[=word]...\r\n"
        "       %s -p",
        "name        set the readonly attribute to the variable name\r\n"
        "word        set the value of variable name to word\r\n\n"
        "Options:\r\n"
        "  -p        print the names and values of all readonly variables\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "repeat"  , 6, "repeat a command count times"                              , repeat    , 1,       /* non-POSIX */
        "Usage: %s [-hv] count command",
        "count       the number of times to repeat command\r\n"
        "command     the command to execute count times\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "return"  , 6, "return from a function or dot script"                      , __return  , 1,       /* POSIX */
        "Usage: %s [n]",
        "n           exit status to return\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "set"     , 3, "set or unset options and positional parameters"            , set       , 5,       /* POSIX */
        "Usage: %s [-abCdeEfhHkmnprtTuvx] [-o option ...] [argument...]\r\n"
        "       %s [+abCdeEfhHkmnprtTuvx] [+o option ...] [argument...]\r\n"
        "       %s -- [argument...]\r\n"
        "       %s -o\r\n"
        "       %s +o",
        "--           used to delimit arguments if the first argument begins with '+' or '-',\r\n"
        "argument     values to set positional parameters to\r\n\n"
        "Options (leading '-' enables options, leading '+' disables them):\r\n"
        "  --         end of options\r\n"
        "  -a         mark all variables with the export attribute\r\n"
        "  -b         asynchronous notification of background job completions\r\n"
        "  -B         perform brace expansion, so \"{a,b}\" expands to \"a\" \"b\"\r\n"
        "  -C         don't overwrite existing files when using '>' for redirection\r\n"
        "  -d         dump the parser's Abstract Syntax Tree (AST) before executing commands\r\n"
        "  -e         exit shell on error\r\n"
        "  -E         ERR traps are inherited by shell functions, command substitutions and subshells\r\n"
        "  -f         disable pathname expansion\r\n"
        "  -h         remember utility locations when they are first invoked\r\n"
        "  -H         enable history substitution\r\n"
        "  -k         place all variable assignments in command environment (ignored)\r\n"
        "  -m         enable/disable the job control feature\r\n"
        "  -n         read commands but don't execute them (non-interactive shells only)\r\n"
        "  -o         print current options string to stdout\r\n"
        "  +o         print current options string in a format suitable for reinput to the shell\r\n"
        "  -o option  extended format for setting/unsetting options. Argument option can be:\r\n"
        "     allexport       equivalent to -a\r\n"
        "     braceexpand     equivalent to -B\r\n"
        "     errexit         equivalent to -e\r\n"
        "     errtrace        equivalent to -E\r\n"
        "     functrace       equivalent to -T\r\n"
        "     hashall         equivalent to -h\r\n"
        "     hashexpand      equivalent to -H\r\n"
        "     history         equivalent to -w\r\n"
        "     ignoreeof       prevent interactive shells from exiting on EOF\r\n"
        "     keyword         equivalent to -k\r\n"
        "     monitor         equivalent to -m\r\n"
        "     noclobber       equivalent to -C\r\n"
        "     noglob          equivalent to -f\r\n"
        "     noexec          equivalent to -n\r\n"
        "     nolog           don't save function definitions to command history list (ignored)\r\n"
        "     notify          equivalent to -b\r\n"
        "     nounset         equivalent to -u\r\n"
        "     onecmd          equivalent to -t\r\n"
        "     pipefail        pipeline's exit status is that of the rightmost command to exit with \r\n"
        "                       non-zero status, or zero if all exited successfully\r\n"
        "     privileged      equivalent to -p\r\n"
        "     verbose         equivalent to -v\r\n"
        "     vi              allow command line editing using the builtin vi editor\r\n"
        "     xtrace          equivalent to -x\r\n"
        "  -p         turn on privileged mode. $ENV file is not processed. $CDPATH and $GLOBIGNORE are\r\n"
        "               ignored. If -p is not passed to the shell, and the effective uid (gid) is not\r\n"
        "               equal to the real uid (gid), effective ids are reset to their real values\r\n"
        "  -r         enable the restricted shell. This option cannot be unset once set\r\n"
        "  -t         exit the shell after executing one command\r\n"
        "  -T         DEBUG and RETURN traps are inherited by shell functions, command substitutions\r\n"
        "               and subshells\r\n"
        "  -u         expanding unset parameters (except $@ and $*) results in error\r\n"
        "  -v         verbose mode (write input to stderr as it is read)\r\n"
        "  -x         write command trace to stderr before executing each command\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "setx"    , 4, "set/unset optional (extra) shell options"                  , setx     , 1,       /* non-POSIX */
        "Usage: %s [-hvpsuqo] option",
        "option      can be any of the following (the name inside brackets is the shell from\r\n"
        "            which the option was taken/based; 'int' means interactive shell, 'non-int'\r\n"
        "            means non-interactive shell):\r\n"
        "addsuffix          append space to file- and slash to dir-names on tab completion (tcsh)\r\n"
        "autocd             dirs passed as single-word commands are passed to 'cd' (bash int)\r\n"
        "cdable_vars        cd arguments can be variable names (bash)\r\n"
        "cdable-vars        same as the above\r\n"
        "checkhash          for hashed commands, check the file exists before exec'ing (bash)\r\n"
        "checkjobs          list stopped/running jobs and warn user before exit (bash int)\r\n"
        "checkwinsize       check window size after external cmds, updating $LINES/$COLUMNS (bash)\r\n"
        "clearscreen        clear the screen on shell's startup\r\n"
        "cmdhist            save multi-line command in a single history entry (bash)\r\n"
        "complete_fullquote quote metacharacters in filenames during completion (bash)\r\n"
        "complete-fullquote same as the above\r\n"
        "dextract           pushd extracts the given dir instead of rotating the stack (tcsh)\r\n"
        "dotglob            files starting with '.' are included in filename expansion (bash)\r\n"
        "dunique            pushd removes similar entries before pushing dir on the stack (tcsh)\r\n"
        "execfail           failing to exec a file doesn't exit the shell (bash non-int)\r\n"
        "expand_aliases     perform alias expansion (bash)\r\n"
        "expand-aliases     same as the above\r\n"
        "extglob            enable ksh-like extended pattern matching (bash)\r\n"
        "failglob           failing to match filenames to patterns result in expansion error (bash)\r\n"
        "force_fignore      $FIGNORE determines which words to ignore on word expansion (bash)\r\n"
        "force-fignore      same as the above\r\n"
        "globasciiranges    bracket pattern matching expressions use the C locale (bash)\r\n"
        "histappend         append (don't overwrite) the history list to $HISTFILE (bash)\r\n"
        "histreedit         enable the user to re-redit a failed history substitution (bash int)\r\n"
        "histverify         reload (instead of directly execute) history substitution results (bash int)\r\n"
        "hostcomplete       perform hostname completion for words containing '@' (bash int)\r\n"
        "huponexit          send SIGHUP to all jobs on exit (bash int login)\r\n"
        "inherit_errexit    command substitution subshells inherit the -e option (bash)\r\n"
        "inherit-errexit    same as the above\r\n"
        "interactive_comments\r\n"
        "                   recognize '#' as the beginning of a comment (bash int)\r\n"
        "interactive-comments\r\n"
        "                   same as the above\r\n"
        "lastpipe           last cmd of foreground pipeline is run in the current shell (bash)\r\n"
        "lithist            save multi-line commands with embedded newlines (bash with 'cmdhist' on)\r\n"
        "listjobs           list jobs when a job changes status (tcsh)\r\n"
        "listjobs_long      list jobs (detailed) when a job changes status (tcsh)\r\n"
        "listjobs-long      same as the above\r\n"
        "localvar_inherit   local vars inherit value/attribs from previous scopes (bash)\r\n"
        "localvar-inherit   same as the above\r\n"
        "localvar_unset     allow unsetting local vars in previous scopes (bash)\r\n"
        "localvar-unset     same as the above\r\n"
        "login_shell        indicates a login shell (cannot be changed) (bash)\r\n"
        "login-shell        same as the above\r\n"
        "mailwarn           warn about mail files that have already been read (bash)\r\n"
        "nocaseglob         perform case-insensitive filename expansion (bash)\r\n"
        "nocasematch        perform case-insensitive pattern matching (bash)\r\n"
        "nullglob           patterns expanding to 0 filenames expand to "" (bash)\r\n"
        "printexitvalue     output non-zero exit status for external commands (tcsh)\r\n"
        "progcomp           enable programmable completion (not yet implemented) (bash)\r\n"
        "progcomp_alias     allow alias expansion in completions (not yet implemented) (bash)\r\n"
        "promptvars         perform word expansion on prompt strings (bash)\r\n"
        "pushdtohome        pushd without arguments pushed ~ on the stack (tcsh)\r\n"
        "recognize_only_executables\r\n"
        "                   only executables are recognized in command completion (tcsh)\r\n"
        "recognize-only-executables\r\n"
        "                   same as the above\r\n"
        "restricted_shell   indicates a restricted shell (cannot be changed) (bash)\r\n"
        "restricted-shell   same as the above\r\n"
        "savedirs           save the directory stack when login shell exits (tcsh)\r\n"
        "savehist           save the history list when shell exits (tcsh)\r\n"
        "shift_verbose      allow the shift builtin to output err messages (bash)\r\n"
        "shift-verbose      same as the above\r\n"
        "sourcepath         the source builtin uses $PATH to find files (bash)\r\n"
        "usercomplete       perform hostname completion for words starting with '~'\r\n"
        "xpg_echo           echo expands backslash escape sequences by default (bash)\r\n"
        "xpg-echo           same as the above\r\n\n"
        "Options:\r\n"
        "  -o        restrict options to those recognized by `set -o`\r\n"
        "  -p        print output that can be re-input to the shell\r\n"
        "  -q        suppress normal output. the return status tells whether options are set or not\r\n"
        "  -s        set (enable) each passed option\r\n"
        "  -u        unset (disable) each passed option\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "shift"   , 5, "shift positional parameters"                               , shift     , 1,       /* POSIX */
        "Usage: %s [n]",
        "n           the value by which to shift positional parameters to the left.\r\n"
        "            parameter 1 becomes (1+n), parameters 2 becomes (2+n), and so on\r\n\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    {
        "source"  , 6, "execute commands in the current environment"               , source    , 1,       /* POSIX */
        "Usage: %s [-hv] file",
        "file        execute commands from this file in the current environment\r\n\n"
        "This command is the same as dot or '.', except when the -h option is given, where\r\n"
        "file is read and the commands are added to the history list, which is identical to\r\n"
        "invoking `history -L`.\r\n\n"
        "Options:\r\n"
        "  -h        read file and add commands to the history list\r\n",
        BUILTIN_PRINT_VOPTION | BUILTIN_ENABLED,  /* print only the -h option */
    },
    {
        "suspend" , 7, "suspend execution of the shell"                            , suspend  , 1,       /* non-POSIX */
        "Usage: %s [-fhv]",
        "Options:\r\n"
        "  -f        force suspend, even if the shell is a login shell\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "times"   , 5, "write process times"                                       , __times   , 1,       /* POSIX */
        "Usage: %s",
        "",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
    /*
     * NOTE: we removed time from this builtins list because it is now recognized as a reserved word.
     */
#if 0
    {
        "time"    , 4, "write command execution times"                             , __time    , 1,       /* POSIX */
        "Usage: %s command",
        "command     the command to execute and time\r\n\n"
        "Options:\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
#endif
    {
        "trap"    , 4, "trap signals"                                              , trap      , 2,       /* POSIX */
        "Usage: %s [-hvlp] n [condition...]\r\n"
        "       %s [-hvlp] [action condition...]",
        "n           treat all operands as conditions; reset each condition to the default value\r\n\n"
        "action      can be either:\r\n"
        "   -        reset each condition to the default value\r\n"
        "   \"\"       (empty string) ignore each condition if it arises\r\n"
        "   any other value will be read and executed by the shell when one of the corresponding\r\n"
        "   conditions arises.\r\n\n"
        "condition   can be either:\r\n"
        "   EXIT     set/reset exit traps\r\n"
        "   ERR      set/reset error traps\r\n"
        "   DEBUG    set/reset debug traps (not yet implemented!)\r\n"
        "   name     signal name without the SIG prefix\r\n\n"
        "Options:\r\n"
        "  -l        list all conditions and their signal numbers\r\n"
        "  -p        print the trap actions associated with each condition\r\n",
        3 | BUILTIN_ENABLED,  /* print both -v and -h options */
    },
    {
        "unset"   , 5, "unset values and attributes of variables and functions"    , unset     , 1,       /* POSIX */
        "Usage: %s [-fv] name...",
        "name       names of variables/functions to unset and remove from the environment.\r\n"
        "           readonly variables cannot be unset.\r\n\n"
        "Options:\r\n"
        "  -f       treat each name as a function name\r\n"
        "  -v       treat each name as a variable name\r\n",
        0 | BUILTIN_ENABLED,  /* don't print neither the -v nor the -h options */
    },
};
int special_builtin_count = sizeof(special_builtins)/sizeof(struct builtin_s);


#define UTILITY             "builtin"

void list(char which)
{
    int i;
    switch(which)
    {
        case 's':
            printf("special shell builtins:\n");
            for(i = 0; i < special_builtin_count; i++)
                printf("  %s%*s%s\n", special_builtins[i].name, 10-special_builtins[i].namelen, 
                       " ", special_builtins[i].explanation);
            break;
            
        case 'r':
            printf("regular shell builtins:\n");
            for(i = 0; i < regular_builtin_count; i++)
                printf("  %s%*s%s\n", regular_builtins[i].name, 10-regular_builtins[i].namelen, 
                       " ", regular_builtins[i].explanation);
            break;
                
        default:
            printf("special shell builtins:\n");
            for(i = 0; i < special_builtin_count; i++)
                printf("  %s%*s%s\n", special_builtins[i].name, 10-special_builtins[i].namelen, 
                       " ", special_builtins[i].explanation);
            printf("\nregular shell builtins:\n");
            for(i = 0; i < regular_builtin_count; i++)
                printf("  %s%*s%s\n", regular_builtins[i].name, 10-regular_builtins[i].namelen, 
                       " ", regular_builtins[i].explanation);
            break;
    }
}

int builtin(int argc, char *argv[])
{
    /****************************
     * process the arguments
     ****************************/
    int v = 1, c;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
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
    if(c == -1) return 2;
    
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
