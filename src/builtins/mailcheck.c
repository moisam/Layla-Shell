/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: mailcheck.c
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "../cmd.h"
#include "setx.h"
#include "../symtab/symtab.h"
#include "../debug.h"

#define UTILITY     "mail"

/* the standard message we print when there is unread mail */
#define STDMSG      "You have mail in %s\n"

/* time of the last mail check */
time_t last_check = 0;

/*
 * check for unread mail.
 */
int check_for_mail(void)
{
    /* get the time interval checking variable */
    struct symtab_entry_s *entry = get_symtab_entry("MAILCHECK");
    if(!entry)
    {
        entry = add_to_symtab("MAILCHECK");
        if(!entry)
        {
            fprintf(stderr, "%s: cannot check mail: MAILCHECK is not set\n", UTILITY);
            return 2;
        }
        /* ksh defaults to 600 secs, while bash uses 60 secs */
        symtab_entry_setval(entry, "600");
    }
    int secs = atoi(entry->val);
    /* zero or negative $MAILCHECK value. don't check for mail */
    if(secs <= 0)
    {
        return 0;
    }
    /* get the current time */
    time_t now = time(NULL);

    /* $MAILCHECK secs have passed. time to check for mail */
    if(difftime(now, last_check) >= secs)
    {
        return 1;
    }

    /* don't check for mail yet */
    return 0;
}


/*
 * the mail builtin utility (non-POSIX).. used to check for mail.
 *
 * returns 0 on success, non-zero otherwise.
 *
 * see the manpage for the list of options and an explanation of what each option does.
 * you can also run: `help mail` or `mail -h` from lsh prompt to see a short
 * explanation on how to use this utility.
 */

int mailcheck_builtin(int argc, char **argv)
{
    int v = 1, c;
    int quiet = 0;
    set_shell_varp("OPTIND", NULL);     /* reset $OPTIND */
    argi = 0;   /* defined in args.c */
    /****************************
     * process the options
     ****************************/
    while((c = parse_args(argc, argv, "hvq", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], REGULAR_BUILTIN_MAIL, 1, 0);
                break;

            case 'v':
                printf("%s", shell_ver);
                break;
                
             /* run in quiet mode (don't output message if there is no mail) */
            case 'q':
                quiet = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1)
    {
        return 1;
    }

    struct symtab_entry_s *entry;
    /* get the user name (the easy way) */
    char *user = get_shell_varp("USER", "you");
    
    /* get the mail file(s) path */
    char *p = get_shell_varp("MAILPATH", NULL);
    if(!p)
    {
        p = get_shell_varp("MAIL", NULL);
    }
    if(!p)
    {
        /* output message if not in quiet mode */
        if(!quiet)
        {
            fprintf(stderr, "%s: cannot check mail: you have to set MAIL or MAILPATH\n", UTILITY);
        }
        return 2;
    }

    char *p2;
    int gotmail = 0;

    /* we will set $_ to the name of the mail file we are checking */
    entry = get_symtab_entry("_");

    /* record the time of mail check */
    last_check = time(NULL);

    /* check for unread mail */
check:
    /* path finished */
    if(!p || *p == '\0')
    {
        /* output message if not in quiet mode */
        if(!gotmail && !quiet)
        {
            printf("No mail for %s\n", user);
        }
        return 0;
    }
    /* get the next path entry */
    p2 = p;
    while(*p2 && *p2 != ':')
    {
        p2++;
    }
    int  plen = p2-p;
    if(!plen)
    {
        plen = 1;
    }
    /* copy the path */
    char path[plen+1];
    strncpy(path, p, p2-p);
    path[p2-p] = '\0';
    if(p2[-1] != '/')
    {
        strcat(path, "/");
    }
    /* does the path entry contain a message to print? */
    char *msg = NULL;
    if((msg = strchr(path, '?')))
    {
        *msg = '\0';
        msg++;
    }
    /* now check the file */
    struct stat st;
    if(stat(path, &st) == 0)
    {
        /*
         * if the mailpath is a directory, tcsh reports each file in the directory
         * in a separate message.
         * 
         * TODO: implement this functionality.
         */
        if(S_ISREG(st.st_mode))
        {
            if(st.st_size > 0 && difftime(st.st_mtime, last_check) > 0)
            {
                /* do we have a custom message? */
                if(msg)
                {
                    /* set $_ to the current pathname */
                    if(entry)
                    {
                        symtab_entry_setval(entry, path);
                    }
                    char *m = word_expand_to_str(msg);
                    if(m)
                    {
                        printf(m, path);
                        free(m);
                    }
                    else
                    {
                        printf(STDMSG, path);
                    }
                }
                /* no. use our standard message */
                else
                {
                    printf(STDMSG, path);
                }
                gotmail = 1;
            }
            /* bash extension */
            if(optionx_set(OPTION_MAIL_WARN) && difftime(st.st_atime, last_check) > 0)
            {
                printf("The mail in %s has been read\n", path);
            }
        }
    }
    /* restore the '?' letter */
    if(msg)
    {
        msg[-1] = '?';
    }
    p = p2;
    /* check the next path entry */
    if(*p2 == ':')
    {
        p++;
    }
    goto check;
}
