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


char *fifo_filename = "FIFOCMD";


int redirect_proc_do(char *cmdline, int fd1, int fd2)
{
    pid_t pid = 0;
    if((pid = fork()) == 0)     /* child process */
    {
        asynchronous_prologue();

        //struct source_s save_src = __src;
        __src.buffer   = cmdline+1;
        cmdline[strlen(cmdline)-1] = '\0';
        __src.bufsize  = strlen(cmdline);
        __src.curpos   = -2;

        close(0);   /* stdin */
        if(fd1 >= 0)
        {
            dup2(fd1, 0);
            __src.filename = fifo_filename;
        }
        else
        {
            __src.filename = "/dev/null";
            open(__src.filename, O_RDONLY);
        }

        if(fd2 >= 0)
        {
            close(1);   /* stdout */
            dup2(fd2, 1);
        }

        do_cmd();
        //__src = save_src;
        exit(exit_status);
    }
    else if(pid < 0) return 0;
    else return 1;
}

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
            if(errno == EEXIST) continue;
            /*
             * if the system doesn't support named pipes, or another error occurred, such as
             * insufficient disk space, try performing this via a regular pipe, whose path we'll
             * pass as /dev/fd/filedes. if the system doesn't support this scheme, we won't be able
             * to perform process substitution.
             */
            int filedes[2];
            if(pipe(filedes))
            {
                char buf[16];
                sprintf(buf, "/dev/fd/%d", filedes[(op == '<') ? 0 : 1]);
                if(file_exists(buf))
                {
                    char *path = NULL;
                    if(op == '<' && redirect_proc_do(cmdline, -1, filedes[1]))
                    {
                        close(filedes[1]);
                        path = get_malloced_str(buf);
                    }
                    else if(redirect_proc_do(cmdline, filedes[0], -1))
                    {
                        close(filedes[0]);
                        path = get_malloced_str(buf);
                    }
                    else
                    {
                        close(filedes[0]);
                        close(filedes[1]);
                    }
                    return path;
                }
                else
                {
                    fprintf(stderr, "%s: error creating fifo: %s\r\n", SHELL_NAME, "system doesn't support `/dev/fd` file names");
                    close(filedes[0]);
                    close(filedes[1]);
                    return NULL;
                }
            }
            else
            {
                fprintf(stderr, "%s: error creating fifo: %s\r\n", SHELL_NAME, strerror(errno));
                return NULL;
            }
        }
        /*
         * open the fifo in read/write mode so that we won't be blocked waiting for a reader.
         */
        int fd1 = open(tmpname, O_RDWR);
        if(fd1 < 0)
        {
            fprintf(stderr, "%s: error opening fifo: %s\r\n", SHELL_NAME, strerror(errno));
            return NULL;
        }
        char *path = NULL;
        if(op == '<' && redirect_proc_do(cmdline, -1, fd1)) path = get_malloced_str(tmpname);
        else if(redirect_proc_do(cmdline, fd1, -1)) path = get_malloced_str(tmpname);
        close(fd1);
        return path;
    }
    return NULL;
}

/*
 * get the slot belonging to this fileno, or else the first
 * empty slot in the redirection table.
 */
int get_slot(int fileno, struct io_file_s *io_files)
{
    int i;
    for(i = 0; i < FOPEN_MAX; i++)
    {
        if(io_files[i].fileno == fileno || io_files[i].fileno == -1) return i;
    }
    return -1;
}

/*
 * prepare redirection file from redirection node.
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
             * check for the non-POSIX bash redirection extensions of {var}<&N
             * and {var}>&N. the {var} part would have been added as the previous
             * node.
             */
            if(s[0] == '{' && s[i-1] == '}')
            {
                s[i-1] = '\0';
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
        //fprintf(stderr, "%s: invalid redirection file number: %d\r\n", SHELL_NAME, fileno);
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


int redirect_prep(struct node_s *node, struct io_file_s *io_files)
{
    if(!io_files) return 0;
    int  i;
    int  total_redirects = 0;
    for(i = 0; i < FOPEN_MAX; i++)
    {
        io_files[i].path       = NULL;
        io_files[i].fileno     = -1;
        io_files[i].duplicates = -1;
        io_files[i].flags      =  0;
    }
    if(!node) return 0;

    struct node_s *child = node->first_child;
    /* prepare the redirections */
    while(child)
    {
        if(child->type == NODE_IO_REDIRECT)
        {
            if(redirect_prep_node(child, io_files) == -1) total_redirects = -1;
            else total_redirects++;
        }
        child = child->next_sibling;
    }
    return total_redirects;
}


int open_special(char *path, int flags, int mask)
{
    int fd = -1, i, remote = 0;
    if(strstr(path, "/dev/fd/") == path)
    {
        i = atoi(path+8);
        if(i < 0) return -1;
        fd = dup(i);
    }
    else if(strcmp(path, "/dev/stdin") == 0)
    {
        fd = dup(0);
    }
    else if(strcmp(path, "/dev/stdout") == 0)
    {
        fd = dup(1);
    }
    else if(strcmp(path, "/dev/stderr") == 0)
    {
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
        if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        {
            //fprintf(stderr, "%s: error connecting to socket: %s\n", SHELL_NAME, strerror(errno));
            return -1;
        }
    }
    return fd;
}


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
                if(j <= 2 && do_savestd) save_std(j);
                close(i);
                /* POSIX says we can open an "unspecified file" in this case */
                if(j == 0) open("/dev/null", O_RDONLY);
                if(j == 1 || j == 2) open("/dev/null", O_WRONLY);
            }
            else if(path[0] != '\0')
            {
                path = word_expand_to_str(path);
                if(!path) continue;

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
                if(io_files[i].flags == C_FLAG) io_files[i].flags = W_FLAG;
                
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
                    char *p2 = __do_arithmetic(expr);
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
                    if((fd = open_special(path, io_files[i].flags, FILE_MASK)) < 0)
                    {
                        BACKEND_RAISE_ERROR(FAILED_TO_OPEN_FILE, io_files[i].path, strerror(errno));
                        EXIT_IF_NONINTERACTIVE();
                        free(path);
                        continue;
                    }
                }

                if(j <= 2 && do_savestd) save_std(j);
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
            //if(i <= 2) save_std(i);
            if(io_files[i].flags == C_FLAG) io_files[i].flags = W_FLAG;
            //int fd1    = i;
            int fd2    = io_files[i].duplicates;
            int flags1 = io_files[i].flags;
            int flags2 = fcntl(fd2, F_GETFL);
            /* special flag for heredocs */
            if(flags1 == (int)CLOOPEN) goto _dup;
            if((flags1 & W_FLAG) || (flags1 & A_FLAG))
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
_dup:
            if(j <= 2 && do_savestd) save_std(j);
            dup2(io_files[i].duplicates, j);
            if(flag_set(io_files[i].flags, CLOOPEN))
            {
                close(io_files[i].duplicates);
            }
        }
        /*
        switch(j)
        {
            case 0: STDIN  = fdopen(j, "r"); break;
            case 1: STDOUT = fdopen(j, "w"); break;
            case 2: STDERR = fdopen(j, "w"); break;
        }
        */
    } /* end for */
    return 1;
}


int redirect_do(struct node_s *redirect_list)
{
    if(!redirect_list) return 1;
    struct io_file_s io_files[FOPEN_MAX];
    if(redirect_prep(redirect_list, io_files) == -1) return 0;
    if(!__redirect_do(io_files, 1)) return 0;
    do_restore_std = 0;
    return 1;
}

void redirect_restore()
{
    do_restore_std = 1;
    restore_std();
}
