/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019 (c)
 * 
 *    file: redirect.c
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

#define _XOPEN_SOURCE   500     /* fdopen() and mkdtemp() */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "../cmd.h"
#include "backend.h"
#include "../parser/parser.h"
#include "../error/error.h"
#include "../debug.h"


/*
 * perform process substitution. the op parameter specifies the redirection operator to
 * apply to the process substitution, which can be '<' or '>'. the cmdline parameter
 * contains the command(s) to execute in the process. the tmpname parameter contains
 * the pathname of the file we'll use for the redirection.
 */
void redirect_proc_do(char *cmdline, char op, char *tmpname)
{
    if(fork_child() == 0)
    {
        int i = strlen(cmdline)-1;
        int j = i+strlen(tmpname)+12;
        char *buf = malloc(j);
        if(!buf)
        {
            return;
        }
        char c = cmdline[i];
        cmdline[i] = ' ';
        sprintf(buf, "{ %s} %c%s", cmdline+1, (op == '>') ? '<' : '>', tmpname);
        cmdline[i] = c;
        struct source_s src;
        src.buffer   = buf;
        src.bufsize  = j;
        src.curpos   = -2;
        src.srctype = SOURCE_FIFO;
        src.srcname = NULL;
        parse_and_execute(&src);
        unlink(tmpname);
        exit(exit_status);
    }
}


/*
 * prepare for process substitution by opening a FIFO under /tmp/lsh, or if the
 * system doesn't support named FIFO's, create a regular pipe and use its file
 * descriptors in place of the FIFO. in the latter case, the pipe will be passed
 * to the process as a file named /dev/fdN, where N is the file descriptor number.
 * returns the pathname of the FIFO/pipe, so that we can pass it to the other end,
 * i.e. the command which will read from or write to the process we'll fork.
 */
char *redirect_proc(char op, char *cmdline)
{
    char *tmpdir = get_shell_varp("TMPDIR", "/tmp");
    int len = strlen(tmpdir);
    char tmpname[len+12];
    int tries = 100;
    while(tries--)
    {
        sprintf(tmpname, "%s%clsh/fifo%d", tmpdir, '/', tries);
        /* try to perform process substitution via using a named pipe/fifo */
        if(mkfifo(tmpname, 0600) != 0)
        {
            if(errno == EEXIST)
            {
                continue;
            }
            /*
             * if the system doesn't support named pipes, or another error occurred, such as
             * insufficient disk space, try performing this via a regular pipe, whose path we'll
             * pass as /dev/fd/filedes. if the system doesn't support this scheme, we won't be able
             * to perform process substitution.
             */
            int filedes[2];
            if(pipe(filedes) == 0)
            {
                char buf[16];
                sprintf(buf, "/dev/fd/%d", filedes[(op == '<') ? 0 : 1]);
                if(file_exists(buf))
                {
                    redirect_proc_do(cmdline, op, buf);
                    if(op == '<')
                    {
                        close(filedes[1]);
                        return get_malloced_str(buf);
                    }
                    else
                    {
                        close(filedes[0]);
                        return get_malloced_str(buf);
                    }
                    return NULL;
                }
                else
                {
                    fprintf(stderr, "%s: error creating fifo: %s\n", SHELL_NAME, "system doesn't support `/dev/fd` file names");
                    close(filedes[0]);
                    close(filedes[1]);
                    return NULL;
                }
            }
            else
            {
                fprintf(stderr, "%s: error creating fifo: %s\n", SHELL_NAME, strerror(errno));
                return NULL;
            }
        }
        redirect_proc_do(cmdline, op, tmpname);
        return get_malloced_str(tmpname);
    }
    return NULL;
}

/*
 * get the slot belonging to this fileno, or else the first empty
 * slot in the redirection table. returns -1 if no slot is available.
 */
int get_slot(int fileno, struct io_file_s *io_files)
{
    int i;
    for(i = 0; i < FOPEN_MAX; i++)
    {
        if(io_files[i].fileno == fileno || io_files[i].fileno == -1)
        {
            return i;
        }
    }
    return -1;
}

/*
 * prepare a redirection file from the given redirection node.
 */
int redirect_prep_node(struct node_s *child, struct io_file_s *io_files)
{
    int fileno = -1, i;
    char *s;
    char buf[32];
    switch(child->val_type)
    {
        case VAL_SINT:
            fileno = child->val.sint;
            break;
            
        case VAL_STR:
            s = child->val.str;
            i = strlen(s);
            /*
             * check for the non-POSIX bash and zsh redirection extensions of {var}<&N
             * and {var}>&N. the {var} part would have been added as the previous
             * node.
             */
            if(s[0] == '{' && s[i-1] == '}')
            {
                s[i-1] = '\0';
                struct node_s *child2 = child->first_child;
                if(child2)
                {
                    child2 = child2->first_child;
                }
                if(child2->val_type != VAL_STR)
                {
                    child2 = NULL;
                }
                /*
                 * if the path is '-', it means we need to close the fd, so we'll get the fd number
                 * from the shell variable where it was saved before.
                 */
                if(child2 && child2->val.str && strcmp(child2->val.str, "-") == 0)
                {
                    char *s2 = get_shell_varp(s+1, NULL);
                    if(!s2)
                    {
                        s[i-1] = '}';
                        break;
                    }
                    char *strend;
                    fileno = strtol(s2, &strend, 10);
                    s[i-1] = '}';
                    if(strend == s2)
                    {
                        fileno = -1;
                    }
                    break;
                }
                /* search for an available slot for the new file descriptor, starting with #10 */
                for(fileno = 10; fileno < FOPEN_MAX; fileno++)
                {
                    /* get a vacant file descriptor */
                    errno = 0;
                    if(fcntl(fileno, F_GETFD) == -1 && errno == EBADF)
                    {
                        /* we need to save the file number for later reference */
                        struct symtab_entry_s *entry = add_to_symtab(s+1);
                        if(entry)
                        {
                            sprintf(buf, "%d", fileno);
                            symtab_entry_setval(entry, buf);
                        }
                        break;
                    }
                }           
                s[i-1] = '}';
            }
            break;
            
        default:
            break;
    }
        
    if(fileno < 0 || fileno >= FOPEN_MAX)
    {
        sprintf(buf, "%d", fileno);
        BACKEND_RAISE_ERROR(INVALID_REDIRECT_FILENO, buf, NULL);
        //fprintf(stderr, "%s: invalid redirection file number: %d\n", SHELL_NAME, fileno);
        return -1;
    }
    
    if((i = get_slot(fileno, io_files)) == -1)
    {
        fprintf(stderr, "%s: too many open files\n", SHELL_NAME);
        return -1;
    }
    if(!do_io_redirect(child, &io_files[i]))
    {
        return -1;
    }
    else
    {
        io_files[i].fileno = fileno;
        /*
         * in the case of combined stdout/stderr redirection, redirected word must
         * not be a number or '-'. this form of redirection is written as >&word or &>word.
         */
        child = child->first_child;
        if(fileno == 1 && child && child->type == NODE_IO_FILE)
        {
            if(io_files[i].duplicates == -1 && io_files[i].path[0] != '-' &&
               (child->val.chr == IO_FILE_AND_GREAT_GREAT || child->val.chr == IO_FILE_GREATAND))
            {
                struct node_s *node2 = io_file_node(2, IO_FILE_GREATAND, "1", child->lineno);
                if(node2)
                {
                    if((i = get_slot(2, io_files)) != -1)
                    {
                        do_io_redirect(node2, &io_files[i]);
                        io_files[i].fileno = 2;
                    }
                    free_node_tree(node2);
                }
                else
                {
                    fprintf(stderr, "%s: failed to duplicate stdout on stderr\n", SHELL_NAME);
                    return -1;
                }
            }
        }
    }
    return 0;
}


/*
 * initialize the redirection table for the command to be executed.
 */
int redirect_prep(struct node_s *node, struct io_file_s *io_files)
{
    if(!io_files)
    {
        return 0;
    }
    int  i;
    int  total_redirects = 0;
    for(i = 0; i < FOPEN_MAX; i++)
    {
        io_files[i].path       = NULL;
        io_files[i].fileno     = -1;
        io_files[i].duplicates = -1;
        io_files[i].flags      =  0;
    }
    if(!node)
    {
        return 0;
    }

    struct node_s *child = node->first_child;
    /* prepare the redirections */
    while(child)
    {
        if(child->type == NODE_IO_REDIRECT)
        {
            if(redirect_prep_node(child, io_files) == -1)
            {
                total_redirects = -1;
            }
            else
            {
                total_redirects++;
            }
        }
        child = child->next_sibling;
    }
    return total_redirects;
}


#define OPEN_SPECIAL_ERROR()                            \
do {                                                    \
    fprintf(stderr,                                     \
            "%s: error opening %s: "                    \
            "use of invalid redirection operator\n",  \
            SHELL_NAME, path);                          \
    errno = EINVAL;                                     \
    return -1;                                          \
} while(0)

/*
 * open a special file, such as a remote tcp or udp connection, or a filename
 * such as /dev/stdin.
 * returns the file descriptor on which the file is opened, -1 otherwise.
 */
int open_special(char *path, int flags)
{
    int fd = -1, i, remote = 0;
    if(strstr(path, "/dev/fd/") == path)
    {
        i = atoi(path+8);
        if(i < 0)
        {
            return -1;
        }
        fd = dup(i);
    }
    else if(strcmp(path, "/dev/stdin") == 0)
    {
        if(!flag_set(flags, R_FLAG))
        {
            OPEN_SPECIAL_ERROR();
        }
        fd = dup(0);
    }
    else if(strcmp(path, "/dev/stdout") == 0)
    {
        if(!flag_set(flags, R_FLAG))
        {
            OPEN_SPECIAL_ERROR();
        }
        fd = dup(1);
    }
    else if(strcmp(path, "/dev/stderr") == 0)
    {
        if(!flag_set(flags, R_FLAG))
        {
            OPEN_SPECIAL_ERROR();
        }
        fd = dup(2);
    }
    else if(strstr(path, "/dev/tcp/") == path)
    {
        remote = 1;
    }
    else if(strstr(path, "/dev/udp/") == path)
    {
        remote = 2;
    }
    if(remote)
    {
        /* get the hostname and port parts */
        char *s1 = path+9;
        char *s2 = strchr(s1, '/');
        if(!s2)
        {
            fprintf(stderr, "%s: error opening socket: missing port number\n", SHELL_NAME);
            errno = EINVAL;
            return -1;
        }
        if(!*++s2)
        {
            fprintf(stderr, "%s: error opening socket: missing port number\n", SHELL_NAME);
            errno = EINVAL;
            return -1;
        }
        
        int port;
        struct sockaddr_in serv_addr;
        struct hostent *server;
        port = atoi(s2);
        fd = socket(AF_INET, (remote == 1) ? SOCK_STREAM : SOCK_DGRAM, 0);
        if(fd < 0) 
        {
            //fprintf(stderr, "%s: error opening socket: %s\n", SHELL_NAME, strerror(errno));
            return -1;
        }
        s2[-1] = '\0';
        server = gethostbyname(s1);
        if(server == NULL)
        {
            fprintf(stderr, "%s: no such host: %s\n", SHELL_NAME, s1);
            s2[-1] = '/';
            return -1;
        }
        s2[-1] = '/';
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy(server->h_addr_list[0], &serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(port);
        if(connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        {
            //fprintf(stderr, "%s: error connecting to socket: %s\n", SHELL_NAME, strerror(errno));
            return -1;
        }
    }
    return fd;
}


/*
 * perform the redirections in the *io_files redirection list. this function should be
 * called after the shell has forked a child process to handle execution of a command.
 * if called from the shell itself, the redirections will affect the file descriptors
 * of the shell process.
 */
int __redirect_do(struct io_file_s *io_files, int do_savestd)
{
    int i, j;
    /* perform the redirections */
    for(i = 0; i < FOPEN_MAX; i++)
    {
        j = io_files[i].fileno;
        char *path;
        if((path = io_files[i].path))
        {
            //if(i <= 2) save_std(i);
            if(path[0] == '-' && path[1] == '\0')
            {
                if(j <= 2 && do_savestd)
                {
                    save_std(j);
                }
                close(j);
                /* POSIX says we can open an "unspecified file" in this case */
                if(j == 0)
                {
                    open("/dev/null", O_RDONLY);
                }
                else if(j == 1 || j == 2)
                {
                    open("/dev/null", O_WRONLY);
                }
            }
            else if(path[0] != '\0')
            {
                path = word_expand_to_str(path);
                if(!path)
                {
                    continue;
                }

                /* check the noclobber situation */
                if(flag_set(io_files[i].flags, W_FLAG) && option_set('C'))
                {
                    struct stat st;
                    if(stat(path, &st) == 0)
                    {
                        if(S_ISREG(st.st_mode))
                        {
                            free(path);
                            continue;
                        }
                    }
                    io_files[i].flags |= O_EXCL;
                }
                if(io_files[i].flags == C_FLAG)
                {
                    io_files[i].flags = W_FLAG;
                }
                
                /*
                 * >#((expr)) and <#((expr)) are non-POSIX extensions to move
                 * I/O file pointers to the offset specified by expr.
                 * 
                 * TODO: implement ksh-like redirection operators
                 *       '<#pattern' and '<##pattern'.
                 * 
                 *       See the Input and Output section on
                 *       https://docs.oracle.com/cd/E36784_01/html/E36870/ksh-1.html
                 */
                if(match_filename("#((*))", path, 0, 1))
                {
                    long len = strlen(path)-5;
                    char expr[len+1];
                    strncpy(expr, path+3, len);
                    expr[len] = '\0';
                    char *p2 = arithm_expand(expr);
                    if(p2)
                    {
                        char *pend;
                        len = strtol(p2, &pend, 10);
                        if(pend == p2)
                        {
                            free(p2);
                            BACKEND_RAISE_ERROR(FAILED_REDIRECT, "invalid file offset", expr);
                            EXIT_IF_NONINTERACTIVE();
                            continue;
                        }
                        free(p2);
                        /* now seek to the given offset */
                        if(lseek(i, len, SEEK_SET) < 0)
                        {
                            BACKEND_RAISE_ERROR(FAILED_REDIRECT, "failed to lseek file", strerror(errno));
                            EXIT_IF_NONINTERACTIVE();
                            continue;
                        }
                    }
                    else
                    {
                            BACKEND_RAISE_ERROR(FAILED_REDIRECT, "invalid file offset", expr);
                            EXIT_IF_NONINTERACTIVE();
                            continue;
                    }
                }
                
                /*
                 * 'normal' file redirection.
                 */
                int fd = open(path, io_files[i].flags, FILE_MASK);
                if(fd < 0)
                {
                    //errno = 0;
                    if((fd = open_special(path, io_files[i].flags)) < 0)
                    {
                        BACKEND_RAISE_ERROR(FAILED_TO_OPEN_FILE, io_files[i].path, strerror(errno));
                        EXIT_IF_NONINTERACTIVE();
                        free(path);
                        continue;
                    }
                }

                if(j <= 2 && do_savestd)
                {
                    save_std(j);
                }
                if(fd != j)
                {
                    //close(j);
                    dup2(fd, j);
                    close(fd);
                }
                free(path);
            }
        }
        else if(io_files[i].duplicates >= 0)
        {
            if(io_files[i].flags == C_FLAG)
            {
                io_files[i].flags = W_FLAG;
            }
            int fd2    = io_files[i].duplicates;
            int flags1 = io_files[i].flags;
            int flags2 = fcntl(fd2, F_GETFL);
            /* special flag for heredocs */
            if(flags1 != (int)CLOOPEN)
            {
                if(flag_set(flags1, W_FLAG) || flag_set(flags1, A_FLAG))
                {
                    if(!flag_set(flags2, O_WRONLY) && !flag_set(flags2, O_RDWR))
                    {
                        BACKEND_RAISE_ERROR(FAILED_REDIRECT, NULL, NULL);
                        EXIT_IF_NONINTERACTIVE();
                        continue;
                    }
                }
                if((flags1 & R_FLAG))
                {
                    if(!flag_set(flags2, O_RDONLY) && !flag_set(flags2, O_RDWR))
                    {
                        BACKEND_RAISE_ERROR(FAILED_REDIRECT, NULL, NULL);
                        EXIT_IF_NONINTERACTIVE();
                        continue;
                    }
                }
            }
// _dup:
            if(j <= 2 && do_savestd)
            {
                save_std(j);
            }
            dup2(io_files[i].duplicates, j);
            if(flag_set(io_files[i].flags, CLOOPEN))
            {
                close(io_files[i].duplicates);
            }
        }
    } /* end for */
    return 1;
}


/*
 * prepare a redirection list and then execute the redirections.
 */
int redirect_do(struct node_s *redirect_list)
{
    if(!redirect_list)
    {
        return 1;
    }
    struct io_file_s io_files[FOPEN_MAX];
    if(redirect_prep(redirect_list, io_files) == -1)
    {
        return 0;
    }
    if(!__redirect_do(io_files, 1))
    {
        return 0;
    }
    do_restore_std = 0;
    return 1;
}


/*
 * restore the standard streams if they have been redirected.
 */
void redirect_restore()
{
    do_restore_std = 1;
    restore_std();
}


/*
 * prepare an I/O redirection for a file.
 */
int  do_io_file(struct node_s *node, struct io_file_s *io_file)
{
    int fileno     = -1;
    int duplicates = 0;
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }
    char *str = child->val.str;
    char buf[32];

    /* r-shells can't redirect output */
    if(startup_finished && option_set('r'))
    {
        char c = node->val.chr;
        if(c == IO_FILE_LESSGREAT || c == IO_FILE_CLOBBER || c == IO_FILE_GREAT ||
           c == IO_FILE_GREATAND  || c == IO_FILE_DGREAT  || c == IO_FILE_AND_GREAT_GREAT)
        {
            /*
             * NOTE: consequences of failed redirection are handled by the caller, 
             *       i.e. do_simple_command().
             */
            fprintf(stderr," %s: restricted shells can't redirect output\n", SHELL_NAME);
            return 0;
        }
    }
    
    switch(node->val.chr)
    {
        case IO_FILE_LESS     :
            io_file->flags = R_FLAG;
            break;
            
        case IO_FILE_LESSAND  :
            duplicates = 1;
            io_file->flags = R_FLAG;
            break;
            
        case IO_FILE_LESSGREAT:
            io_file->flags = R_FLAG|W_FLAG;
            break;
            
        case IO_FILE_CLOBBER  :
            io_file->flags = C_FLAG;
            break;
            
        case IO_FILE_GREAT    :
            io_file->flags = W_FLAG;
            break;
            
        case IO_FILE_GREATAND :
            duplicates = 1;
            io_file->flags = W_FLAG;
            break;
            
        case IO_FILE_AND_GREAT_GREAT:
            duplicates = 1;
            io_file->flags = A_FLAG;
            break;
            
        case IO_FILE_DGREAT   :
            io_file->flags = A_FLAG;
            break;
    }

    if(duplicates && strcmp(str, "-"))
    {
        /* I/O from coprocess */
        if(strcmp(str, "p-") == 0 || strcmp(str, "p") == 0)
        {
            switch(node->val.chr)
            {
                case IO_FILE_LESSAND  :
                    if(wfiledes[0] == -1)
                    {
                        goto invalid;
                    }
                    fileno = wfiledes[0];
                    break;
                    
                case IO_FILE_GREATAND :
                    if(rfiledes[1] == -1)
                    {
                        goto invalid;
                    }
                    fileno = rfiledes[1];
                    break;
                    
                default:
                    goto invalid;
            }
        }
        else
        {
            char *str2 = NULL;
            /* get the file number from the shell variable in the >{$var} type of redirection */
            if(str[0] == '$')
            {
                str2 = word_expand_to_str(str);
                if(str2)
                {
                    str = get_malloced_str(str2);
                    if(!str)
                    {
                        return 0;
                    }
                    free(str2);
                }
            }
            char *strend;
            fileno = strtol(str, &strend, 10);
            if(strend == str)
            {
                io_file->duplicates = -1;
                io_file->path       = str;
                return 1;
            }
        }
        if(fileno < 0 || fileno >= FOPEN_MAX)
        {
            goto invalid;
        }
        /* >&n- and <&n-, but don't close coproc files */
        if(str[strlen(str)-1] == '-' && strcmp(str, "p-"))
        {
            io_file->flags |= CLOOPEN;
        }
        io_file->duplicates = fileno;
        io_file->path       = NULL;
    }
    else
    {
        io_file->duplicates = -1;
        io_file->path       = str;
    }
    
    return 1;
    
invalid:
    sprintf(buf, "%d", fileno);
    BACKEND_RAISE_ERROR(INVALID_REDIRECT_FILENO, buf, NULL);
    /*
     * NOTE: consequences of failed redirection are handled by the caller, 
     *       i.e. do_simple_command().
     */
    return 0;
}


/*
 * prepare an I/O redirection for a here document.
 */
int  do_io_here(struct node_s *node, struct io_file_s *io_file)
{
    struct node_s *child = node->first_child;
    if(!child)
    {
        return 0;
    }
    char *heredoc = child->val.str;
    FILE *tmp = tmpfile();
    if(!tmp)
    {
        return 0;
    }
    if(node->val.chr == IO_HERE_EXPAND)
    {
        struct word_s *head = word_expand(heredoc);
        struct word_s *w = head;
        while(w)
        {
            fprintf(tmp, "%s", w->data);
            w = w->next;
        }
        free_all_words(head);
    }
    else
    {
        fprintf(tmp, "%s", heredoc);
    }
    int fd = fileno(tmp);
    rewind(tmp);
    io_file->duplicates = fd;
    io_file->path       = NULL;
    io_file->flags      = CLOOPEN;
    return 1;
}


/*
 * prepare an I/O redirection for a file or a here document by calling the
 * appropriate delegate function to handle the redirection.
 */
int  do_io_redirect(struct node_s *node, struct io_file_s *io_file)
{
    struct node_s *io = node->first_child;
    if(io->type == NODE_IO_FILE)
    {
        return do_io_file(io, io_file);
    }
    return do_io_here(io, io_file);
}
