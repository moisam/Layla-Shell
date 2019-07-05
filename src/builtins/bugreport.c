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


int bugreport(int argc, char **argv)
{
    struct  symtab_entry_s *REPLY = add_to_symtab("REPLY");
    const  char *to = "mohammed_isam1984@yahoo.com;";
    char   *read_argv[] = { "read", "-r", NULL };
    char   *subject = NULL, *body = NULL, *from = NULL;
    int     bodysz = 0;
    struct  utsname buf;
    int     got_uname = !uname(&buf);
    int     res = 0;
    
    printf("\r\n"
           "Layla shell's bugreport utility\r\n\n"
           
           "This utility was written to help you submit your bug reports to the shell's author(s).\r\n"
           "If you noticed an unusual/erroneous behavior, survived a shell crash, or if you have any\r\n"
           "comments or ideas, we are all ears. We just request you be as detailed as you can in your\r\n"
           "description, so that we can help you reach the resolution you want. If you had a crash, please\r\n"
           "include sufficient details about the circumstances (how it happened, what command did you try\r\n"
           "to run, what other programs were running on your system, were you running the shell inside a\r\n"
           "terminal or a terminal emulator, what kind of operating system you are using, etc). The more\r\n"
           "details you give, the better we will be equipped to help you solve the issue quickly.\r\n\n"
           
           "This report will be sent to the following email address(es):\r\n"
           "    (1) Mohammed Isam (mohammed_isam1984@yahoo.com)\r\n"
           
           "\r\n"
           "The following data will be attached to the email:\r\n"
           "  * Compile time system info:\r\n"
           "      CPU architecture: %s\r\n"
           "      Operating system: %s\r\n"
           "      Compiler name: %s\r\n"
           "      Compiler version: %s\r\n"
           "\r\n"
           "  * Runtime system info:\r\n"
           , CPU_ARCH, OS_TYPE, COMPILER_TYPE, COMPILER_BUILD);
    
    if(!got_uname)
    {
        printf("      COULDN'T DETERMINE THE RUNNIG OPERATING SYSTEM\r\n"
               "      [Please include enough information about your system in the message's body below]\r\n");
    }
    else
    {
        printf("      Operating system name: %s\r\n"
               "      Operating system release: %s\r\n"
               "      Operating system version: %s\r\n"
               "      Machine type: %s\r\n"
               , buf.sysname, buf.release, buf.version, buf.machine);
    }
    printf("  * Shell version: %s\r\n", shell_ver);
    
    printf("\r\n");
    printf("Please enter the subject of your email message (an empty line will cancel this bugreport)\r\n\n");
    
    if(__read(2, read_argv) != 0)
    {
        printf("\r\n\r\nAborted\r\n\r\n");
        return 2;
    }    

    if(REPLY && REPLY->val)
    {
        if(REPLY->val[0] == '\0')
        {
            printf("\r\n\r\nAborted\r\n\r\n");
            return 2;
        }    
        subject = get_malloced_str(REPLY->val);
    }
    
    printf("\r\n");
    printf("Please enter your email address (an empty line will cancel this bugreport)\r\n\n");
    
    if(__read(2, read_argv) != 0)
    {
        printf("\r\n\r\nAborted\r\n\r\n");
        free_malloced_str(subject);
        return 2;
    }    

    if(REPLY && REPLY->val)
    {
        if(REPLY->val[0] == '\0')
        {
            printf("\r\n\r\nAborted\r\n\r\n");
            free_malloced_str(subject);
            return 2;
        }    
        from = get_malloced_str(REPLY->val);
    }
    
    printf("\r\n");
    printf("Please enter the body of your email message. Try to be as detailed as you can. You can enter\r\n"
           "multiple lines. When you are finished with writing your message, press CTRL-D to continue:\r\n\n");
    
    while(1)
    {
        symtab_entry_setval(REPLY, NULL);
        if(__read(2, read_argv) != 0)
        {
            break;
        } 

        if(REPLY && REPLY->val)
        {
            int len = strlen(REPLY->val);
            if(!body)
            {
                bodysz = 512;
                while(bodysz < len) bodysz <<= 1;
                body = malloc(bodysz);
                if(body) strcpy(body, REPLY->val);
            }
            else
            {
                while(bodysz < len) bodysz <<= 1;
                char *body2 = realloc(body, bodysz);
                if(body2)
                {
                    body = body2;
                    strcat(body, REPLY->val);
                }
            }
        }
    }
    
    if(!body)
    {
        free_malloced_str(subject);
        free_malloced_str(from);
        printf("\r\n\r\nAborted\r\n\r\n");
        return 2;
    }
    
    
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
        fprintf(mailpipe, "  * Shell version: %s\r\n", shell_ver);
        fwrite(".\n", 1, 2, mailpipe);
        pclose(mailpipe);
        res = 0;
        printf("\r\n\nYour message will be sent soon.\r\n"
               "Please check your spool folder in a few minutes to make sure the message was sent.\r\n\n"
               "Thank you for your bugreport!\r\n\n");
     }
     else
     {
        printf("\r\n\n");
        perror("Failed to invoke sendmail (do you have it installed on this computer?)");
        printf("\r\n\n");
        res = 1;
     }
    
     free_malloced_str(subject);
     free_malloced_str(from);     
     return res;
}
