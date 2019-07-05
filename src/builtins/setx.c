/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2019 (c)
 * 
 *    file: setx.c
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

#include <strings.h>
#include "../cmd.h"
#include "setx.h"
#include "../debug.h"

#define UTILITY     "setx"

/*
 * this utility is used to set/unset different shell options, which are deemed
 * extra or optional, i.e. non-POSIX extensions. you will notice most of the list
 * contains bash-like options (which you can check in the bash manual, section 4.3.2
 * The Shopt Builtin), while a minority of options are borrowed from tcsh features.
 * in fact, setx is almost identical to bash's shopt, we added an alias shopt='setx'
 * to alias.c. you can extend the shell's capabilities by adding more options
 * to this list, but the maximum is sixty-four, as we are saving the options in
 * a bitmap. some options include underscores in their names. we added those,
 * in addition to another version of each where underscores were substituted by
 * dashes (it just feels more natural) to achieve the same effect. this is only a
 * convenience feature, it doesn't add any extra functionality to the shell.
 */
__int64_t optionsx = 0;

struct optionx_s
{
    char      *name;
    __int64_t  val ;
}
optionx_list[] =
{
    { "addsuffix"                   , OPTION_ADD_SUFFIX           },    /* similar to setting tcsh addsuffix variable */
    { "autocd"                      , OPTION_AUTO_CD              },
    { "cdable_vars"                 , OPTION_CDABLE_VARS          },
    { "cdable-vars"                 , OPTION_CDABLE_VARS          },
    { "checkhash"                   , OPTION_CHECK_HASH           },
    { "checkjobs"                   , OPTION_CHECK_JOBS           },
    { "checkwinsize"                , OPTION_CHECK_WINSIZE        },
    { "clearscreen"                 , OPTION_CLEAR_SCREEN         },    /* our extension to clear the screen on startup */
    { "cmdhist"                     , OPTION_CMD_HIST             },
    { "complete_fullquote"          , OPTION_COMPLETE_FULL_QUOTE  },
    { "complete-fullquote"          , OPTION_COMPLETE_FULL_QUOTE  },
    { "dextract"                    , OPTION_DEXTRACT             },    /* similar to setting tcsh dextract variable */
    { "dotglob"                     , OPTION_DOT_GLOB             },
    { "dunique"                     , OPTION_DUNIQUE              },    /* similar to setting tcsh dunique variable */
    { "execfail"                    , OPTION_EXEC_FAIL            },
    { "expand_aliases"              , OPTION_EXPAND_ALIASES       },
    { "expand-aliases"              , OPTION_EXPAND_ALIASES       },
    { "extglob"                     , OPTION_EXT_GLOB             },
    { "failglob"                    , OPTION_FAIL_GLOB            },
    { "force_fignore"               , OPTION_FORCE_FIGNORE        },
    { "force-fignore"               , OPTION_FORCE_FIGNORE        },
    { "globasciiranges"             , OPTION_GLOB_ASCII_RANGES    },
    { "histappend"                  , OPTION_HIST_APPEND          },    
    { "histreedit"                  , OPTION_HIST_RE_EDIT         },
    { "histverify"                  , OPTION_HIST_VERIFY          },
    { "hostcomplete"                , OPTION_HOST_COMPLETE        },    
    { "huponexit"                   , OPTION_HUP_ON_EXIT          },
    { "inherit_errexit"             , OPTION_INHERIT_ERREXIT      },
    { "inherit-errexit"             , OPTION_INHERIT_ERREXIT      },
    { "interactive_comments"        , OPTION_INTERACTIVE_COMMENTS },
    { "interactive-comments"        , OPTION_INTERACTIVE_COMMENTS },
    { "lastpipe"                    , OPTION_LAST_PIPE            },
    { "lithist"                     , OPTION_LIT_HIST             },
    { "listjobs"                    , OPTION_LIST_JOBS            },    /* similar to setting tcsh listjobs variable */
    { "listjobs_long"               , OPTION_LIST_JOBS_LONG       },    /* similar to setting tcsh listjobs variable to 'long' */
    { "listjobs-long"               , OPTION_LIST_JOBS_LONG       },    /* as above */
    { "localvar_inherit"            , OPTION_LOCAL_VAR_INHERIT    },
    { "localvar-inherit"            , OPTION_LOCAL_VAR_INHERIT    },
    { "localvar_unset"              , OPTION_LOCAL_VAR_UNSET      },
    { "localvar-unset"              , OPTION_LOCAL_VAR_UNSET      },
    { "login_shell"                 , OPTION_LOGIN_SHELL          },
    { "login-shell"                 , OPTION_LOGIN_SHELL          },
    { "mailwarn"                    , OPTION_MAIL_WARN            },
    { "nocasematch"                 , OPTION_NOCASE_MATCH         },
    { "nullglob"                    , OPTION_NULL_GLOB            },
    { "nocaseglob"                  , OPTION_NOCASE_GLOB          },
    { "printexitvalue"              , OPTION_PRINT_EXIT_VALUE     },    /* similar to setting tcsh printexitvalue variable */
    
    /*
     * TODO: these two are currently not implemented
     */
    { "progcomp"            , },
    { "progcomp_alias"      , },
    
    { "promptvars"                  , OPTION_PROMPT_VARS          },
    { "pushdtohome"                 , OPTION_PUSHD_TO_HOME        },    /* similar to setting tcsh pushdtohome variable */
    { "recognize_only_executables"  , OPTION_RECOGNIZE_ONLY_EXE   },    /* similar to setting tcsh 
                                                                         recognize_only_executables variable */
    { "recognize-only-executables"  , OPTION_RECOGNIZE_ONLY_EXE   },
    { "restricted_shell"            , OPTION_RESTRICTED_SHELL     },
    { "restricted-shell"            , OPTION_RESTRICTED_SHELL     },
    { "savedirs"                    , OPTION_SAVE_DIRS            },    /* similar to setting tcsh savedirs variable */
    { "savehist"                    , OPTION_SAVE_HIST            },    /* similar to setting tcsh savehist variable */
    { "shift_verbose"               , OPTION_SHIFT_VERBOSE        },
    { "shift-verbose"               , OPTION_SHIFT_VERBOSE        },
    { "sourcepath"                  , OPTION_SOURCE_PATH          },
    { "usercomplete"                , OPTION_USER_COMPLETE        },    /* our extension to auto-complete user names.
                                                                   similar to bash's hostcomplete */
    { "xpg_echo"                    , OPTION_XPG_ECHO             },
    { "xpg-echo"                    , OPTION_XPG_ECHO             },
};

int optionx_count = sizeof(optionx_list)/sizeof(struct optionx_s);


int set_optionx(__int64_t op, int onoff)
{
    if(onoff) optionsx |= op;
    else      optionsx &= ~op;
    return 1;
}


__int64_t optionx_index(char *opname)
{
    int i;
    for(i = 0; i < optionx_count; i++)
    {
        if(strcasecmp(opname, optionx_list[i].name) == 0)
            return optionx_list[i].val;
    }
    return -1;
}


void purge_xoptions(char which, int formal)
{
    int i;
    for(i = 0; i < optionx_count; i++)
    {
        int isset = optionx_set(optionx_list[i].val);
        if((which == 's' && !isset) || (which == 'u' && isset)) continue;
        /* options with '-' in their names are duplicates for those with '_' in their names */
        if(strchr(optionx_list[i].name, '-')) continue;
        if(formal)
        {
            printf("setx -%c %s\n",  isset ? 's' : 'u', optionx_list[i].name);
        }
        else
        {
            printf("%s%*s\t%s\n", optionx_list[i].name,
                   (int)(24-strlen(optionx_list[i].name)), " ",
                   isset ? "on": "off");
        }
    }
}


int setx(int argc, char **argv)
{
    /****************************
     * process the arguments
     ****************************/
    int enable = 0, disable = 0, quiet = 0, setonly = 0, formal = 0;
    int v = 1, c;
    int res = 0;
    set_shell_varp("OPTIND", NULL);
    argi = 0;   /* args.c */
    while((c = parse_args(argc, argv, "hvpsuqo", &v, 1)) > 0)
    {
        switch(c)
        {
            case 'h':
                print_help(argv[0], SPECIAL_BUILTIN_SETX, 0, 0);
                return 0;
                
            case 'v':
                printf("%s", shell_ver);
                return 0;
                
            case 'p':
                formal = 1;
                break;
                
            case 's':
                enable = 1;
                break;
                
            case 'u':
                disable = 1;
                break;
                
            case 'q':
                quiet = 1;
                break;
                
            case 'o':
                setonly = 1;
                break;
        }
    }
    /* unknown option */
    if(c == -1) return 2;

    if(v >= argc)
    {
        if(quiet)
        {
            /* return 0 if all options are set, non-zero otherwise */
            for(v = 0; v < optionx_count; v++)
            {
                if(!optionx_set(optionx_list[v].val)) res++;
            }
            return res;
        }
        else
        {
            purge_xoptions(enable ? 's' : disable ? 'u' : 'a', formal);
            return 0;
        }
    }
  
    /*
     * we loop on the passed options, checking if they are set.
     * return status is zero if all options are set, non-zero otherwise.
     * if -q is passed, return status reflects the number of unset options.
     * otherwise it is 2 to indicate failure.
     */
    for( ; v < argc; v++)
    {
        char *arg = argv[v];
        
        /* set-like options */
        if(setonly)
        {
            if(enable || disable)
            {
                char o[] = { enable ? '-' : '+', 'o', '\0' };
                if(do_options(o, arg) != 0)
                {
                    if(quiet) res++;
                    else      res = 2;
                }
            }
            else
            {
                char o = short_option(arg);
                if(!o)
                {
                    if(quiet) res++;
                    else
                    {
                        fprintf(stderr, "%s: invalid option: %s\n", UTILITY, arg);
                        res = 2;
                    }
                }
                else if(quiet)
                {
                    if(!option_set(o)) res++;
                }
                else if(formal)
                {
                    printf("setx -%c -o %s\n", option_set(o) ? 's' : 'u', arg);
                }
                else
                {
                    printf("%s%*s\t%s\n", arg, (int)(24-strlen(arg)), " ", option_set(o) ? "on": "off");
                }
            }
            continue;
        }
        
        /* setx extended options */
        __int64_t c2;
        if((c2 = optionx_index(arg)) < 0)
        {
            if(quiet)
            {
                res++;
                continue;
            }
            fprintf(stderr, "%s: invalid option: %s\r\n", UTILITY, arg);
            return 2;
        }
        
        if(enable || disable)
        {
            if(c2 == OPTION_LOGIN_SHELL || c2 == OPTION_RESTRICTED_SHELL)
            {
                fprintf(stderr, "%s: error setting %s: readonly option\r\n", UTILITY, arg);
                return 2;
            }

            if(!set_optionx(c2, enable ? 1 : 0))
            {
                fprintf(stderr, "%s: error setting: %s\r\n", UTILITY, arg);
                return 2;
            }
            continue;
        }
        else
        {
            int o = optionx_set(c2);
            if(!o) res++;
            if(quiet) continue;
            else if(formal)
            {
                printf("setx -%c %s\n", o ? 's' : 'u', arg);
            }
            else
            {
                printf("%s%*s\t%s\n", arg, (int)(24-strlen(arg)), " ", o ? "on": "off");
            }
        }
    }  
    return res;
}
