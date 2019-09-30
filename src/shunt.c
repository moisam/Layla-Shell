/*
 * Copyright (c) 2012 the authors listed at the following URL, and/or
 * the authors of referenced articles or incorporated external code:
 * http://en.literateprograms.org/Shunting_yard_algorithm_(C)?action=history&offset=20080201043325
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *  
 * Retrieved from: http://en.literateprograms.org/Shunting_yard_algorithm_(C)?oldid=12454
 * 
 * 
 * Copyright (c) 2019 Mohammed Isam [mohammed_isam1984@yahoo.com]
 * 
 * Extensive modifications have been applied to this file to include most of the C
 * language operators and to make this file usable as part of the Layla shell.
 * Please compare with the original file at the above link to see the differences.
 * 
 * UPDATE: the original file doesn't seem to be available anymore, but the archived
 * version can be accessed from here:
 * https://web.archive.org/web/20110718214204/http://en.literateprograms.org/Shunting_yard_algorithm_(C)
 * 
 * For more information, see: https://en.wikipedia.org/wiki/Shunting-yard_algorithm
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cmd.h"
#include "symtab/symtab.h"
#include "debug.h"

#define MAXOPSTACK          64
#define MAXNUMSTACK         64
#define MAXBASE             36

struct stack_item_s
{
#define ITEM_LONG_INT       1
#define ITEM_VAR_PTR        2
    int  type;
    union
    {
        long val;
        struct symtab_entry_s *ptr;
    };
};

struct op_s *opstack[MAXOPSTACK];
int    nopstack  = 0;

//int numstack[MAXNUMSTACK];
struct stack_item_s numstack[MAXNUMSTACK];
int    nnumstack = 0;
int    error     = 0;


/*
 * extract the value of the hex digit c.
 */
char get_xdigit(char c)
{
    if(c >= '0' && c <= '9')
    {
        c -= '0';
    }
    else if(c >= 'a' && c <= 'z')
    {
        c = c-'a'+10;
    }
    else
    {
        c = c-'A'+10;
    }
    return c;
}


long long_value(struct stack_item_s *a)
{
    if(a->type == ITEM_LONG_INT)
    {
        return a->val;
    }
    else if(a->type == ITEM_VAR_PTR)
    {
        if(a->ptr->val)
        {
            return atol(a->ptr->val);
        }
    }
    return 0;
}

long eval_uminus (struct stack_item_s *a1, struct stack_item_s *a2 __attribute__ ((unused)) )
{
    return -long_value(a1);
}

long eval_uplus  (struct stack_item_s *a1, struct stack_item_s *a2 __attribute__ ((unused)) )
{
    return  long_value(a1);
}

long eval_lognot(struct stack_item_s *a1, struct stack_item_s *a2 __attribute__ ((unused)) )
{
    return !long_value(a1);
}

long eval_bitnot(struct stack_item_s *a1, struct stack_item_s *a2 __attribute__ ((unused)) )
{
    return ~long_value(a1);
}

long eval_mult(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) * long_value(a2);
}

long eval_add(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) + long_value(a2);
}

long eval_sub(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) - long_value(a2);
}

long eval_lsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) << long_value(a2);
}

long eval_rsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) >> long_value(a2);
}

long eval_lt(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) < long_value(a2);
}

long eval_le(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) <= long_value(a2);
}

long eval_gt(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) > long_value(a2);
}

long eval_ge(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) >= long_value(a2);
}

long eval_eq(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) == long_value(a2);
}

long eval_ne(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) != long_value(a2);
}

long eval_bitand(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) & long_value(a2);
}

long eval_bitxor(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) ^ long_value(a2);
}

long eval_bitor(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) | long_value(a2);
}

long eval_logand(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) && long_value(a2);
}

long eval_logor(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) || long_value(a2);
}

long __eval_exp(long a1, long a2)
{
    return a2 < 0 ? 0 : (a2 == 0 ? 1 : a1 * __eval_exp(a1, a2-1));
}

long eval_exp(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_exp(long_value(a1), long_value(a2));
}

long eval_div(struct stack_item_s *a1, struct stack_item_s *a2) 
{
    error = 0;
    long n2 = long_value(a2);
    if(!n2)
    {
        fprintf(stderr, "%s: Division by zero\n", SHELL_NAME);
        return error = 1, 0;
    }
    return long_value(a1) / n2;
}

long eval_mod(struct stack_item_s *a1, struct stack_item_s *a2) 
{
    error = 0;
    long n2 = long_value(a2);
    if(!n2)
    {
        fprintf(stderr, "%s: Division by zero\n", SHELL_NAME);
        return error = 1, 0;
    }
    return long_value(a1) % n2;
}

long eval_assign(struct stack_item_s *a1, struct stack_item_s *a2)
{
    long val = long_value(a2);
    if(a1->type == ITEM_VAR_PTR)
    {
        char buf[16];
        sprintf(buf, "%ld", val);
        symtab_entry_setval(a1->ptr, buf);
    }
    return val;
}

long __eval_assign_ext(long (*f)(struct stack_item_s *a1, struct stack_item_s *a2),
            struct stack_item_s *a1, struct stack_item_s *a2)
{
    long val = f(a1, a2);
    if(a1->type == ITEM_VAR_PTR)
    {
        char buf[16];
        sprintf(buf, "%ld", val);
        symtab_entry_setval(a1->ptr, buf);
    }
    return val;
}

long eval_assign_add(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_add, a1, a2);
}

long eval_assign_sub(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_sub, a1, a2);
}

long eval_assign_mult(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_mult, a1, a2);
}

long eval_assign_div(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_div, a1, a2);
}

long eval_assign_mod(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_mod, a1, a2);
}

long eval_assign_lsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_lsh, a1, a2);
}

long eval_assign_rsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_rsh, a1, a2);
}

long eval_assign_and(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_bitand, a1, a2);
}

long eval_assign_xor(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_bitxor, a1, a2);
}

long eval_assign_or(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return __eval_assign_ext(eval_bitor, a1, a2);
}

long __eval_inc_dec(int pre, int add, struct stack_item_s *a1)
{
    long val = long_value(a1);
    char buf[16];
    if(pre)
    {
        if(add)
        {
            val++;
        }
        else
        {
            val--;
        }
        
        if(a1->type == ITEM_VAR_PTR)
        {
            sprintf(buf, "%ld", val);
            symtab_entry_setval(a1->ptr, buf);
        }
    }
    else
    {
        int diff = add ? 1 : -1;
        if(a1->type == ITEM_VAR_PTR)
        {
            sprintf(buf, "%ld", val+diff);
            symtab_entry_setval(a1->ptr, buf);
        }
    }
    return val;
}

long eval_postinc(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return __eval_inc_dec(0, 1, a1);
}

long eval_postdec(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return __eval_inc_dec(0, 0, a1);
}

long eval_preinc(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return __eval_inc_dec(1, 1, a1);
}

long eval_predec(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return __eval_inc_dec(1, 0, a1);
}


/* extended operator list */
// #define NUMBER               0       /* numeric constant */
// #define REFERENCE            1       /* variable reference */
#define CH_GT                2       /* greater than */
#define CH_LT                3       /* lesser than */
#define CH_GE                4       /* greater than or equals */
#define CH_LE                5       /* lesser than or equals */
#define CH_RSH               6       /* shift right */
#define CH_LSH               7       /* shitf left */
#define CH_NE                8       /* not equals */
#define CH_EQ                9       /* equals */
#define CH_ASSIGN            10      /* assignment */
#define CH_PRE_INC           11      /* pre-increment op */
#define CH_POST_INC          12      /* post-increment op */
#define CH_PRE_DEC           13      /* pre-decrement op */
#define CH_POST_DEC          14      /* post-decrement op */
#define CH_B_AND             15      /* bitwise AND */
#define CH_B_OR              16      /* bitwise OR */
#define CH_B_XOR             17      /* bitwise XOR */
#define CH_AND               18      /* logical AND */
#define CH_OR                19      /* logical OR */
#define CH_EXP               20      /* exponent or ** */
#define CH_MINUS             21      /* unary minus */
#define CH_PLUS              22      /* unary plus */
#define CH_ASSIGN_PLUS       23      /* += assignment */
#define CH_ASSIGN_MINUS      24      /* -= assignment */
#define CH_ASSIGN_MULT       25      /* *= assignment */
#define CH_ASSIGN_DIV        26      /* /= assignment */
#define CH_ASSIGN_MOD        27      /* %= assignment */
#define CH_ASSIGN_LSH        28      /* <<= assignment */
#define CH_ASSIGN_RSH        29      /* >>= assignment */
#define CH_ASSIGN_AND        30      /* &= assignment */
#define CH_ASSIGN_XOR        31      /* ^= assignment */
#define CH_ASSIGN_OR         32      /* |= assignment */

enum { ASSOC_NONE = 0, ASSOC_LEFT, ASSOC_RIGHT };

/*
 * see this link for C operator precedence:
 * https://en.cppreference.com/w/c/language/operator_precedence
 */

struct op_s
{
    char op;
    int  prec;
    int  assoc;
    char unary;
    char chars;
    long (*eval)(struct stack_item_s *a1, struct stack_item_s *a2);
} ops[] =
{
    { CH_POST_INC     , 20, ASSOC_LEFT , 1, 2, eval_postinc       },
    { CH_POST_DEC     , 20, ASSOC_LEFT , 1, 2, eval_postdec       },
    { CH_PRE_INC      , 19, ASSOC_RIGHT, 1, 2, eval_postinc       },
    { CH_PRE_DEC      , 19, ASSOC_RIGHT, 1, 2, eval_postdec       },
    { CH_MINUS        , 19, ASSOC_RIGHT, 1, 1, eval_uminus        },
    { CH_PLUS         , 19, ASSOC_RIGHT, 1, 1, eval_uplus         },
    { '!'             , 19, ASSOC_RIGHT, 1, 1, eval_lognot        },
    { '~'             , 19, ASSOC_RIGHT, 1, 1, eval_bitnot        },
    { CH_EXP          , 18, ASSOC_RIGHT, 0, 2, eval_exp           },
    { '*'             , 17, ASSOC_LEFT , 0, 1, eval_mult          },
    { '/'             , 17, ASSOC_LEFT , 0, 1, eval_div           },
    { '%'             , 17, ASSOC_LEFT , 0, 1, eval_mod           },
    { '+'             , 16, ASSOC_LEFT , 0, 1, eval_add           },
    { '-'             , 16, ASSOC_LEFT , 0, 1, eval_sub           },
    { CH_LSH          , 15, ASSOC_LEFT , 0, 2, eval_lsh           },
    { CH_RSH          , 15, ASSOC_LEFT , 0, 2, eval_rsh           },
    { '<'             , 14, ASSOC_LEFT , 0, 1, eval_lt            },
    { CH_LE           , 14, ASSOC_LEFT , 0, 2, eval_le            },
    { '>'             , 14, ASSOC_LEFT , 0, 1, eval_gt            },
    { CH_GE           , 14, ASSOC_LEFT , 0, 2, eval_ge            },
    { CH_EQ           , 13, ASSOC_LEFT , 0, 2, eval_eq            },
    { CH_NE           , 13, ASSOC_LEFT , 0, 2, eval_ne            },
    { '&'             , 12, ASSOC_LEFT , 0, 1, eval_bitand        },
    { '^'             , 11, ASSOC_LEFT , 0, 1, eval_bitxor        },
    { '|'             , 10, ASSOC_LEFT , 0, 1, eval_bitor         },
    { CH_AND          , 9 , ASSOC_LEFT , 0, 2, eval_logand        },
    { CH_OR           , 8 , ASSOC_LEFT , 0, 2, eval_logor         },
    { CH_ASSIGN       , 7 , ASSOC_RIGHT, 0, 1, eval_assign        },
    { CH_ASSIGN_PLUS  , 7 , ASSOC_RIGHT, 0, 2, eval_assign_add    },
    { CH_ASSIGN_MINUS , 7 , ASSOC_RIGHT, 0, 2, eval_assign_sub    },
    { CH_ASSIGN_MULT  , 7 , ASSOC_RIGHT, 0, 2, eval_assign_mult   },
    { CH_ASSIGN_DIV   , 7 , ASSOC_RIGHT, 0, 2, eval_assign_div    },
    { CH_ASSIGN_MOD   , 7 , ASSOC_RIGHT, 0, 2, eval_assign_mod    },
    { CH_ASSIGN_LSH   , 7 , ASSOC_RIGHT, 0, 3, eval_assign_lsh    },
    { CH_ASSIGN_RSH   , 7 , ASSOC_RIGHT, 0, 3, eval_assign_rsh    },
    { CH_ASSIGN_AND   , 7 , ASSOC_RIGHT, 0, 2, eval_assign_and    },
    { CH_ASSIGN_XOR   , 7 , ASSOC_RIGHT, 0, 2, eval_assign_xor    },
    { CH_ASSIGN_OR    , 7 , ASSOC_RIGHT, 0, 2, eval_assign_or     },

    /*
     * TODO: add the comma ',' and ternary '?:' operators.
     */

    { '('             , 0 , ASSOC_NONE , 0, 1, NULL               },
    { ')'             , 0 , ASSOC_NONE , 0, 1, NULL               }
};

struct op_s *OP_POST_INC     = &ops[0 ];
struct op_s *OP_POST_DEC     = &ops[1 ];
struct op_s *OP_PRE_INC      = &ops[2 ];
struct op_s *OP_PRE_DEC      = &ops[3 ];
struct op_s *OP_UMINUS       = &ops[4 ];
struct op_s *OP_UPLUS        = &ops[5 ];
struct op_s *OP_LOG_NOT      = &ops[6 ];
struct op_s *OP_BIT_NOT      = &ops[7 ];
struct op_s *OP_EXP          = &ops[8 ];
struct op_s *OP_MULT         = &ops[9 ];
struct op_s *OP_DIV          = &ops[10];
struct op_s *OP_MOD          = &ops[11];
struct op_s *OP_ADD          = &ops[12];
struct op_s *OP_SUB          = &ops[13];
struct op_s *OP_LSH          = &ops[14];
struct op_s *OP_RSH          = &ops[15];
struct op_s *OP_LT           = &ops[16];
struct op_s *OP_LE           = &ops[17];
struct op_s *OP_GT           = &ops[18];
struct op_s *OP_GE           = &ops[19];
struct op_s *OP_EQ           = &ops[20];
struct op_s *OP_NE           = &ops[21];
struct op_s *OP_BIT_AND      = &ops[22];
struct op_s *OP_BIT_XOR      = &ops[23];
struct op_s *OP_BIT_OR       = &ops[24];
struct op_s *OP_LOG_AND      = &ops[25];
struct op_s *OP_LOG_OR       = &ops[26];
struct op_s *OP_ASSIGN       = &ops[27];
struct op_s *OP_ASSIGN_ADD   = &ops[28];
struct op_s *OP_ASSIGN_SUB   = &ops[29];
struct op_s *OP_ASSIGN_MULT  = &ops[30];
struct op_s *OP_ASSIGN_DIV   = &ops[31];
struct op_s *OP_ASSIGN_MOD   = &ops[32];
struct op_s *OP_ASSIGN_LSH   = &ops[33];
struct op_s *OP_ASSIGN_RSH   = &ops[34];
struct op_s *OP_ASSIGN_AND   = &ops[35];
struct op_s *OP_ASSIGN_XOR   = &ops[36];
struct op_s *OP_ASSIGN_OR    = &ops[37];
struct op_s *OP_LBRACE       = &ops[38];
struct op_s *OP_RBRACE       = &ops[39];


int special_char(char c)
{
    if(c == '_' || c =='@' || c == '#' || c == '$' || c == '?')
    {
        return 1;
    }
    return 0;
}

struct op_s *getop(char *expr)
{
    switch(*expr)
    {
        case '+':
            if(expr[1] == '+')
            {
                return OP_POST_INC;
            }
            else if(expr[1] == '=')
            {
                return OP_ASSIGN_ADD;
            }
            return OP_ADD;

        case '-':
            if(expr[1] == '-')
            {
                return OP_POST_DEC;
            }
            else if(expr[1] == '=')
            {
                return OP_ASSIGN_SUB;
            }
            return OP_SUB;

        case '*':
            if(expr[1] == '*')
            {
                return OP_EXP;
            }
            else if(expr[1] == '=')
            {
                return OP_ASSIGN_MULT;
            }
            return OP_MULT;

        case '<':
            if(expr[1] == '<')
            {
                if(expr[2] == '=')
                {
                    return OP_ASSIGN_LSH;
                }
                return OP_LSH;
            }
            else if(expr[1] == '=')
            {
                return OP_LE;
            }
            return OP_LT;

        case '>':
            if(expr[1] == '>')
            {
                if(expr[2] == '=')
                {
                    return OP_ASSIGN_RSH;
                }
                return OP_RSH;
            }
            else if(expr[1] == '=')
            {
                return OP_GE;
            }
            return OP_GT;

        case '!':
            if(expr[1] == '=')
            {
                return OP_NE;
            }
            return OP_LOG_NOT;

        case '=':
            if(expr[1] == '=')
            {
                return OP_EQ;
            }
            return OP_ASSIGN;

        case '&':
            if(expr[1] == '&')
            {
                return OP_LOG_AND;
            }
            else if(expr[1] == '=')
            {
                return OP_ASSIGN_AND;
            }
            return OP_BIT_AND;

        case '|':
            if(expr[1] == '|')
            {
                return OP_LOG_OR;
            }
            else if(expr[1] == '=')
            {
                return OP_ASSIGN_OR;
            }
            return OP_BIT_OR;

        case '^':
            if(expr[1] == '=')
            {
                return OP_ASSIGN_XOR;
            }
            return OP_BIT_XOR;
        case '/':
            if(expr[1] == '=')
            {
                return OP_ASSIGN_DIV;
            }
            return OP_DIV;
            
        case '%':
            if(expr[1] == '=')
            {
                return OP_ASSIGN_MOD;
            }
            return OP_MOD;

        case '~':
            return OP_BIT_NOT;
            
        case '(':
            return OP_LBRACE ;
            
        case ')':
            return OP_RBRACE ;
    }
    return NULL;
}

void push_opstack(struct op_s *op)
{
    if(nopstack>MAXOPSTACK-1)
    {
        fprintf(stderr, "%s: Operator stack overflow\n", SHELL_NAME);
        error = 1;
        return;
    }
    opstack[nopstack++]=op;
}

struct op_s *pop_opstack()
{
    if(!nopstack)
    {
        fprintf(stderr, "%s: Operator stack empty\n", SHELL_NAME);
        error = 1;
        return NULL;
    }
    return opstack[--nopstack];
}

void push_numstackl(long val)
{
    if(nnumstack > MAXNUMSTACK-1)
    {
        fprintf(stderr, "%s: Number stack overflow\n", SHELL_NAME);
        error = 1;
        return;
    }

    numstack[nnumstack].type = ITEM_LONG_INT;
    numstack[nnumstack++].val = val;
}

void push_numstackv(struct symtab_entry_s *val)
{
    if(nnumstack > MAXNUMSTACK-1)
    {
        fprintf(stderr, "%s: Number stack overflow\n", SHELL_NAME);
        error = 1;
        return;
    }

    numstack[nnumstack].type = ITEM_VAR_PTR;
    numstack[nnumstack++].ptr = val;
}

struct stack_item_s pop_numstack()
// long pop_numstack()
{
    if(!nnumstack)
    {
        fprintf(stderr, "%s: Number stack empty\n", SHELL_NAME);
        error = 1;
        return (struct stack_item_s) { };
    }
    return numstack[--nnumstack];
}


void shunt_op(struct op_s *op)
{
    struct op_s *pop;
    error = 0;
    if(op->op == '(')
    {
        push_opstack(op);
        return;
    }
    else if(op->op == ')')
    {
        while(nopstack > 0 && opstack[nopstack-1]->op != '(')
        {
            pop = pop_opstack();
            if(error)
            {
                return;
            }
            struct stack_item_s n1 = pop_numstack();
            if(error)
            {
                return;
            }
            if(pop->unary)
            {
                push_numstackl(pop->eval(&n1, 0));
            }
            else
            {
                struct stack_item_s n2 = pop_numstack();
                if(error)
                {
                    return;
                }
                push_numstackl(pop->eval(&n2, &n1));
                if(error)
                {
                    return;
                }
            }
        }
        if(!(pop = pop_opstack()) || pop->op != '(')
        {
            fprintf(stderr, "%s: Stack error. No matching \'(\'\n", SHELL_NAME);
            error = 1;
        }
        return;
    }

    if(op->assoc == ASSOC_RIGHT)
    {
        while(nopstack && op->prec < opstack[nopstack-1]->prec)
        {
            pop = pop_opstack();
            if(error)
            {
                return;
            }
            struct stack_item_s n1 = pop_numstack();
            if(pop->unary)
            {
                push_numstackl(pop->eval(&n1, 0));
            }
            else
            {
                struct stack_item_s n2 = pop_numstack();
                if(error)
                {
                    return;
                }
                push_numstackl(pop->eval(&n2, &n1));
            }
            if(error)
            {
                return;
            }
        }
    }
    else
    {
        while(nopstack && op->prec <= opstack[nopstack-1]->prec)
        {
            pop = pop_opstack();
            if(error)
            {
                return;
            }
            struct stack_item_s n1 = pop_numstack();
            if(pop->unary)
            {
                push_numstackl(pop->eval(&n1, 0));
            }
            else
            {
                struct stack_item_s n2 = pop_numstack();
                if(error)
                {
                    return;
                }
                push_numstackl(pop->eval(&n2, &n1));
            }
            if(error)
            {
                return;
            }
        }
    }
    push_opstack(op);
}

int isndigit(int c, char max_digit)
{
    if(max_digit <= '9')
    {
        return (c >= '0' && c <= max_digit);
    }
    else
    {
        if(c >= '0' && c <= '9')
        {
            return 1;          /* normal digits   */
        }
        if(c >= 'a' && c <= max_digit)
        {
            return 1;    /* small letters   */
        }
        return (c >= 'A' && c <= (max_digit-32));   /* capital letters */
    }
}

long get_num(char *s, int *chars)
{
    char *s2   = s ;
    long  num  = 0 ;
    int   base = 10;
    char  max_digit = '9';
    if(*s2 == '0')
    {
        switch(s2[1])
        {
            case 'x':
            case 'X':
                base  = 16;
                s2   += 2 ;
                while(isxdigit(*s2))
                {
                    char c = get_xdigit(*s2);
                    num = num*base + c;
                    s2++;
                }
                break;

            case 'b':
            case 'B':
                base  = 2;
                max_digit = '1';
                s2   += 2 ;
                break;
                
            default:
                base = 8;
                max_digit = '7';
                s2++;
                break;
        }
    }
    if(*s2 >= '0' && *s2 <= max_digit)
    {
        while(*s2 >= '0' && *s2 <= max_digit)
        {
            num = num*base + (*s2)-'0';
            s2++;
        }
    }
    /*
     * numbers can be written as base#n, where base is a number
     * between 1 and 64. this is a non-POSIX extension. digits higher
     * than 9 are represented with alphabetic characters a..z and A..Z,
     * the number of legal letters depends on the selected base.
     */
    if(*s2 == '#')
    {
        base = num;
        num  = 0;
        if(base <= 10)
        {
            max_digit = '0'+(base-1 );
        }
        else
        {
            max_digit = 'a'+(base-11);
        }
        s2++;
        while(isndigit(*s2, max_digit))
        {
            num = num*base + get_xdigit(*s2);
            s2++;
        }
    }
    *chars = s2-s;
    return num;
}

struct symtab_entry_s *get_var(char *s, int *chars)
{
    char *ss = s;
    if(*ss == '$')
    {
        ss++;        /* var names can begin with '$'. skip it */
    }
    char *s2 = ss;
    while(*s2 && (isalnum(*s2) || special_char(*s2)))
    {
        s2++;
    }
    int len = s2-ss;
    /* empty var name */
    if(len == 0)
    {
        *chars = s2-s;
        return NULL;
    }
    /* copy the name */
    char name[len+1];
    strncpy(name, ss, len);
    name[len] = '\0';
    /* get the symbol table entry for that var */
    struct symtab_entry_s *e = get_symtab_entry(name);
    if(!e)
    {
        e = add_to_symtab(name);
    }
    *chars = s2-s;              /* get the real length, including leading '$' if present */
    return e;
}


/*
 * Reverse Polish Notation (RPN) calculator.
 * 
 * POSIX note about arithmetic expansion:
 *   The shell shall expand all tokens in the expression for parameter expansion, 
 *   command substitution, and quote removal.
 * 
 * And the rules are:
 *   - Only signed long integer arithmetic is required.
 *   - Only the decimal-constant, octal-constant, and hexadecimal-constant constants 
 *     specified in the ISO C standard, Section 6.4.4.1 are required to be recognized 
 *     as constants.
 *   - The sizeof() operator and the prefix and postfix "++" and "--" operators are not 
 *     required.
 *   - Selection, iteration, and jump statements are not supported.
 * 
 * TODO: we should implement the functionality for math functions.
 *       this, of course, means we will need to compile our shell against
 *       libmath (by passing the -lm option to gcc).
 * 
 * TODO: other operators to implement (not required by POSIX):
 *       - the ternary operator (exprt ? expr : expr)
 *       - the comma operator (expr, expr)
 */

char *arithm_expand(char *__expr)
{
    char   *expr;
    char   *tstart       = NULL;
    struct  op_s startop = { 'X', 0, ASSOC_NONE, 0, 0, NULL };    /* Dummy operator to mark start */
    struct  op_s *op     = NULL;
    int     n1, n2;
    struct  op_s *lastop = &startop;
    /*
     * get a copy of __expr without the $(( and )).
     */
    int baseexp_len = strlen(__expr);
    char *baseexp = malloc(baseexp_len+1);
    if(!baseexp)
    {
        fprintf(stderr, "%s: insufficient memory for arithmetic expansion\n", SHELL_NAME);
        return NULL;
    }
    /* lose the $(( */
    if(__expr[0] == '$' && __expr[1] == '(')
    {
        strcpy(baseexp, __expr+3);
        baseexp_len -= 3;
    }
    else
    {
        strcpy(baseexp, __expr  );
    }
    /* and the )) */
    if(baseexp[baseexp_len-1] == ')' && baseexp[baseexp_len-2] == ')')
    {
        baseexp[baseexp_len-2] = '\0';
    }
    /* init our stacks */
    nopstack = 0;
    nnumstack = 0;
    /* clear the error flag */
    error = 0;
    expr = baseexp;
    
    /* and go ... */
    for( ; *expr; )
    {
        if(!tstart)
        {
            if((op = getop(expr)))
            {
                if(lastop && (lastop == &startop || lastop->op != ')'))
                {
                    /* take care of unary plus and minus */
                    if(op->op == '-')
                    {
                        op = OP_UMINUS;
                    }
                    else if(op->op == '+')
                    {
                        op = OP_UPLUS;
                    }
                    else if(op->op != '(' && !op->unary)
                    {
                        fprintf(stderr, "%s: illegal use of binary operator (%c)\n", SHELL_NAME, op->op);
                        goto err;
                    }
                }
                /* fix the pre-post ++/-- dilemma */
                if(op->op == CH_POST_INC || op->op == CH_POST_DEC)
                {
                    /* post ++/-- has higher precedence over pre ++/-- */
                    if(expr < baseexp+2 || !isalnum(expr[-2]) || !special_char(expr[-2]))
                    {
                        if(op == OP_POST_INC)
                        {
                            op = OP_PRE_INC;
                        }
                        else
                        {
                            op = OP_PRE_DEC;
                        }
                    }
                }
                error = 0;
                shunt_op(op);
                if(error)
                {
                    goto err;
                }
                lastop = op;
                expr += op->chars;
            }
            else if(isalnum(*expr) || special_char(*expr))
            {
                tstart = expr;
            }
            else if(isspace(*expr))
            {
                expr++;
            }
            else
            {
                fprintf(stderr, "%s: Syntax error near: %s\n", SHELL_NAME, expr);
                goto err;
            }
        }
        else
        {
            if(isspace(*expr))
            {
                expr++;
            }
            else if(isdigit(*expr))
            {
                n1 = get_num(tstart, &n2);
                error = 0;
                push_numstackl(n1);
                if(error)
                {
                    goto err;
                }
                tstart = NULL;
                lastop = NULL;
                expr += n2;
            }
            else if(isalpha(*expr) || special_char(*expr))
            {
                struct symtab_entry_s *n1 = get_var(tstart, &n2);
                if(!n1)
                {
                    fprintf(stderr, "%s: Failed to add symbol near: %s\n", SHELL_NAME, tstart);
                    goto err;
                }
                error = 0;
                push_numstackv(n1);
                if(error)
                {
                    goto err;
                }
                tstart = NULL;
                lastop = NULL;
                expr += n2;
            }
            else if((op = getop(expr)))
            {
                n1 = get_num(tstart, &n2);
                error = 0;
                push_numstackl(n1);
                if(error)
                {
                    goto err;
                }
                tstart = NULL;

                /* fix the pre-post ++/-- dilemma */
                if(op->op == CH_POST_INC || op->op == CH_POST_DEC)
                {
                    /* post ++/-- has higher precedence over pre ++/-- */
                    if(expr < baseexp+2 || !isalnum(expr[-2]) || !special_char(expr[-2]))
                    {
                        if(op == OP_POST_INC)
                        {
                            op = OP_PRE_INC;
                        }
                        else
                        {
                            op = OP_PRE_DEC;
                        }
                    }
                }

                shunt_op(op);
                if(error)
                {
                    goto err;
                }
                lastop = op;
                expr += op->chars;
            }
            else
            {
                fprintf(stderr, "%s: Syntax error near: %s\n", SHELL_NAME, expr);
                goto err;
            }
        }
    }

    if(tstart)
    {
        error = 0;
        if(isdigit(*tstart))
        {
            push_numstackl(get_num(tstart, &n2));
        }
        else if(isalpha(*tstart) || special_char(*tstart))
        {
            push_numstackv(get_var(tstart, &n2));
        }
        if(error)
        {
            goto err;
        }
    }

    while(nopstack)
    {
        error = 0;
        op = pop_opstack();
        if(error)
        {
            goto err;
        }
        struct stack_item_s n1 = pop_numstack();
        if(error)
        {
            goto err;
        }
        if(op->unary)
        {
            push_numstackl(op->eval(&n1, 0));
        }
        else
        {
            struct stack_item_s n2 = pop_numstack();
            if(error)
            {
                goto err;
            }
            push_numstackl(op->eval(&n2, &n1));
        }
        if(error)
        {
            goto err;
        }
    }

    /* empty arithmetic expression result */
    if(!nnumstack)
    {
        /*return false as the result */
        set_exit_status(1, 0);
        free(baseexp);
        return NULL;
    }

    /* we must have only 1 item on the stack now */
    if(nnumstack != 1)
    {
        fprintf(stderr, "%s: Number stack has %d elements after evaluation. Should be 1.\n", SHELL_NAME, nnumstack);
        goto err;
    }

    char res[64];
    sprintf(res, "%ld", numstack[0].val);
    char *res2 = __get_malloced_str(res);
    /*
     * invert the exit status for callers who use our value to test for true/false exit status,
     * which is inverted, i.e. non-zero result is true (or zero exit status) and vice versa.
     * this is what bash does with the (( expr )) compound command.
     */
    set_exit_status(!numstack[0].val, 0);
    free(baseexp);
    return res2;

err:
    free(baseexp);
    return NULL;
}
