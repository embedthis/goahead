/*
    js.h -- JavaScript header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_JS
#define _h_JS 1

/********************************* Includes ***********************************/

#include    "goahead.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Defines ***********************************/
#ifdef __cplusplus
extern "C" {
#endif
/*
    Constants
 */
#define JS_MAX_TOKEN        1024    /* Max token size */
#define JS_INC              110     /* Growth for lines */
#define JS_SCRIPT_INC       1023    /* Growth for scripts */
#define JS_OFFSET           1       /* hAlloc doesn't like 0 entries */
#define JS_MAX_RECURSE      100     /* Sanity for maximum recursion */

/*
    Javascript Lexical analyser tokens
 */
#define TOK_ERR             -1      /* Any error */
#define TOK_LPAREN          1       /* ( */
#define TOK_RPAREN          2       /* ) */
#define TOK_IF              3       /* if */
#define TOK_ELSE            4       /* else */
#define TOK_LBRACE          5       /* { */
#define TOK_RBRACE          6       /* } */
#define TOK_LOGICAL         7       /* ||, &&, ! */
#define TOK_EXPR            8       /* +, -, /, % */
#define TOK_SEMI            9       /* ; */
#define TOK_LITERAL         10      /* literal string */
#define TOK_FUNCTION        11      /* function name */
#define TOK_NEWLINE         12      /* newline white space */
#define TOK_ID              13      /* function name */
#define TOK_EOF             14      /* End of script */
#define TOK_COMMA           15      /* Comma */
#define TOK_VAR             16      /* var */
#define TOK_ASSIGNMENT      17      /* = */
#define TOK_FOR             18      /* for */
#define TOK_INC_DEC         19      /* ++, -- */
#define TOK_RETURN          20      /* return */

/*
    Expression operators
 */
#define EXPR_LESS           1       /* < */
#define EXPR_LESSEQ         2       /* <= */
#define EXPR_GREATER        3       /* > */
#define EXPR_GREATEREQ      4       /* >= */
#define EXPR_EQ             5       /* == */
#define EXPR_NOTEQ          6       /* != */
#define EXPR_PLUS           7       /* + */
#define EXPR_MINUS          8       /* - */
#define EXPR_DIV            9       /* / */
#define EXPR_MOD            10      /* % */
#define EXPR_LSHIFT         11      /* << */
#define EXPR_RSHIFT         12      /* >> */
#define EXPR_MUL            13      /* * */
#define EXPR_ASSIGNMENT     14      /* = */
#define EXPR_INC            15      /* ++ */
#define EXPR_DEC            16      /* -- */
#define EXPR_BOOL_COMP      17      /* ! */
/*
    Conditional operators
 */
#define COND_AND            1       /* && */
#define COND_OR             2       /* || */
#define COND_NOT            3       /* ! */

/*
    States
 */
#define STATE_ERR               -1          /* Error state */
#define STATE_EOF               1           /* End of file */
#define STATE_COND              2           /* Parsing a "(conditional)" stmt */
#define STATE_COND_DONE         3
#define STATE_RELEXP            4           /* Parsing a relational expr */
#define STATE_RELEXP_DONE       5
#define STATE_EXPR              6           /* Parsing an expression */
#define STATE_EXPR_DONE         7
#define STATE_STMT              8           /* Parsing General statement */
#define STATE_STMT_DONE         9
#define STATE_STMT_BLOCK_DONE   10          /* End of block "}" */
#define STATE_ARG_LIST          11          /* Function arg list */
#define STATE_ARG_LIST_DONE     12
#define STATE_DEC_LIST          16          /* Declaration list */
#define STATE_DEC_LIST_DONE     17
#define STATE_DEC               18
#define STATE_DEC_DONE          19
#define STATE_RET               20          /* Return statement */

#define STATE_BEGIN             STATE_STMT

/*
    Flags. Used in Js and as parameter to parse()
 */
#define FLAGS_EXE               0x1             /* Execute statements */
#define FLAGS_VARIABLES         0x2             /* Allocated variables store */
#define FLAGS_FUNCTIONS         0x4             /* Allocated function store */

/*
    Function call structure
 */
typedef struct JsFun {
    char        *fname;                         /* Function name */
    char        **args;                         /* Args for function (halloc) */
    int         nArgs;                          /* Number of args */
} JsFun;

/*
    Evaluation block structure
 */
typedef struct JsInput {
    WebsBuf     tokbuf;                         /* Current token */
    WebsBuf     script;                         /* Input script for parsing */
    char        *putBackToken;                  /* Putback token string */
    int         putBackTokenId;                 /* Putback token ID */
    char        *line;                          /* Current line */
    int         lineLength;                     /* Current line length */
    int         lineNumber;                     /* Parse line number */
    int         lineColumn;                     /* Column in line */
} JsInput;


/**
    Javascript engine structure
    @defgroup Js Js
 */
typedef struct Js {
    JsInput     *input;                         /* Input evaluation block */
    WebsHash    functions;                      /* Symbol table for functions */
    WebsHash    *variables;                     /* hAlloc list of variables */
    int         variableMax;                    /* Number of entries */
    JsFun       *func;                          /* Current function */
    char        *result;                        /* Current expression result */
    char        *error;                         /* Error message */
    char        *token;                         /* Pointer to token string */
    int         tid;                            /* Current token id */
    int         jid;                            /* Halloc handle */
    int         flags;                          /* Flags */
    void        *userHandle;                    /* User defined handle */
} Js;


/**
    Javascript function procedure
    @ingroup Js
 */
typedef int (*JsProc)(int jid, void *handle, int argc, char **argv);

/******************************** Prototypes **********************************/
/**
    Utility routine to parse function arguments
    @param argc Count of arguments in argv
    @param argv Array of arguments
    @param fmt Printf style format string
    @return Count of the arguments parsed
    @ingroup Js
 */
PUBLIC int jsArgs(int argc, char **argv, cchar *fmt, ...);

/**
    Close a javascript engine
    @param jid Javascript ID allocated via jsOpenEngine
    @ingroup Js
 */
PUBLIC void jsCloseEngine(int jid);

/**
    Emit a parse error
    @param js Javascript engine object
    @param fmt Error message format string
    @ingroup Js
 */
PUBLIC void jsError(Js *js, cchar *fmt, ...);

/**
    Parse and evaluate a script. Return the last function return value.
    @param jid Javascript ID allocated via jsOpenEngine
    @param script Script to evaluate
    @param emsg Pointer to a string to receive any error message
    @param str String value to use as the result. Set to null for errors.
    @ingroup Js
 */
PUBLIC char *jsEval(int jid, cchar *script, char **emsg);

/**
    Get the function result value
    @param jid Javascript ID allocated via jsOpenEngine
    @return Function return value string. Caller must not free.
    @ingroup Js
 */
PUBLIC cchar *jsGetResult(int jid);

/**
    Get a variable value
    @param jid Javascript ID allocated via jsOpenEngine
    @param var Variable name
    @param value Returned value.
    @return If successful, a positive variable index, otherwise -1. This will be zero for global variables
        and > 0 for local variables.
    @ingroup Js
 */
PUBLIC int jsGetVar(int jid, cchar *var, cchar **value);

/**
    Open a new javascript engine
    @param variables Hash table of variables
    @param functions Hash table of functions
    @ingroup Js
 */
PUBLIC int jsOpenEngine(WebsHash variables, WebsHash functions);

/**
    Set a local variable
    @param jid Javascript ID allocated via jsOpenEngine
    @param var Variable name
    @param value Value to use
    @ingroup Js
 */
PUBLIC void jsSetLocalVar(int jid, cchar *var, cchar *value);

/**
    Set a global variable
    @param jid Javascript ID allocated via jsOpenEngine
    @param var Variable name
    @param value value to use
    @ingroup Js
 */
PUBLIC void jsSetGlobalVar(int jid, cchar *var, cchar *value);

/**
    Set the function return result
    @param jid Javascript ID allocated via jsOpenEngine
    @param str String value to use as the result
    @ingroup Js
 */
PUBLIC void jsSetResult(int jid, cchar *str);

/**
    Set a variable value in the top most variable frame
    @param jid Javascript ID allocated via jsOpenEngine
    @param var Variable name
    @param value Value to set
    @ingroup Js
 */
PUBLIC void jsSetVar(int jid, cchar *var, cchar *value);

/**
    Set a global function
    @param jid Javascript ID allocated via jsOpenEngine
    @param name Javascript function name
    @param fn C function providing the implementation.
    @ingroup Js
 */
PUBLIC int jsSetGlobalFunction(int jid, cchar *name, JsProc fn);

/*
    Internal API
 */
PUBLIC int      jsCloseBlock(int jid, int vid);
PUBLIC char     *jsEvalBlock(int jid, cchar *script, char **emsg);
PUBLIC WebsHash jsGetFunctionTable(int jid);
PUBLIC void     *jsGetGlobalFunction(int jid, cchar *name);
PUBLIC int      jsGetLineNumber(int jid);
PUBLIC void     *jsGetUserHandle(int jid);
PUBLIC WebsHash jsGetVariableTable(int jid);
PUBLIC int      jsLexOpen(Js *ep);
PUBLIC void     jsLexClose(Js *ep);
PUBLIC int      jsLexOpenScript(Js *ep, cchar *script);
PUBLIC void     jsLexCloseScript(Js *ep);
PUBLIC void     jsLexSaveInputState(Js *ep, JsInput *state);
PUBLIC void     jsLexFreeInputState(Js *ep, JsInput *state);
PUBLIC void     jsLexRestoreInputState(Js *ep, JsInput *state);
PUBLIC int      jsLexGetToken(Js *ep, int state);
PUBLIC void     jsLexPutbackToken(Js *ep, int tid, cchar *string);
PUBLIC int      jsOpenBlock(int jid);
PUBLIC int      jsRemoveGlobalFunction(int jid, cchar *name);
PUBLIC int      jsSetGlobalFunctionDirect(WebsHash functions, cchar *name, JsProc fn);
PUBLIC void     jsSetUserHandle(int jid, void *handle);

#if ME_GOAHEAD_LEGACY
    typedef Js ej_t;
    typedef JsInput jsinput_t;
    typedef JsFun jsfunc_t;

    #define ejOpenBlock jsOpenBlock
    #define ejCloseBlock jsCloseBlock
    #define ejEvalBlock jsEvalBlock
    #define ejRemoveGlobalFunction jsRemoveGlobalFunction
    #define ejGetGlobalFunction jsGetGlobalFunction
    #define ejSetGlobalFunctionDirect jsSetGlobalFunctionDirect
    #define ejError jsError
    #define ejSetUserHandle jsSetUserHandle
    #define ejGetUserHandle jsGetUserHandle
    #define ejGetLineNumber jsGetLineNumber
    #define ejGetResult jsGetResult
    #define ejSetLocalVar jsSetLocalVar
    #define ejSetGlobalVar jsSetGlobalVar
    #define ejLexOpen jsLexOpen
    #define ejLexClose jsLexClose
    #define ejLexOpenScript jsLexOpenScript
    #define ejLexCloseScript jsLexCloseScript
    #define ejLexSaveInputState jsLexSaveInputState
    #define ejLexFreeInputState jsLexFreeInputState
    #define ejLexRestoreInputState jsLexRestoreInputState
    #define ejLexGetToken jsLexGetToken
    #define ejLexPutbackToken jsLexPutbackToken
    #define ejGetVariableTable jsGetVariableTable
    #define ejGetFunctionTable jsGetFunctionTable
    #define ejArgs jsArgs
    #define ejSetResult jsSetResult
    #define ejOpenEngine jsOpenEngine
    #define ejCloseEngine jsCloseEngine
    #define ejSetGlobalFunction jsSetGlobalFunction
    #define ejSetVar jsSetVar
    #define ejGetVar jsGetVar
    #define ejEval jsEval
#endif
#ifdef __cplusplus
}
#endif
#endif /* ME_GOAHEAD_JAVASCRIPT */
#endif /* _h_JS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
