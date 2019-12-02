/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: bugreport.c
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

/* required macro definition for popen() and pclose() */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <sys/utsname.h>
#include "../cmd.h"
#include "../cpu.h"
#include "../ostype.h"
#include "../comptype.h"
#include "../symtab/symtab.h"

#define UTILITY             "bugreport"


/*
 * this utility is a non-POSIX extension, similar to bashbug in bash. it is used
 * to send bugreports to the shell's author(s), and depends on a properly set and
 * working sendmail program in the system on which lsh is running. the tool is
 * interactive, following a step-wise approach to help the user fill in the bugreport.
 * bash implements bashbug as a shell script that invokes vi. this makes sense, but
 * I think it is better to implement this tool as an integral part of the shell, so
 * that we can collect the information we need reliably (such as the machine type
 * and shell version), without relying on the shell/environment variables which might
 * be changed by the user, filling the report with wrong and unhelpful info (but again,
 * the user can fill whatever they want in the bugreport in all cases).
 */
int bugreport_builtin(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    struct  symtab_entry_s *REPLY = add_to_symtab("REPLY");
    const  char *to = "mohammed_isam1984@yahoo.com;";
    char   *read_argv[] = { "read", "-r", NULL };
    char   *subject = NULL, *body = NULL, *from = NULL;
    int     bodysz = 0;
    struct  utsname buf;
    /* get the kernel's uname info */
    int     got_uname = !uname(&buf);
    int     res = 0;
    
    printf("\n"
           "Layla shell's bugreport utility\n\n"
           
           "This utility was written to help you submit your bug reports to the shell's author(s).\n"
           "If you noticed an unusual/erroneous behavior, survived a shell crash, or if you have any\n"
           "comments or ideas, we are all ears. We just request you be as detailed as you can in your\n"
           "description, so that we can help you reach the resolution you want. If you had a crash, please\n"
           "include sufficient details about the circumstances (how it happened, what command did you try\n"
           "to run, what other programs were running on your system, were you running the shell inside a\n"
           "terminal or a terminal emulator, what kind of operating system you are using, etc). The more\n"
           "details you give, the better we will be equipped to help you solve the issue quickly.\n\n"
           
           "This report will be sent to the following email address(es):\n"
           "    (1) Mohammed Isam (mohammed_isam1984@yahoo.com)\n"
           
           "\n"
           "The following data will be attached to the email:\n"
           "  * Compile time system info:\n"
           "      CPU architecture: %s\n"
           "      Operating system: %s\n"
           "      Compiler name: %s\n"
           "      Compiler version: %s\n"
           "\n"
           "  * Runtime system info:\n"
           , CPU_ARCH, OS_TYPE, COMPILER_TYPE, COMPILER_BUILD);
    
    if(!got_uname)
    {
        printf("      COULDN'T DETERMINE THE RUNNIG OPERATING SYSTEM\n"
               "      [Please include enough information about your system in the message's body below]\n");
    }
    else
    {
        printf("      Operating system name: %s\n"
               "      Operating system release: %s\n"
               "      Operating system version: %s\n"
               "      Machine type: %s\n"
               , buf.sysname, buf.release, buf.version, buf.machine);
    }
    printf("  * Shell version: %s\n", shell_ver);
    printf("\n");
    
    /* get the email subject */
    printf("Please enter the subject of your email message (an empty line will cancel this bugreport)\n\n");
    /* abort if error reading input */
    if(read_builtin(2, read_argv) != 0)
    {
        printf("\n\nAborted\n\n");
        return 2;
    }    
    if(REPLY && REPLY->val)
    {
        /* abort if input is empty */
        if(REPLY->val[0] == '\0')
        {
            printf("\n\nAborted\n\n");
            return 2;
        }
        /* save the email subject */
        subject = get_malloced_str(REPLY->val);
    }
    printf("\n");

    /* get the user's email address */
    printf("Please enter your email address (an empty line will cancel this bugreport)\n\n");
    /* abort if error reading input */
    if(read_builtin(2, read_argv) != 0)
    {
        printf("\n\nAborted\n\n");
        /* discard saved subject */
        free_malloced_str(subject);
        return 2;
    }    
    if(REPLY && REPLY->val)
    {
        /* abort if input is empty */
        if(REPLY->val[0] == '\0')
        {
            printf("\n\nAborted\n\n");
            /* discard saved subject */
            free_malloced_str(subject);
            return 2;
        }    
        /* save the email address */
        from = get_malloced_str(REPLY->val);
    }
    printf("\n");
    
    /* get the message body */
    printf("Please enter the body of your email message. Try to be as detailed as you can. You can enter\n"
           "multiple lines. When you are finished with writing your message, press CTRL-D to continue:\n\n");
    
    /* keep reading input, until we hit EOF */
    while(1)
    {
        symtab_entry_setval(REPLY, NULL);
        /* stop if error reading input */
        if(read_builtin(2, read_argv) != 0)
        {
            break;
        } 
        /* we've got input */
        if(REPLY && REPLY->val)
        {
            int len = strlen(REPLY->val);
            /*
             * first line in the message body. determine how much memory we need by
             * starting at 512 bytes, doubling it up until we have a number large enough
             * to store the message body, then we alloc that much memory and store our
             * input there.
             */
            if(!body)
            {
                bodysz = 512;
                while(bodysz < len)
                {
                    bodysz <<= 1;
                }
                body = malloc(bodysz);
                if(body)
                {
                    strcpy(body, REPLY->val);
                }
            }
            else
            {
                /*
                 * second and following lines. extend the memory storage we have by doubling
                 * the size until we have enough to save the old body in addition to the new
                 * line. after realloc'ing the memory, concatenate the new line to the old
                 * message body.
                 */
                while(bodysz < len)
                {
                    bodysz <<= 1;
                }
                char *body2 = realloc(body, bodysz);
                if(body2)
                {
                    body = body2;
                    strcat(body, REPLY->val);
                }
            }
        }
    }
    
    /* abort if message body is empty */
    if(!body)
    {
        /* discard saved subject and email address */
        free_malloced_str(subject);
        free_malloced_str(from);
        printf("\n\nAborted\n\n");
        return 2;
    }
    
    /* send the email via sendmail (see `man sendmail` for the message structure) */
    FILE *mailpipe = popen("/usr/lib/sendmail -t", "w");
    if(mailpipe != NULL)
    {
        fprintf(mailpipe, "To: %s\n", to);
        fprintf(mailpipe, "From: %s\n", from);
        fprintf(mailpipe, "Subject: %s\n\n", subject);
        fwrite(body, 1, strlen(body), mailpipe);
        fprintf(mailpipe,
            "  * Compile time system info:\n"
            "    CPU architecture: %s\n"
            "    Operating system: %s\n"
            "    Compiler name: %s\n"
            "    Compiler version: %s\n"
            "\n"
            "  * Runtime system info:\n"
            , CPU_ARCH, OS_TYPE, COMPILER_TYPE, COMPILER_BUILD);
        if(got_uname)
        {
            fprintf(mailpipe,
                "    Operating system name: %s\n"
                "    Operating system release: %s\n"
                "    Operating system version: %s\n"
                "    Machine type: %s\n"
                , buf.sysname, buf.release, buf.version, buf.machine);
        }
        else
        {
            fprintf(mailpipe,
                "    COULDN'T DETERMINE THE RUNNIG OPERATING SYSTEM\n");
        }
        fprintf(mailpipe, "  * Shell version: %s\n", shell_ver);
        fwrite(".\n", 1, 2, mailpipe);
        pclose(mailpipe);
        res = 0;
        printf("\n\nYour message will be sent soon.\n"
               "Please check your spool folder in a few minutes to make sure the message was sent.\n\n"
               "Thank you for your bugreport!\n\n");
     }
     else
     {
        printf("\n\n");
        perror("Failed to invoke sendmail (do you have it installed on this computer?)");
        printf("\n\n");
        res = 1;
     }
    
    /* discard saved subject, email address and body */
     free_malloced_str(subject);
     free_malloced_str(from);
     free(body);
     return res;
}
