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

/* max stack capacities */
#define MAXOPSTACK          64
#define MAXNUMSTACK         64

/* min and max bases for numeric operands */
#define MINBASE             2
#define MAXBASE             64

/* values for the type field of struct stack_item_s */
#define ITEM_LONG_INT       1
#define ITEM_VAR_PTR        2

/* struct to represent operand stack items */
struct stack_item_s
{
    int  type;
    union
    {
        long val;
        struct symtab_entry_s *ptr;
    };
};

// struct op_s *opstack[MAXOPSTACK];
// struct stack_item_s numstack[MAXNUMSTACK];
struct op_s **opstack;
struct stack_item_s *numstack;

int    nopstack  = 0;
int    nnumstack = 0;

int    error     = 0;

// struct symtab_entry_s dummy_var = { .name = "", .val_type = SYM_STR, .val = "0" };

/*
 * Recursively call arithm_expand() to perform the expansion on a variable
 * or a sub-expression.
 */
char *arithm_expand_recursive(char *str)
{
    /* save our pointers */
    struct op_s **opstack2 = opstack;
    struct stack_item_s *numstack2 = numstack;
    int nopstack2 = nopstack, nnumstack2 = nnumstack, error2 = error;

    /* perform arithmetic expansion on the sub-expression */
    char *res = arithm_expand(str);

    /* restore our pointers */
    opstack = opstack2;
    numstack = numstack2;
    nopstack = nopstack2;
    nnumstack = nnumstack2;
    error = error2;
    
    return res;
}


/*
 * The following functions perform different operations on their operands,
 * such as bitwise AND and OR, addition, subtraction, etc.
 */

long long_value(struct stack_item_s *a)
{
    /* for binary operators, bail out the 2nd operand if the first raised error */
    if(error)
    {
        return 0;
    }
    
    if(a->type == ITEM_LONG_INT)
    {
        return a->val;
    }
    else if(a->type == ITEM_VAR_PTR)
    {
        if(a->ptr->val)
        {
            /*
             * try to get a numeric value from the variable.. if that doesn't
             * work, try to arithmetically evaluate the string.
             */
            char *strend;
            long val = strtol(a->ptr->val, &strend, 10);
            if(!*strend)
            {
                return val;
            }
            
            char *s = arithm_expand_recursive(a->ptr->val);
            if(!s)
            {
                error = 1;
                return 0;
            }
            
            val = strtol(s, NULL, 10);
            free(s);
            return val;
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


/*
 * Functions to perform left and right bit shift.
 */

long eval_lsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) << long_value(a2);
}

long eval_rsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) >> long_value(a2);
}


/*
 * Functions to perform numeric comparisons: <, <=, >, >=, ==, !=.
 */

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


/*
 * Functions to perform bitwise operations: &, ^, |.
 */

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


/*
 * Functions to perform logical operations: &&, ||.
 */

long eval_logand(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) && long_value(a2);
}

long eval_logor(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return long_value(a1) || long_value(a2);
}


/*
 * Functions to perform arithmetic operators: exp, /, %.
 */

long do_eval_exp(long a1, long a2)
{
    return a2 < 0 ? 0 : (a2 == 0 ? 1 : a1 * do_eval_exp(a1, a2-1));
}

long eval_exp(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return do_eval_exp(long_value(a1), long_value(a2));
}

long eval_div(struct stack_item_s *a1, struct stack_item_s *a2) 
{
    error = 0;
    long n2 = long_value(a2);
    if(!n2)
    {
        PRINT_ERROR("%s: division by zero\n", SOURCE_NAME);
        error = 1;
        return 0;
    }
    return long_value(a1) / n2;
}

long eval_mod(struct stack_item_s *a1, struct stack_item_s *a2) 
{
    error = 0;
    long n2 = long_value(a2);
    if(!n2)
    {
        PRINT_ERROR("%s: division by zero\n", SOURCE_NAME);
        error = 1;
        return 0;
    }
    return long_value(a1) % n2;
}

long eval_assign_val(struct stack_item_s *a1, long val)
{
    if(a1->type == ITEM_VAR_PTR)
    {
        /* can't assign to read-only variables */
        if(flag_set(a1->ptr->flags, FLAG_READONLY))
        {
            READONLY_ASSIGN_ERROR(SOURCE_NAME, a1->ptr->name, "variable");
            error = 1;
            return 0;
        }
        
        /*
         * if we added this variable, remove the local flag, so the variable 
         * will be visible from the outer scope.
         */
        if(flag_set(a1->ptr->flags, FLAG_TEMP_VAR))
        {
            a1->ptr->flags &= ~(FLAG_LOCAL | FLAG_TEMP_VAR);
        }
        
        /*
         * we will set the value manually, instead of calling symtab_entry_setval(),
         * as the latter might end up calling arithm_expand() if the variable
         * has the -i attribute set.
         */
        char *old_val = a1->ptr->val;
        char buf[16];
        sprintf(buf, "%ld", val);
        //symtab_entry_setval(a1->ptr, buf);
    
        /* if special var, do whatever needs to be done in response to its new value */
        if(flag_set(a1->ptr->flags, FLAG_SPECIAL_VAR))
        {
            set_special_var(a1->ptr->name, buf);
        }
        a1->ptr->val = get_malloced_str(buf);

        /* free old value */
        if(old_val)
        {
            free_malloced_str(old_val);
        }
    }
    else
    {
        /* assignment to non-variable */
        PRINT_ERROR("%s: assignment to non-variable: %ld\n", SOURCE_NAME, long_value(a1));
        error = 1;
        val = 0;
    }
    return val;
}

long eval_assign(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_val(a1, long_value(a2));
}

long eval_assign_ext(long (*f)(struct stack_item_s *a1, struct stack_item_s *a2),
            struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_val(a1, f(a1, a2));
}

long eval_assign_add(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_add, a1, a2);
}

long eval_assign_sub(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_sub, a1, a2);
}

long eval_assign_mult(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_mult, a1, a2);
}

long eval_assign_div(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_div, a1, a2);
}

long eval_assign_mod(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_mod, a1, a2);
}

long eval_assign_lsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_lsh, a1, a2);
}

long eval_assign_rsh(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_rsh, a1, a2);
}

long eval_assign_and(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_bitand, a1, a2);
}

long eval_assign_xor(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_bitxor, a1, a2);
}

long eval_assign_or(struct stack_item_s *a1, struct stack_item_s *a2)
{
    return eval_assign_ext(eval_bitor, a1, a2);
}


/*
 * Functions to perform pre- and post- increment and decrement operators.
 */

long do_eval_inc_dec(int pre, int add, struct stack_item_s *a1)
{
    long val = long_value(a1);
    char buf[16];
    int diff = add ? 1 : -1;
    
    if(a1->type != ITEM_VAR_PTR)
    {
        /* assignment to non-variable */
        PRINT_ERROR("%s: expected operand for operator: %s\n", 
                    SOURCE_NAME, add ? "++" : "--");
        error = 1;
        return 0;
    }

    /* can't assign to read-only variables */
    if(flag_set(a1->ptr->flags, FLAG_READONLY))
    {
        READONLY_ASSIGN_ERROR(SOURCE_NAME, a1->ptr->name, "variable");
        error = 1;
        return 0;
    }

    if(pre)
    {
        val += diff;
        sprintf(buf, "%ld", val);
        symtab_entry_setval(a1->ptr, buf);
    }
    else
    {
        sprintf(buf, "%ld", val+diff);
        symtab_entry_setval(a1->ptr, buf);
    }
    return val;
}

long eval_postinc(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return do_eval_inc_dec(0, 1, a1);
}

long eval_postdec(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return do_eval_inc_dec(0, 0, a1);
}

long eval_preinc(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return do_eval_inc_dec(1, 1, a1);
}

long eval_predec(struct stack_item_s *a1, struct stack_item_s *unused __attribute__((unused)))
{
    return do_eval_inc_dec(1, 0, a1);
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
 * See this link for C operator precedence:
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
} arithm_ops[] =
{
    { CH_POST_INC     , 20, ASSOC_LEFT , 1, 2, eval_postinc       },
    { CH_POST_DEC     , 20, ASSOC_LEFT , 1, 2, eval_postdec       },
    { CH_PRE_INC      , 19, ASSOC_RIGHT, 1, 2, eval_preinc        },
    { CH_PRE_DEC      , 19, ASSOC_RIGHT, 1, 2, eval_predec        },
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
     * TODO: add the ternary '?:' operator.
     */

    { '('             , 0 , ASSOC_NONE , 0, 1, NULL               },
    { ')'             , 0 , ASSOC_NONE , 0, 1, NULL               },
    { ','             , 0 , ASSOC_NONE , 0, 1, NULL               }
};

struct op_s *OP_POST_INC     = &arithm_ops[0 ];
struct op_s *OP_POST_DEC     = &arithm_ops[1 ];
struct op_s *OP_PRE_INC      = &arithm_ops[2 ];
struct op_s *OP_PRE_DEC      = &arithm_ops[3 ];
struct op_s *OP_UMINUS       = &arithm_ops[4 ];
struct op_s *OP_UPLUS        = &arithm_ops[5 ];
struct op_s *OP_LOG_NOT      = &arithm_ops[6 ];
struct op_s *OP_BIT_NOT      = &arithm_ops[7 ];
struct op_s *OP_EXP          = &arithm_ops[8 ];
struct op_s *OP_MULT         = &arithm_ops[9 ];
struct op_s *OP_DIV          = &arithm_ops[10];
struct op_s *OP_MOD          = &arithm_ops[11];
struct op_s *OP_ADD          = &arithm_ops[12];
struct op_s *OP_SUB          = &arithm_ops[13];
struct op_s *OP_LSH          = &arithm_ops[14];
struct op_s *OP_RSH          = &arithm_ops[15];
struct op_s *OP_LT           = &arithm_ops[16];
struct op_s *OP_LE           = &arithm_ops[17];
struct op_s *OP_GT           = &arithm_ops[18];
struct op_s *OP_GE           = &arithm_ops[19];
struct op_s *OP_EQ           = &arithm_ops[20];
struct op_s *OP_NE           = &arithm_ops[21];
struct op_s *OP_BIT_AND      = &arithm_ops[22];
struct op_s *OP_BIT_XOR      = &arithm_ops[23];
struct op_s *OP_BIT_OR       = &arithm_ops[24];
struct op_s *OP_LOG_AND      = &arithm_ops[25];
struct op_s *OP_LOG_OR       = &arithm_ops[26];
struct op_s *OP_ASSIGN       = &arithm_ops[27];
struct op_s *OP_ASSIGN_ADD   = &arithm_ops[28];
struct op_s *OP_ASSIGN_SUB   = &arithm_ops[29];
struct op_s *OP_ASSIGN_MULT  = &arithm_ops[30];
struct op_s *OP_ASSIGN_DIV   = &arithm_ops[31];
struct op_s *OP_ASSIGN_MOD   = &arithm_ops[32];
struct op_s *OP_ASSIGN_LSH   = &arithm_ops[33];
struct op_s *OP_ASSIGN_RSH   = &arithm_ops[34];
struct op_s *OP_ASSIGN_AND   = &arithm_ops[35];
struct op_s *OP_ASSIGN_XOR   = &arithm_ops[36];
struct op_s *OP_ASSIGN_OR    = &arithm_ops[37];
struct op_s *OP_LBRACE       = &arithm_ops[38];
struct op_s *OP_RBRACE       = &arithm_ops[39];
struct op_s *OP_COMMA        = &arithm_ops[40];


/*
 * Return 1 if the given char is a valid shell variable name char.
 */
int valid_name_char(char c)
{
    switch(c)
    {
        case '_':
        case '@':
        case '#':
        case '$':
        case '?':
            return 1;

        default:
            if(isalnum(c))
            {
                return 1;
            }
            return 0;
    }
}


/*
 * Push an operator on the operator stack.
 */
void push_opstack(struct op_s *op)
{
    if(nopstack > MAXOPSTACK-1)
    {
        PRINT_ERROR("%s: operator stack overflow\n", SOURCE_NAME);
        error = 1;
        return;
    }
    opstack[nopstack++] = op;
}


/*
 * Pop an operator from the operator stack.
 */
struct op_s *pop_opstack(void)
{
    if(!nopstack)
    {
        PRINT_ERROR("%s: operator stack is empty: operator expected\n", 
                    SOURCE_NAME);
        error = 1;
        return NULL;
    }
    return opstack[--nopstack];
}


/*
 * Push a long numeric operand on the operand stack.
 */
void push_numstackl(long val)
{
    if(nnumstack > MAXNUMSTACK-1)
    {
        PRINT_ERROR("%s: number stack overflow\n", SOURCE_NAME);
        error = 1;
        return;
    }

    numstack[nnumstack].type = ITEM_LONG_INT;
    numstack[nnumstack++].val = val;
}


/*
 * Push a shell variable operand on the operand stack.
 */
void push_numstackv(struct symtab_entry_s *val)
{
    if(nnumstack > MAXNUMSTACK-1)
    {
        PRINT_ERROR("%s: number stack overflow\n", SOURCE_NAME);
        error = 1;
        return;
    }

    numstack[nnumstack].type = ITEM_VAR_PTR;
    numstack[nnumstack++].ptr = val;
}


/*
 * Pop an operand from the operand stack.
 */
struct stack_item_s pop_numstack(void)
{
    if(!nnumstack)
    {
        PRINT_ERROR("%s: number stack is empty: operand expected\n", 
                    SOURCE_NAME);
        error = 1;
        return (struct stack_item_s) { };
    }
    return numstack[--nnumstack];
}


/*
 * Perform operator shunting when we have a new operator by popping the operator
 * at the top of the stack and applying it to the operands on the operand stack.
 * We do this if the operator on top of the stack is not a '(' operator and:
 * 
 *   - has greater precedence than the new operator, or
 *   - has equal precedence to the new operator, but the top-of-stack one is
 *     left-associative
 * 
 * After popping the operator, we push the new operator on the operator stack,
 * and we push the previous top-of-stack operator's result on the operand stack.
 */

#define ERR_RETURN()    if(error) { return; }

void shunt_op(struct op_s *op)
{
    struct op_s *pop;
    error = 0;
    if(op->op == '(')
    {
        push_opstack(op);
        return;
    }
    else if(op->op == ')' || op->op == ',')
    {
        while(nopstack > 0)
        {
            if(opstack[nopstack-1]->op == '(')
            {
                break;
            }
            
            pop = pop_opstack();
            ERR_RETURN();

            struct stack_item_s n1 = pop_numstack();
            ERR_RETURN();

            if(pop->unary)
            {
                push_numstackl(pop->eval(&n1, 0));
            }
            else
            {
                struct stack_item_s n2 = pop_numstack();
                ERR_RETURN();
                push_numstackl(pop->eval(&n2, &n1));
                ERR_RETURN();
            }
        }

        if(op->op == ')')
        {
            if(!(pop = pop_opstack()) || pop->op != '(')
            {
                PRINT_ERROR("%s: stack error: no matching \'(\'\n", SOURCE_NAME);
                error = 1;
            }
        }
        return;
    }

    if(op->assoc == ASSOC_RIGHT)
    {
        while(nopstack && op->prec < opstack[nopstack-1]->prec)
        {
            pop = pop_opstack();
            ERR_RETURN();

            struct stack_item_s n1 = pop_numstack();
            if(pop->unary)
            {
                push_numstackl(pop->eval(&n1, 0));
            }
            else
            {
                struct stack_item_s n2 = pop_numstack();
                ERR_RETURN();
                push_numstackl(pop->eval(&n2, &n1));
            }
            ERR_RETURN();
        }
    }
    else
    {
        while(nopstack && op->prec <= opstack[nopstack-1]->prec)
        {
            pop = pop_opstack();
            ERR_RETURN();

            struct stack_item_s n1 = pop_numstack();
            if(pop->unary)
            {
                push_numstackl(pop->eval(&n1, 0));
            }
            else
            {
                struct stack_item_s n2 = pop_numstack();
                ERR_RETURN();
                push_numstackl(pop->eval(&n2, &n1));
            }
            ERR_RETURN();
        }
    }
    push_opstack(op);
}


/*
 * Check if the given digit falls in the range [0]..[base-1], then return
 * the numeric value of that digit.
 * The base can be any number from 2 to 64, with values higher than 9
 * represented by the letters a-z, then A-Z, then @ and _ (similar to bash).
 * If the base is <= 36, small and capital letters can be used interchangeably.
 * the result is place in the *result field, and 1 is returned. Otherwise
 * zero is returned.
 */
int get_ndigit(char c, int base, int *result)
{
    /* invalid char */
    if(!isalnum(c) && c != '@' && c != '_')
    {
        return 0;
    }

    char max, max2;
    /* base 10 or less: only digits 0-9 are acceptable */
    if(base <= 10)
    {
        max = '0'+base-1;
        if(c >= '0' && c <= max)
        {
            (*result) = c-'0';
            return 1;
        }
        goto invalid;
    }

    /* base larger than 10: if the digit is 0-9, return it */
    if(c >= '0' && c <= '9')
    {
        (*result) = c-'0';
        return 1;
    }

    /* bases 11 to 36: small and capital letters can be used interchangeably */
    if(base <= 36)
    {
        max  = 'a'+base-11;     /* max. small letter in this base */
        max2 = max-32;          /* max capital letter in this base */
        if(c >= 'a' && c <= max)
        {
            (*result) = c-'a'+10;
            return 1;
        }
        if(c >= 'A' && c <= max2)
        {
            (*result) = c-'A'+10;
            return 1;
        }
    }

    /*
     *  bases 37 to 64: the following characters represent the corresponding digits:
     *     a-z => 10-35
     *     A-Z => 36-61
     *     @   => 62
     *     _   => 63
     */
    else if(base <= 64)
    {
        /* check the small letters first */
        if(c >= 'a' && c <= 'z')
        {
            (*result) = c-'a'+10;
            return 1;
        }
        max2 = 'A'+base-37;          /* max capital letter in this base */
        if(c >= 'A' && c <= max2)
        {
            (*result) = c-'A'+36;
            return 1;
        }
    }
    
    if(c == '@' && (base == 63 || base == 64))
    {
        (*result) = 62;
        return 1;
    }
    
    if(c == '_' && base == 64)
    {
        (*result) = 63;
        return 1;
    }

invalid:
    /* invalid digit */
    PRINT_ERROR("%s: digit (%c) exceeds the value of the base (%d)\n", 
                SOURCE_NAME, c, base);
    error = 1;
    return 0;
}


/*
 * Extract an arithmetic operator from the beginning of expr.
 */
struct op_s *get_op(char *expr)
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
            return OP_LBRACE;

        case ')':
            return OP_RBRACE;

        case ',':
            return OP_COMMA;
    }
    return NULL;
}


/*
 * Extract a numeric operand from the beginning of the given string.
 * Numbers can be hex constants (preceded by 0x or 0X), octal (preceded by 0),
 * binary (preceded by 0b or 0B), or in any base, given in the format: [base#]n.
 * The number of characters used to get the number is stored in *char_count,
 * while the number itself is return as a long int.
 */
long get_num(char *s, int *char_count)
{
    char *s2 = s;
    long num = 0;
    int num2, base = 10;

    /* check if we have a predefined base */
    if(*s2 == '0')
    {
        char c = s2[1];
        switch(c)
        {
            case 'x':
            case 'X':
                /* hex number */
                base = 16;
                s2 += 2;
                break;

            case 'b':
            case 'B':
                /* binary number */
                base = 2;
                s2 += 2;
                break;
                
            default:
                if(isdigit(c))
                {
                    /* octal number */
                    base = 8;
                    s2++;
                }
                else if(isspace(c) || c == '\0' || c == '+' || c == '-' ||
                        c == '*' || c == '/' || c == '%' || c == '!' ||
                       c == '^' || c == ',')
                {
                    /* zero followed by space or \0 or an operator char */
                    (*char_count) = 1;
                    return 0;
                }
                else
                {
                    PRINT_ERROR("%s: invalid number near: %s\n", SOURCE_NAME, s);
                    error = 1;
                    return 0;
                }
                break;
        }
    }

    /* get the number according to the given base (use base 10 if none) */
    while(get_ndigit(*s2, base, &num2))
    {
        num = (num*base) + num2;
        s2++;
    }

    /* check we didn't encounter an invalid digit */
    if(error)
    {
        return 0;
    }

    /* if binary, octal or hex base, return the result */
    if(base != 10)
    {
        (*char_count) = s2-s;
        return num;
    }

    /*
     * Numbers can be written as base#n, where base is a number
     * between 2 and 64. This is a non-POSIX extension. Digits higher
     * than 9 are represented by alphabetic characters a..z, A..Z, @ and _.
     * the number of legal letters depends on the selected base.
     */
    if(*s2 == '#')
    {
        if(num < MINBASE || num > MAXBASE)
        {
            /* invalid base */
            PRINT_ERROR("%s: invalid arithmetic base: %ld\n", SOURCE_NAME, num);
            error = 1;
            return 0;
        }
        
        base = num;
        num  = 0;
        s2++;

        while(get_ndigit(*s2, base, &num2))
        {
            num = (num*base) + num2;
            s2++;
        }

        /* check we didn't encounter an invalid digit */
        if(error)
        {
            return 0;
        }
        
        /* check the number is not attached to non-digit chars */
        if(*s2 && !isspace(*s2))
        {
            PRINT_ERROR("%s: invalid number near: %s\n", SOURCE_NAME, s);
            error = 1;
            return 0;
        }
    }
    (*char_count) = s2-s;
    return num;
}


/*
 * Extract a shell variable name operand from the beginning of chars.
 */
struct symtab_entry_s *get_var(char *s, int *char_count)
{
    char *ss = s;
    int has_braces = 0;
    
    /* var names can begin with '$'. skip it */
    if(*ss == '$')
    {
        ss++;
    
        /* and can be enclosed in { } */
        has_braces = (*ss == '{');
        if(has_braces)
        {
            ss++;
        }
    }

    /* find the end of the name */
    char *s2 = ss;
    while(*s2 && valid_name_char(*s2))
    {
        s2++;
    }
    
    int len = s2-ss;

    /* check for a missing '}' */
    if(has_braces)
    {
        if(*s2 != '}')
        {
            return NULL;
        }
        s2++;
    }
    
    /* empty var name */
    if(len == 0)
    {
        (*char_count) = s2-s;
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
        e->flags = FLAG_LOCAL | FLAG_TEMP_VAR;
    }
    /* get the real length, including leading '$' if present */
    (*char_count) = s2-s;
    return e;
}


/*
 * Determine whether an increment '++' or decrement '--' operator works as a
 * pre- or post-operator by examining the characters preceding the operator.
 * If the operator is preceded by an alphanumeric character, its a post-op,
 * otherwise its a pre-op (post-ops have higher precedence than pre-ops).
 */
int is_post_op(char *baseexp, char *expr)
{
    if(expr < baseexp+2)
    {
        return 0;
    }
    
    expr--;
    while(expr > baseexp && isspace(*expr))
    {
        expr--;
    }
    
    return valid_name_char(*expr);
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

#define CHECK_ERR_FLAG()    if(error) { goto err; }
#define DISCARD_COMMA()     if(lastop && lastop->op == ',') { pop_numstack(); }

char *arithm_expand(char *orig_expr)
{
    /* our stacks */
    struct op_s *__opstack[MAXOPSTACK];
    struct stack_item_s __numstack[MAXNUMSTACK];
    opstack = __opstack;
    numstack = __numstack;

    char   *expr;
    char   *tstart       = NULL;
    struct  op_s startop = { 'X', 0, ASSOC_NONE, 0, 0, NULL };    /* dummy operator to mark start */
    struct  op_s *op     = NULL;
    int     n1, n2;
    struct  op_s *lastop = &startop;
    
    /*
     * get a copy of orig_expr without the $(( and )), or the $[ and ]
     * if we're given the obsolete arithmetic expansion operator.
     */
    int baseexp_len = strlen(orig_expr);
    char *baseexp = malloc(baseexp_len+1);
    if(!baseexp)
    {
        PRINT_ERROR("%s: insufficient memory for arithmetic expansion\n", 
                    SOURCE_NAME);
        return NULL;
    }

    /* lose the $(( */
    if(orig_expr[0] == '$' && orig_expr[1] == '(')
    {
        strcpy(baseexp, orig_expr+3);
        baseexp_len -= 3;
        /* and the )) */
        if(baseexp[baseexp_len-1] == ')' && baseexp[baseexp_len-2] == ')')
        {
            baseexp[baseexp_len-2] = '\0';
        }
    }
    /* lose the $[ */
    else if(orig_expr[0] == '$' && orig_expr[1] == '[')
    {
        strcpy(baseexp, orig_expr+2);
        baseexp_len -= 2;
        /* and the ] */
        if(baseexp[baseexp_len-1] == ']')
        {
            baseexp[baseexp_len-1] = '\0';
        }
    }
    else
    {
        strcpy(baseexp, orig_expr);
    }
    
    /* perhaps we need to perform word-expansion? */
    if(strchr_any(baseexp, "'`\"") || strstr(baseexp, "$("))
    {
        struct word_s *word = word_expand(baseexp, FLAG_REMOVE_QUOTES);
        if(word)
        {
            char *newstr = wordlist_to_str(word, WORDLIST_ADD_SPACES);
            free_all_words(word);
            if(newstr)
            {
                free(baseexp);
                baseexp = newstr;
                baseexp_len = strlen(baseexp);
            }
        }
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
            if((op = get_op(expr)))
            {
                if(lastop)
                {
                    if(lastop == &startop || lastop->op != ')')
                    {
                        if(lastop->op != CH_POST_INC && lastop->op != CH_POST_DEC)
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
                                PRINT_ERROR("%s: illegal use of binary operator near: %s\n", 
                                            SOURCE_NAME, expr);
                                goto err;
                            }
                        }
                    }
                    else
                    {
                        DISCARD_COMMA();
                    }
                }

                /* fix the pre-post ++/-- dilemma */
                if(op->op == CH_POST_INC || op->op == CH_POST_DEC)
                {
                    /* post ++/-- has higher precedence over pre ++/-- */
                    if(!is_post_op(baseexp, expr))
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
                CHECK_ERR_FLAG();
                lastop = op;
                expr += op->chars;
            }
            else if(valid_name_char(*expr))
            {
                tstart = expr;
            }
            else if(isspace(*expr))
            {
                expr++;
            }
            else
            {
                /* unknown token - try to parse as a command substitution */
                free(baseexp);
                return command_substitute(orig_expr);
            }
        }
        else
        {
            if(isspace(*expr))
            {
                /* skip over whitespace chars */
                expr++;
            }
            else if(isdigit(*expr))
            {
                /* numeric argument */
                error = 0;
                n1 = get_num(tstart, &n2);
                CHECK_ERR_FLAG();
                DISCARD_COMMA();
                push_numstackl(n1);
                CHECK_ERR_FLAG();
                tstart = NULL;
                lastop = NULL;
                expr += n2;
            }
            else if(*expr == '$' && expr[1] == '(' && expr[2] == '(')
            {
                /* nested arithmetic expression */
                size_t i = find_closing_brace(expr+1, 0);
                if(i == 0)
                {
                    /* closing brace not found */
                    PRINT_ERROR("%s: syntax error near: %s\n", SOURCE_NAME, expr);
                    goto err;
                }
                
                /* we add 2 for the $ at the beginning and the ) at the end */
                char *sub_expr = get_malloced_strl(expr, 0, i+2);
                char *sub_res, *strend;
                
                if(!sub_expr)
                {
                    PRINT_ERROR("%s: insufficient memory to parse arithmetic expression\n", 
                                SOURCE_NAME);
                    goto err;
                }

                /* perform arithmetic expansion on the sub-expression */
                sub_res = arithm_expand_recursive(sub_expr);
                free_malloced_str(sub_expr);

                if(!sub_res)
                {
                    goto err;
                }
                
                /* get the expansion's numeric result */
                n1 = strtol(sub_res, &strend, 10);
                free(sub_res);
                
                /* push it on the stack and move on */
                DISCARD_COMMA();
                push_numstackl(n1);
                CHECK_ERR_FLAG();
                tstart = NULL;
                lastop = NULL;
                expr += i+2;
            }
            else if(valid_name_char(*expr))
            {
                /* variable name */
                struct symtab_entry_s *n1 = get_var(tstart, &n2);
                if(!n1)
                {
                    PRINT_ERROR("%s: failed to add symbol near: %s\n", 
                                SOURCE_NAME, tstart);
                    goto err;
                }
                DISCARD_COMMA();
                error = 0;
                push_numstackv(n1);
                CHECK_ERR_FLAG();
                tstart = NULL;
                lastop = NULL;
                expr += n2;
            }
            else if((op = get_op(expr)))
            {
                /* operator token */
                error = 0;
                n1 = get_num(tstart, &n2);
                CHECK_ERR_FLAG();
                DISCARD_COMMA();
                push_numstackl(n1);
                CHECK_ERR_FLAG();
                tstart = NULL;

                /* fix the pre-post ++/-- dilemma */
                if(op->op == CH_POST_INC || op->op == CH_POST_DEC)
                {
                    /* post ++/-- has higher precedence over pre ++/-- */
                    if(!is_post_op(baseexp, expr))
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
                CHECK_ERR_FLAG();
                lastop = op;
                expr += op->chars;
            }
            else
            {
                /* unknown token - try to parse as a command substitution */
                free(baseexp);
                return command_substitute(orig_expr);
            }
        }
    }

    if(tstart)
    {
        error = 0;
        if(isdigit(*tstart))
        {
            n1 = get_num(tstart, &n2);
            CHECK_ERR_FLAG();
            push_numstackl(n1);
        }
        else if(valid_name_char(*tstart))
        {
            push_numstackv(get_var(tstart, &n2));
        }
        CHECK_ERR_FLAG();
    }

    while(nopstack)
    {
        error = 0;
        op = pop_opstack();
        CHECK_ERR_FLAG();
        
        /*
         * we shouldn't have a '(', as shunt_op() should have popped it when
         * we got ')'.. if we still find a '(' in the operator stack, it means
         * we have a '(' with no matchin ')'.
         */
        if(op->op == '(')
        {
            PRINT_ERROR("%s: error: missing \')\'\n", SOURCE_NAME);
            goto err;
        }
        
        struct stack_item_s n1 = pop_numstack();
        CHECK_ERR_FLAG();
        if(op->unary)
        {
            push_numstackl(op->eval(&n1, 0));
        }
        else
        {
            struct stack_item_s n2 = pop_numstack();
            CHECK_ERR_FLAG();
            push_numstackl(op->eval(&n2, &n1));
        }
        CHECK_ERR_FLAG();
    }

    /* empty arithmetic expression result */
    if(!nnumstack)
    {
        /*return true as the result */
        set_internal_exit_status(2);
        free(baseexp);
        return __get_malloced_str("");
    }

    /* we must have only 1 item on the stack now */
    if(nnumstack != 1)
    {
        PRINT_ERROR("%s: number stack has %d elements after evaluation (should be 1)\n", 
                    SOURCE_NAME, nnumstack);
        goto err;
    }

    char res[64];
    if(numstack[0].type == ITEM_LONG_INT)
    {
        sprintf(res, "%ld", numstack[0].val);
    }
    else
    {
        struct symtab_entry_s *e = numstack[0].ptr;
        if(e->val && e->val_type == SYM_STR)
        {
            strcpy(res, e->val);
        }
        else
        {
            sprintf(res, "0");
        }
    }
    
    char *res2 = __get_malloced_str(res);

    /*
     * invert the exit status for callers who use our value to test for true/false exit status,
     * which is inverted, i.e. non-zero result is true (or zero exit status) and vice versa.
     * this is what bash does with the (( expr )) compound command.
     */
    n1 = strtol(res, &expr, 10);
    if(*expr)
    {
        n1 = 0;
    }
    set_internal_exit_status(!n1);

    free(baseexp);
    return res2;

err:
    free(baseexp);
    set_internal_exit_status(2);
    return NULL;
}
