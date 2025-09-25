/*
    js.c -- Mini JavaScript

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "js.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Defines ***********************************/

#define     OCTAL 8
#define     HEX   16

static Js  **jsHandles;     /* List of js handles */
static int jsMax = -1;      /* Maximum size of  */

/****************************** Forward Declarations **************************/

static Js *jsPtr(int jid);
static void     clearString(char **ptr);
static void     setString(char **ptr, cchar *s);
static void     appendString(char **ptr, cchar *s);
static int      parse(Js *ep, int state, int flags);
static int      parseStmt(Js *ep, int state, int flags);
static int      parseDeclaration(Js *ep, int state, int flags);
static int      parseCond(Js *ep, int state, int flags);
static int      parseExpr(Js *ep, int state, int flags);
static int      parseFunctionArgs(Js *ep, int state, int flags);
static int      evalExpr(Js *ep, cchar *lhs, int rel, cchar *rhs);
static int      evalCond(Js *ep, cchar *lhs, int rel, cchar *rhs);
static int      evalFunction(Js *ep);
static void     freeFunc(JsFun *func);
static void     jsRemoveNewlines(Js *ep, int state);

static int      getLexicalToken(Js *ep, int state);
static int      tokenAddChar(Js *ep, int c);
static int      inputGetc(Js *ep);
static void     inputPutback(Js *ep, int c);
static int      charConvert(Js *ep, int base, int maxDig);

/************************************* Code ***********************************/

PUBLIC int jsOpenEngine(WebsHash variables, WebsHash functions)
{
    Js  *ep;
    int jid, vid;

    if ((jid = wallocObject(&jsHandles, &jsMax, sizeof(Js))) < 0) {
        return -1;
    }
    ep = jsHandles[jid];
    ep->jid = jid;

    /*
        Create a top level symbol table if one is not provided for variables and functions. Variables may create other
        symbol tables for block level declarations so we use walloc to manage a list of variable tables.
     */
    if ((vid = wallocHandle(&ep->variables)) < 0) {
        jsMax = wfreeHandle(&jsHandles, ep->jid);
        return -1;
    }
    if (vid >= ep->variableMax) {
        ep->variableMax = vid + 1;
    }
    if (variables == -1) {
        ep->variables[vid] = hashCreate(64) + JS_OFFSET;
        ep->flags |= FLAGS_VARIABLES;
    } else {
        ep->variables[vid] = variables + JS_OFFSET;
    }
    if (functions == -1) {
        ep->functions = hashCreate(64);
        ep->flags |= FLAGS_FUNCTIONS;
    } else {
        ep->functions = functions;
    }
    jsLexOpen(ep);
    ep->recursionDepth = 0;

    /*
        Define standard constants
     */
    jsSetGlobalVar(ep->jid, "null", NULL);
    return ep->jid;
}


PUBLIC void jsCloseEngine(int jid)
{
    Js  *ep;
    int i;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }

    wfree(ep->error);
    ep->error = NULL;

    wfree(ep->result);
    ep->result = NULL;

    jsLexClose(ep);

    for (i = ep->variableMax - 1; i >= 0; i--) {
        if (ep->flags & FLAGS_VARIABLES) {
            hashFree(ep->variables[i] - JS_OFFSET);
        }
        ep->variableMax = wfreeHandle(&ep->variables, i);
    }
    if (ep->flags & FLAGS_FUNCTIONS) {
        hashFree(ep->functions);
    }
    jsMax = wfreeHandle(&jsHandles, ep->jid);
    wfree(ep);
}


#if !ECOS && KEEP
PUBLIC char *jsEvalFile(int jid, char *path, char **emsg)
{
    WebsStat sbuf;
    Js       *ep;
    char     *script, *rs;
    int      fd;

    assert(path && *path);

    if (emsg) {
        *emsg = NULL;
    }
    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    if ((fd = open(path, O_RDONLY | O_BINARY, 0666)) < 0) {
        jsError(ep, "Bad handle %d", jid);
        return NULL;
    }
    if (stat(path, &sbuf) < 0) {
        close(fd);
        jsError(ep, "Cannot stat %s", path);
        return NULL;
    }
    if ((script = walloc(sbuf.st_size + 1)) == NULL) {
        close(fd);
        jsError(ep, "Cannot allocate %d", sbuf.st_size);
        return NULL;
    }
    if (read(fd, script, sbuf.st_size) != (int) sbuf.st_size) {
        close(fd);
        wfree(script);
        jsError(ep, "Error reading %s", path);
        return NULL;
    }
    script[sbuf.st_size] = '\0';
    close(fd);
    rs = jsEvalBlock(jid, script, emsg);
    wfree(script);
    return rs;
}
#endif


/*
    Create a new variable scope block so that consecutive jsEval calls may be made with the same variable scope. This
    space MUST be closed with jsCloseBlock when the evaluations are complete.
 */
PUBLIC int jsOpenBlock(int jid)
{
    Js  *ep;
    int vid;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    if ((vid = wallocHandle(&ep->variables)) < 0) {
        return -1;
    }
    if (vid >= ep->variableMax) {
        ep->variableMax = vid + 1;
    }
    ep->variables[vid] = hashCreate(64) + JS_OFFSET;
    return vid;

}


PUBLIC int jsCloseBlock(int jid, int vid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    hashFree(ep->variables[vid] - JS_OFFSET);
    ep->variableMax = wfreeHandle(&ep->variables, vid);
    return 0;

}


/*
    Create a new variable scope block and evaluate a script. All variables
    created during this context will be automatically deleted when complete.
 */
PUBLIC char *jsEvalBlock(int jid, cchar *script, char **emsg)
{
    char *returnVal;
    int  vid;

    assert(script);

    vid = jsOpenBlock(jid);
    returnVal = jsEval(jid, script, emsg);
    jsCloseBlock(jid, vid);
    return returnVal;
}


/*
    Parse and evaluate Javascript.
 */
PUBLIC char *jsEval(int jid, cchar *script, char **emsg)
{
    Js      *ep;
    JsInput *oldBlock;
    int     state;
    void    *endlessLoopTest;
    int     loopCounter;


    assert(script);

    if (emsg) {
        *emsg = NULL;
    }
    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    setString(&ep->result, "");

    /*
        Allocate a new evaluation block, and save the old one
     */
    oldBlock = ep->input;
    jsLexOpenScript(ep, script);

    /*
        Do the actual parsing and evaluation
     */
    loopCounter = 0;
    endlessLoopTest = NULL;

    do {
        state = parse(ep, STATE_BEGIN, FLAGS_EXE);

        if (state == STATE_RET) {
            state = STATE_EOF;
        }
        /*
            prevent parser from going into infinite loop.  If parsing the same line 10 times then fail and report Syntax
            error.  Most normal error are caught in the parser itself.
         */
        if (endlessLoopTest == ep->input->script.servp) {
            if (loopCounter++ > 10) {
                state = STATE_ERR;
                jsError(ep, "Syntax error");
            }
        } else {
            endlessLoopTest = ep->input->script.servp;
            loopCounter = 0;
        }
    } while (state != STATE_EOF && state != STATE_ERR);

    jsLexCloseScript(ep);

    /*
        Return any error string to the user
     */
    if (state == STATE_ERR && emsg) {
        *emsg = sclone(ep->error);
    }

    /*
        Restore the old evaluation block
     */
    ep->input = oldBlock;

    if (state == STATE_EOF) {
        return ep->result;
    }
    if (state == STATE_ERR) {
        return NULL;
    }
    return ep->result;
}



/*
    Recursive descent parser for Javascript
 */
static int parse(Js *ep, int state, int flags)
{
    assert(ep);

    ep->recursionDepth++;
    if (ep->recursionDepth > JS_MAX_RECURSE) {
        jsError(ep, "Script recursion too deep");
        ep->recursionDepth--;
        return STATE_ERR;
    }
    switch (state) {
    /*
        Any statement, function arguments or conditional expressions
     */
    case STATE_STMT:
        if ((state = parseStmt(ep, state, flags)) != STATE_STMT_DONE &&
            state != STATE_EOF && state != STATE_STMT_BLOCK_DONE &&
            state != STATE_RET) {
            state = STATE_ERR;
        }
        break;

    case STATE_DEC:
        if ((state = parseStmt(ep, state, flags)) != STATE_DEC_DONE &&
            state != STATE_EOF) {
            state = STATE_ERR;
        }
        break;

    case STATE_EXPR:
        if ((state = parseStmt(ep, state, flags)) != STATE_EXPR_DONE &&
            state != STATE_EOF) {
            state = STATE_ERR;
        }
        break;

    /*
        Variable declaration list
     */
    case STATE_DEC_LIST:
        state = parseDeclaration(ep, state, flags);
        break;

    /*
        Function argument string
     */
    case STATE_ARG_LIST:
        state = parseFunctionArgs(ep, state, flags);
        break;

    /*
        Logical condition list (relational operations separated by &&, ||)
     */
    case STATE_COND:
        state = parseCond(ep, state, flags);
        break;

    /*
       j  Expression list
     */
    case STATE_RELEXP:
        state = parseExpr(ep, state, flags);
        break;
    }

    if (state == STATE_ERR && ep->error == NULL) {
        jsError(ep, "Syntax error");
    }
    ep->recursionDepth--;
    return state;
}


/*
    Parse any statement including functions and simple relational operations
 */
static int parseStmt(Js *ep, int state, int flags)
{
    JsFun   func;
    JsFun   *saveFunc;
    JsInput condScript, endScript, bodyScript, incrScript;
    cchar   *value;
    char    *identifier;
    int     done, expectSemi, thenFlags, elseFlags, tid, cond, forFlags;
    int     jsVarType;

    assert(ep);

    /*
        Set these to NULL, else we try to free them if an error occurs.
     */
    endScript.putBackToken = NULL;
    bodyScript.putBackToken = NULL;
    incrScript.putBackToken = NULL;
    condScript.putBackToken = NULL;

    expectSemi = 0;
    saveFunc = NULL;

    for (done = 0; !done; ) {
        tid = jsLexGetToken(ep, state);

        switch (tid) {
        default:
            jsLexPutbackToken(ep, TOK_EXPR, ep->token);
            done++;
            break;

        case TOK_ERR:
            state = STATE_ERR;
            done++;
            break;

        case TOK_EOF:
            state = STATE_EOF;
            done++;
            break;

        case TOK_NEWLINE:
            break;

        case TOK_SEMI:
            /*
                This case is when we discover no statement and just a lone ';'
             */
            if (state != STATE_STMT) {
                jsLexPutbackToken(ep, tid, ep->token);
            }
            done++;
            break;

        case TOK_ID:
            /*
                This could either be a reference to a variable or an assignment
             */
            identifier = NULL;
            setString(&identifier, ep->token);
            /*
                Peek ahead to see if this is an assignment
             */
            tid = jsLexGetToken(ep, state);
            if (tid == TOK_ASSIGNMENT) {
                if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                    clearString(&identifier);
                    goto error;
                }
                if (flags & FLAGS_EXE) {
                    if ( state == STATE_DEC ) {
                        jsSetLocalVar(ep->jid, identifier, ep->result);
                    } else {
                        jsVarType = jsGetVar(ep->jid, identifier, &value);
                        if (jsVarType > 0) {
                            jsSetLocalVar(ep->jid, identifier, ep->result);
                        } else {
                            jsSetGlobalVar(ep->jid, identifier, ep->result);
                        }
                    }
                }

            } else if (tid == TOK_INC_DEC ) {
                value = NULL;
                if (flags & FLAGS_EXE) {
                    jsVarType = jsGetVar(ep->jid, identifier, &value);
                    if (jsVarType < 0 || !value) {
                        jsError(ep, "Undefined variable %s\n", identifier);
                        goto error;
                    }
                    setString(&ep->result, value);
                    if (evalExpr(ep, value, (int) *ep->token, "1") < 0) {
                        state = STATE_ERR;
                        break;
                    }
                    if (jsVarType > 0) {
                        jsSetLocalVar(ep->jid, identifier, ep->result);
                    } else {
                        jsSetGlobalVar(ep->jid, identifier, ep->result);
                    }
                }

            } else {
                /*
                    If we are processing a declaration, allow undefined vars
                 */
                value = NULL;
                if (state == STATE_DEC) {
                    if (jsGetVar(ep->jid, identifier, &value) > 0) {
                        jsError(ep, "Variable already declared",
                                identifier);
                        clearString(&identifier);
                        goto error;
                    }
                    jsSetLocalVar(ep->jid, identifier, NULL);
                } else {
                    if (flags & FLAGS_EXE) {
                        if (jsGetVar(ep->jid, identifier, &value) < 0) {
                            jsError(ep, "Undefined variable %s\n", identifier);
                            clearString(&identifier);
                            goto error;
                        }
                    }
                }
                setString(&ep->result, value);
                jsLexPutbackToken(ep, tid, ep->token);
            }
            clearString(&identifier);

            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_LITERAL:
            /*
                Set the result to the literal (number or string constant)
             */
            setString(&ep->result, ep->token);
            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_FUNCTION:
            /*
                We must save any current ep->func value for the current stack frame
             */
            if (ep->func) {
                saveFunc = ep->func;
            }
            memset(&func, 0, sizeof(JsFun));
            setString(&func.fname, ep->token);
            ep->func = &func;

            setString(&ep->result, "");
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            if (parse(ep, STATE_ARG_LIST, flags) != STATE_ARG_LIST_DONE) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            /*
                Evaluate the function if required
             */
            if (flags & FLAGS_EXE && evalFunction(ep) < 0) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            freeFunc(&func);
            ep->func = saveFunc;

            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }
            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_IF:
            if (state != STATE_STMT) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                goto error;
            }
            /*
                Evaluate the entire condition list "(condition)"
             */
            if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }
            /*
                This is the "then" case. We need to always parse both cases and execute only the relevant case.
             */
            if (*ep->result == '1') {
                thenFlags = flags;
                elseFlags = flags & ~FLAGS_EXE;
            } else {
                thenFlags = flags & ~FLAGS_EXE;
                elseFlags = flags;
            }
            /*
                Process the "then" case.  Allow for RETURN statement
             */
            switch (parse(ep, STATE_STMT, thenFlags)) {
            case STATE_RET:
                return STATE_RET;
            case STATE_STMT_DONE:
                break;
            default:
                goto error;
            }
            /*
                check to see if there is an "else" case
             */
            jsRemoveNewlines(ep, state);
            tid = jsLexGetToken(ep, state);
            if (tid != TOK_ELSE) {
                jsLexPutbackToken(ep, tid, ep->token);
                done++;
                break;
            }
            /*
                Process the "else" case.  Allow for return.
             */
            switch (parse(ep, STATE_STMT, elseFlags)) {
            case STATE_RET:
                return STATE_RET;
            case STATE_STMT_DONE:
                break;
            default:
                goto error;
            }
            done++;
            break;

        case TOK_FOR:
            /*
                Format for the expression is:
                    for (initial; condition; incr) {
                        body;
                    }
             */
            if (state != STATE_STMT) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                goto error;
            }

            /*
                Evaluate the for loop initialization statement
             */
            if (parse(ep, STATE_EXPR, flags) != STATE_EXPR_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_SEMI) {
                goto error;
            }

            /*
                The first time through, we save the current input context just to each step: prior to the conditional,
                the loop increment and the loop body.
             */
            jsLexSaveInputState(ep, &condScript);
            if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                goto error;
            }
            cond = (*ep->result != '0');

            if (jsLexGetToken(ep, state) != TOK_SEMI) {
                goto error;
            }

            /*
                Don't execute the loop increment statement or the body first time
             */
            forFlags = flags & ~FLAGS_EXE;
            jsLexSaveInputState(ep, &incrScript);
            if (parse(ep, STATE_EXPR, forFlags) != STATE_EXPR_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }

            /*
                Parse the body and remember the end of the body script
             */
            jsLexSaveInputState(ep, &bodyScript);
            if (parse(ep, STATE_STMT, forFlags) != STATE_STMT_DONE) {
                goto error;
            }
            jsLexSaveInputState(ep, &endScript);

            /*
                Now actually do the for loop. Note loop has been rotated
             */
            while (cond && (flags & FLAGS_EXE) ) {
                /*
                    Evaluate the body
                 */
                jsLexRestoreInputState(ep, &bodyScript);

                switch (parse(ep, STATE_STMT, flags)) {
                case STATE_RET:
                    return STATE_RET;
                case STATE_STMT_DONE:
                    break;
                default:
                    goto error;
                }
                /*
                    Evaluate the increment script
                 */
                jsLexRestoreInputState(ep, &incrScript);
                if (parse(ep, STATE_EXPR, flags) != STATE_EXPR_DONE) {
                    goto error;
                }
                /*
                    Evaluate the condition
                 */
                jsLexRestoreInputState(ep, &condScript);
                if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                    goto error;
                }
                cond = (*ep->result != '0');
            }
            jsLexRestoreInputState(ep, &endScript);
            done++;
            break;

        case TOK_VAR:
            if (parse(ep, STATE_DEC_LIST, flags) != STATE_DEC_LIST_DONE) {
                goto error;
            }
            done++;
            break;

        case TOK_COMMA:
            jsLexPutbackToken(ep, TOK_EXPR, ep->token);
            done++;
            break;

        case TOK_LPAREN:
            if (state == STATE_EXPR) {
                if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                    goto error;
                }
                if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                    goto error;
                }
                return STATE_EXPR_DONE;
            }
            done++;
            break;

        case TOK_RPAREN:
            jsLexPutbackToken(ep, tid, ep->token);
            return STATE_EXPR_DONE;

        case TOK_LBRACE:
            /*
                This handles any code in braces except "if () {} else {}"
             */
            if (state != STATE_STMT) {
                goto error;
            }

            /*
                Parse will return STATE_STMT_BLOCK_DONE when the RBRACE is seen
             */
            do {
                state = parse(ep, STATE_STMT, flags);
            } while (state == STATE_STMT_DONE);

            /*
                Allow return statement.
             */
            if (state == STATE_RET) {
                return state;
            }

            if (jsLexGetToken(ep, state) != TOK_RBRACE) {
                goto error;
            }
            return STATE_STMT_DONE;

        case TOK_RBRACE:
            if (state == STATE_STMT) {
                jsLexPutbackToken(ep, tid, ep->token);
                return STATE_STMT_BLOCK_DONE;
            }
            goto error;

        case TOK_RETURN:
            if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                goto error;
            }
            if (flags & FLAGS_EXE) {
                while ( jsLexGetToken(ep, state) != TOK_EOF );
                done++;
                return STATE_RET;
            }
            break;
        }
    }

    if (expectSemi) {
        tid = jsLexGetToken(ep, state);
        if (tid != TOK_SEMI && tid != TOK_NEWLINE) {
            goto error;
        }
        /*
            Skip newline after semi-colon
         */
        jsRemoveNewlines(ep, state);
    }

doneParse:
    if (tid == TOK_FOR) {
        jsLexFreeInputState(ep, &condScript);
        jsLexFreeInputState(ep, &incrScript);
        jsLexFreeInputState(ep, &endScript);
        jsLexFreeInputState(ep, &bodyScript);
    }
    if (state == STATE_STMT) {
        return STATE_STMT_DONE;
    } else if (state == STATE_DEC) {
        return STATE_DEC_DONE;
    } else if (state == STATE_EXPR) {
        return STATE_EXPR_DONE;
    } else if (state == STATE_EOF) {
        return state;
    } else {
        return STATE_ERR;
    }

error:
    state = STATE_ERR;
    goto doneParse;
}


/*
    Parse variable declaration list
 */
static int parseDeclaration(Js *ep, int state, int flags)
{
    int tid;

    assert(ep);

    /*
        Declarations can be of the following forms:
                var x;
                var x, y, z;
                var x = 1 + 2 / 3, y = 2 + 4;

        We set the variable to NULL if there is no associated assignment.
     */
    do {
        if ((tid = jsLexGetToken(ep, state)) != TOK_ID) {
            return STATE_ERR;
        }
        jsLexPutbackToken(ep, tid, ep->token);

        /*
            Parse the entire assignment or simple identifier declaration
         */
        if (parse(ep, STATE_DEC, flags) != STATE_DEC_DONE) {
            return STATE_ERR;
        }

        /*
            Peek at the next token, continue if comma seen
         */
        tid = jsLexGetToken(ep, state);
        if (tid == TOK_SEMI) {
            return STATE_DEC_LIST_DONE;
        } else if (tid != TOK_COMMA) {
            return STATE_ERR;
        }
    } while (tid == TOK_COMMA);

    if (tid != TOK_SEMI) {
        return STATE_ERR;
    }
    return STATE_DEC_LIST_DONE;
}


/*
    Parse function arguments
 */
static int parseFunctionArgs(Js *ep, int state, int flags)
{
    int tid, aid;

    assert(ep);

    do {
        state = parse(ep, STATE_RELEXP, flags);
        if (state == STATE_EOF || state == STATE_ERR) {
            return state;
        }
        if (state == STATE_RELEXP_DONE) {
            aid = wallocHandle(&ep->func->args);
            ep->func->args[aid] = sclone(ep->result);
            ep->func->nArgs++;
        }
        /*
            Peek at the next token, continue if more args (ie. comma seen)
         */
        tid = jsLexGetToken(ep, state);
        if (tid != TOK_COMMA) {
            jsLexPutbackToken(ep, tid, ep->token);
        }
    } while (tid == TOK_COMMA);

    if (tid != TOK_RPAREN && state != STATE_RELEXP_DONE) {
        return STATE_ERR;
    }
    return STATE_ARG_LIST_DONE;
}


/*
    Parse conditional expression (relational ops separated by ||, &&)
 */
static int parseCond(Js *ep, int state, int flags)
{
    char *lhs, *rhs;
    int  tid, operator;

    assert(ep);

    setString(&ep->result, "");
    rhs = lhs = NULL;
    operator = 0;

    do {
        /*
            Recurse to handle one side of a conditional. Accumulate the left hand side and the final result in
               ep->result.
         */
        state = parse(ep, STATE_RELEXP, flags);
        if (state != STATE_RELEXP_DONE) {
            state = STATE_ERR;
            break;
        }

        if (operator > 0) {
            setString(&rhs, ep->result);
            if (evalCond(ep, lhs, operator, rhs) < 0) {
                state = STATE_ERR;
                break;
            }
        }
        setString(&lhs, ep->result);

        tid = jsLexGetToken(ep, state);
        if (tid == TOK_LOGICAL) {
            operator = (int) *ep->token;

        } else if (tid == TOK_RPAREN || tid == TOK_SEMI) {
            jsLexPutbackToken(ep, tid, ep->token);
            state = STATE_COND_DONE;
            break;

        } else {
            jsLexPutbackToken(ep, tid, ep->token);
        }

    } while (state == STATE_RELEXP_DONE);

    if (lhs) {
        wfree(lhs);
    }
    if (rhs) {
        wfree(rhs);
    }
    return state;
}


/*
    Parse expression (leftHandSide operator rightHandSide)
 */
static int parseExpr(Js *ep, int state, int flags)
{
    char *lhs, *rhs;
    int  rel, tid;

    assert(ep);

    setString(&ep->result, "");
    rhs = lhs = NULL;
    rel = 0;
    tid = 0;

    do {
        /*
            This loop will handle an entire expression list. We call parse to evaluate each term which returns the
            result in ep->result.
         */
        if (tid == TOK_LOGICAL) {
            if ((state = parse(ep, STATE_RELEXP, flags)) != STATE_RELEXP_DONE) {
                state = STATE_ERR;
                break;
            }
        } else {
            if ((state = parse(ep, STATE_EXPR, flags)) != STATE_EXPR_DONE) {
                state = STATE_ERR;
                break;
            }
        }

        if (rel > 0) {
            setString(&rhs, ep->result);
            if (tid == TOK_LOGICAL) {
                if (evalCond(ep, lhs, rel, rhs) < 0) {
                    state = STATE_ERR;
                    break;
                }
            } else {
                if (evalExpr(ep, lhs, rel, rhs) < 0) {
                    state = STATE_ERR;
                    break;
                }
            }
        }
        setString(&lhs, ep->result);

        if ((tid = jsLexGetToken(ep, state)) == TOK_EXPR ||
            tid == TOK_INC_DEC || tid == TOK_LOGICAL) {
            rel = (int) *ep->token;

        } else {
            jsLexPutbackToken(ep, tid, ep->token);
            state = STATE_RELEXP_DONE;
        }

    } while (state == STATE_EXPR_DONE);

    if (rhs) {
        wfree(rhs);
    }
    if (lhs) {
        wfree(lhs);
    }
    return state;
}


/*
    Evaluate a condition. Implements &&, ||, !
 */
static int evalCond(Js *ep, cchar *lhs, int rel, cchar *rhs)
{
    char buf[16];
    int  l, r, lval;

    assert(lhs);
    assert(rhs);
    assert(rel > 0);

    lval = 0;
    if (isdigit((uchar) * lhs) && isdigit((uchar) * rhs)) {
        l = atoi(lhs);
        r = atoi(rhs);
        switch (rel) {
        case COND_AND:
            lval = l && r;
            break;
        case COND_OR:
            lval = l || r;
            break;
        default:
            jsError(ep, "Bad operator %d", rel);
            return -1;
        }
    } else {
        if (!isdigit((uchar) * lhs)) {
            jsError(ep, "Conditional must be numeric", lhs);
        } else {
            jsError(ep, "Conditional must be numeric", rhs);
        }
    }
    itosbuf(buf, sizeof(buf), lval, 10);
    setString(&ep->result, buf);
    return 0;
}


/*
    Evaluate an operation
 */
static int evalExpr(Js *ep, cchar *lhs, int rel, cchar *rhs)
{
    cchar *cp;
    char  buf[16];
    int   numeric, l, r, lval;

    assert(lhs);
    assert(rhs);
    assert(rel > 0);

    /*
        All of the characters in the lhs and rhs must be numeric
     */
    numeric = 1;
    for (cp = lhs; *cp; cp++) {
        if (!isdigit((uchar) * cp)) {
            numeric = 0;
            break;
        }
    }
    if (numeric) {
        for (cp = rhs; *cp; cp++) {
            if (!isdigit((uchar) * cp)) {
                numeric = 0;
                break;
            }
        }
    }
    if (numeric) {
        l = atoi(lhs);
        r = atoi(rhs);
        switch (rel) {
        case EXPR_PLUS:
            lval = l + r;
            break;
        case EXPR_INC:
            lval = l + 1;
            break;
        case EXPR_MINUS:
            lval = l - r;
            break;
        case EXPR_DEC:
            lval = l - 1;
            break;
        case EXPR_MUL:
            lval = l * r;
            break;
        case EXPR_DIV:
            if (r != 0) {
                lval = l / r;
            } else {
                lval = 0;
            }
            break;
        case EXPR_MOD:
            if (r != 0) {
                lval = l % r;
            } else {
                lval = 0;
            }
            break;
        case EXPR_LSHIFT:
            lval = l << r;
            break;
        case EXPR_RSHIFT:
            lval = l >> r;
            break;
        case EXPR_EQ:
            lval = l == r;
            break;
        case EXPR_NOTEQ:
            lval = l != r;
            break;
        case EXPR_LESS:
            lval = (l < r) ? 1 : 0;
            break;
        case EXPR_LESSEQ:
            lval = (l <= r) ? 1 : 0;
            break;
        case EXPR_GREATER:
            lval = (l > r) ? 1 : 0;
            break;
        case EXPR_GREATEREQ:
            lval = (l >= r) ? 1 : 0;
            break;
        case EXPR_BOOL_COMP:
            lval = (r == 0) ? 1 : 0;
            break;
        default:
            jsError(ep, "Bad operator %d", rel);
            return -1;
        }
    } else {
        switch (rel) {
        case EXPR_PLUS:
            clearString(&ep->result);
            appendString(&ep->result, lhs);
            appendString(&ep->result, rhs);
            return 0;
        case EXPR_LESS:
            lval = strcmp(lhs, rhs) < 0;
            break;
        case EXPR_LESSEQ:
            lval = strcmp(lhs, rhs) <= 0;
            break;
        case EXPR_GREATER:
            lval = strcmp(lhs, rhs) > 0;
            break;
        case EXPR_GREATEREQ:
            lval = strcmp(lhs, rhs) >= 0;
            break;
        case EXPR_EQ:
            lval = strcmp(lhs, rhs) == 0;
            break;
        case EXPR_NOTEQ:
            lval = strcmp(lhs, rhs) != 0;
            break;
        case EXPR_INC:
        case EXPR_DEC:
        case EXPR_MINUS:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        default:
            jsError(ep, "Bad operator");
            return -1;
        }
    }
    itosbuf(buf, sizeof(buf), lval, 10);
    setString(&ep->result, buf);
    return 0;
}


/*
    Evaluate a function
 */
static int evalFunction(Js *ep)
{
    WebsKey *sp;

    int (*fn)(int jid, void *handle, int argc, char **argv);

    if ((sp = hashLookup(ep->functions, ep->func->fname)) == NULL) {
        jsError(ep, "Undefined procedure %s", ep->func->fname);
        return -1;
    }
    fn = (int (*)(int, void*, int, char**)) sp->content.value.symbol;
    if (fn == NULL) {
        jsError(ep, "Undefined procedure %s", ep->func->fname);
        return -1;
    }
    return (*fn)(ep->jid, ep->userHandle, ep->func->nArgs, ep->func->args);
}


/*
    Output a parse js_error message
 */
PUBLIC void jsError(Js *ep, cchar *fmt, ...)
{
    va_list args;
    JsInput *ip;
    char    *errbuf, *msgbuf;

    assert(ep);
    assert(fmt);
    ip = ep->input;

    va_start(args, fmt);
    msgbuf = sfmtv(fmt, args);
    va_end(args);

    if (ep && ip) {
        errbuf = sfmt("%s\n At line %d, line => \n\n%s\n", msgbuf, ip->lineNumber, ip->line);
        wfree(ep->error);
        ep->error = errbuf;
    }
    wfree(msgbuf);
}


static void clearString(char **ptr)
{
    assert(ptr);

    if (*ptr) {
        wfree(*ptr);
    }
    *ptr = NULL;
}


static void setString(char **ptr, cchar *s)
{
    assert(ptr);

    if (*ptr) {
        wfree(*ptr);
    }
    *ptr = sclone(s);
}


static void appendString(char **ptr, cchar *s)
{
    ssize len, oldlen, size;

    assert(ptr);

    if (*ptr) {
        len = strlen(s);
        oldlen = strlen(*ptr);
        size = (len + oldlen + 1) * sizeof(char);
        *ptr = wrealloc(*ptr, size);
        strcpy(&(*ptr)[oldlen], s);
    } else {
        *ptr = sclone(s);
    }
}


/*
    Define a function
 */
PUBLIC int jsSetGlobalFunction(int jid, cchar *name, JsProc fn)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return jsSetGlobalFunctionDirect(ep->functions, name, fn);
}


/*
    Define a function directly into the function symbol table.
 */
PUBLIC int jsSetGlobalFunctionDirect(WebsHash functions, cchar *name, JsProc fn)
{
    if (hashEnter(functions, name, valueSymbol(fn), 0) == NULL) {
        return -1;
    }
    return 0;
}


/*
    Remove ("undefine") a function
 */
PUBLIC int jsRemoveGlobalFunction(int jid, cchar *name)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return hashDelete(ep->functions, name);
}


PUBLIC void *jsGetGlobalFunction(int jid, cchar *name)
{
    Js      *ep;
    WebsKey *sp;

    int (*fn)(int jid, void *handle, int argc, char **argv);

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }

    if ((sp = hashLookup(ep->functions, name)) != NULL) {
        fn = (int (*)(int, void*, int, char**)) sp->content.value.symbol;
        return (void*) fn;
    }
    return NULL;
}


/*
    Utility routine to crack Javascript arguments. Return the number of args
    seen. This routine only supports %s and %d type args.

    Typical usage:

        if (jsArgs(argc, argv, "%s %d", &name, &age) < 2) {
            error("Insufficient args");
            return -1;
        }
 */
PUBLIC int jsArgs(int argc, char **argv, cchar *fmt, ...)
{
    va_list vargs;
    cchar   *cp;
    char    **sp;
    int     *ip;
    int     argn;

    if (argv == NULL) {
        return 0;
    }

    va_start(vargs, fmt);
    for (argn = 0, cp = fmt; cp && *cp && argv[argn]; ) {
        if (*cp++ != '%') {
            continue;
        }
        switch (*cp) {
        case 'd':
            ip = va_arg(vargs, int*);
            *ip = atoi(argv[argn]);
            break;

        case 's':
            sp = va_arg(vargs, char**);
            *sp = argv[argn];
            break;

        default:
            /*
                Unsupported
             */
            assert(0);
        }
        argn++;
    }

    va_end(vargs);
    return argn;
}


PUBLIC void jsSetUserHandle(int jid, void *handle)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    ep->userHandle = handle;
}


void * jsGetUserHandle(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    return ep->userHandle;
}


PUBLIC int jsGetLineNumber(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return ep->input->lineNumber;
}


PUBLIC void jsSetResult(int jid, cchar *s)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    setString(&ep->result, s);
}


PUBLIC cchar *jsGetResult(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    return ep->result;
}

/*
    Set a variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in the
    top-most variable frame.
 */
PUBLIC void jsSetVar(int jid, cchar *var, cchar *value)
{
    Js        *ep;
    WebsValue v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[ep->variableMax - 1] - JS_OFFSET, var, v, 0);
}


/*
    Set a local variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in
    the top-most variable frame.
 */
PUBLIC void jsSetLocalVar(int jid, cchar *var, cchar *value)
{
    Js        *ep;
    WebsValue v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[ep->variableMax - 1] - JS_OFFSET, var, v, 0);
}


/*
    Set a global variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in
    the global variable frame.
 */
PUBLIC void jsSetGlobalVar(int jid, cchar *var, cchar *value)
{
    Js        *ep;
    WebsValue v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[0] - JS_OFFSET, var, v, 0);
}


/*
    Get a variable
 */
PUBLIC int jsGetVar(int jid, cchar *var, cchar **value)
{
    Js      *ep;
    WebsKey *sp;
    int     i;

    assert(var && *var);
    assert(value);

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    i = ep->variableMax - 1;
    if ((sp = hashLookup(ep->variables[i] - JS_OFFSET, var)) == NULL) {
        i = 0;
        if ((sp = hashLookup(ep->variables[0] - JS_OFFSET, var)) == NULL) {
            return -1;
        }
    }
    assert(sp->content.type == string);
    *value = sp->content.value.string;
    return i;
}


WebsHash jsGetVariableTable(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return *ep->variables;
}


WebsHash jsGetFunctionTable(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return ep->functions;
}


/*
    Free an argument list
 */
static void freeFunc(JsFun *func)
{
    int i;

    for (i = func->nArgs - 1; i >= 0; i--) {
        wfree(func->args[i]);
        func->nArgs = wfreeHandle(&func->args, i);
    }
    if (func->fname) {
        wfree(func->fname);
        func->fname = NULL;
    }
}


/*
    Get Javascript engine pointer
 */
static Js *jsPtr(int jid)
{
    assert(0 <= jid && jid < jsMax);

    if (jid < 0 || jid >= jsMax || jsHandles[jid] == NULL) {
        jsError(NULL, "Bad handle %d", jid);
        return NULL;
    }
    return jsHandles[jid];
}


/*
    This function removes any new lines.  Used for else cases, etc.
 */
static void jsRemoveNewlines(Js *ep, int state)
{
    int tid;

    do {
        tid = jsLexGetToken(ep, state);
    } while (tid == TOK_NEWLINE);
    jsLexPutbackToken(ep, tid, ep->token);
}


PUBLIC int jsLexOpen(Js *ep)
{
    return 0;
}


PUBLIC void jsLexClose(Js *ep)
{
}


PUBLIC int jsLexOpenScript(Js *ep, cchar *script)
{
    JsInput *ip;

    assert(ep);
    assert(script);

    if ((ep->input = walloc(sizeof(JsInput))) == NULL) {
        return -1;
    }
    ip = ep->input;
    memset(ip, 0, sizeof(*ip));

    assert(ip);
    assert(ip->putBackToken == NULL);
    assert(ip->putBackTokenId == 0);

    /*
        Create the parse token buffer
     */
    if (bufCreate(&ip->tokbuf, JS_MAX_TOKEN, 0) < 0) {
        return -1;
    }

    /*
        Put the Javascript into a ring queue for easy parsing
     */
    int len = (int) slen(script);
    if (bufCreate(&ip->script, len, 0) < 0) {
        return -1;
    }
    bufPutStr(&ip->script, script);

    ip->lineNumber = 1;
    ip->lineLength = 0;
    ip->lineColumn = 0;
    ip->line = NULL;

    return 0;
}


PUBLIC void jsLexCloseScript(Js *ep)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    if (ip->putBackToken) {
        wfree(ip->putBackToken);
        ip->putBackToken = NULL;
    }
    ip->putBackTokenId = 0;

    if (ip->line) {
        wfree(ip->line);
        ip->line = NULL;
    }
    bufFree(&ip->tokbuf);
    bufFree(&ip->script);
    wfree(ip);
}


PUBLIC void jsLexSaveInputState(Js *ep, JsInput *state)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    *state = *ip;

    if (ip->putBackToken) {
        state->putBackToken = sclone(ip->putBackToken);
    }
}


PUBLIC void jsLexRestoreInputState(Js *ep, JsInput *state)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    ip->tokbuf = state->tokbuf;
    ip->script = state->script;
    ip->putBackTokenId = state->putBackTokenId;
    if (ip->putBackToken) {
        wfree(ip->putBackToken);
        ip->putBackToken = NULL;
    }
    if (state->putBackToken) {
        ip->putBackToken = sclone(state->putBackToken);
    }
}


PUBLIC void jsLexFreeInputState(Js *ep, JsInput *state)
{
    if (state->putBackToken) {
        wfree(state->putBackToken);
        state->putBackToken = NULL;
    }
}


PUBLIC int jsLexGetToken(Js *ep, int state)
{
    ep->tid = getLexicalToken(ep, state);
    return ep->tid;
}


static int getLexicalToken(Js *ep, int state)
{
    WebsBuf *tokq;
    JsInput *ip;
    int     done, tid, c, quote, style;

    assert(ep);
    ip = ep->input;
    assert(ip);

    tokq = &ip->tokbuf;
    ep->tid = -1;
    tid = -1;
    ep->token = "";
    bufFlush(tokq);

    if (ip->putBackTokenId > 0) {
        bufPutStr(tokq, ip->putBackToken);
        tid = ip->putBackTokenId;
        ip->putBackTokenId = 0;
        ep->token = (char*) tokq->servp;
        return tid;
    }
    if ((c = inputGetc(ep)) < 0) {
        return TOK_EOF;
    }
    for (done = 0; !done; ) {
        switch (c) {
        case -1:
            return TOK_EOF;

        case ' ':
        case '\t':
        case '\r':
            do {
                if ((c = inputGetc(ep)) < 0)
                    break;
            } while (c == ' ' || c == '\t' || c == '\r');
            break;

        case '\n':
            return TOK_NEWLINE;

        case '(':
            tokenAddChar(ep, c);
            return TOK_LPAREN;

        case ')':
            tokenAddChar(ep, c);
            return TOK_RPAREN;

        case '{':
            tokenAddChar(ep, c);
            return TOK_LBRACE;

        case '}':
            tokenAddChar(ep, c);
            return TOK_RBRACE;

        case '+':
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '+' ) {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_PLUS);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_INC);
            return TOK_INC_DEC;

        case '-':
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '-' ) {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_MINUS);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_DEC);
            return TOK_INC_DEC;

        case '*':
            tokenAddChar(ep, EXPR_MUL);
            return TOK_EXPR;

        case '%':
            tokenAddChar(ep, EXPR_MOD);
            return TOK_EXPR;

        case '/':
            /*
                Handle the division operator and comments
             */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '*' && c != '/') {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_DIV);
                return TOK_EXPR;
            }
            style = c;
            /*
                Eat comments. Both C and C++ comment styles are supported.
             */
            while (1) {
                if ((c = inputGetc(ep)) < 0) {
                    jsError(ep, "Syntax Error");
                    return TOK_ERR;
                }
                if (c == '\n' && style == '/') {
                    break;
                } else if (c == '*') {
                    c = inputGetc(ep);
                    if (style == '/') {
                        if (c == '\n') {
                            break;
                        }
                    } else {
                        if (c == '/') {
                            break;
                        }
                    }
                }
            }
            /*
                Continue looking for a token, so get the next character
             */
            if ((c = inputGetc(ep)) < 0) {
                return TOK_EOF;
            }
            break;

        case '<':                                   /* < and <= */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '<') {
                tokenAddChar(ep, EXPR_LSHIFT);
                return TOK_EXPR;
            } else if (c == '=') {
                tokenAddChar(ep, EXPR_LESSEQ);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_LESS);
            inputPutback(ep, c);
            return TOK_EXPR;

        case '>':                                   /* > and >= */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '>') {
                tokenAddChar(ep, EXPR_RSHIFT);
                return TOK_EXPR;
            } else if (c == '=') {
                tokenAddChar(ep, EXPR_GREATEREQ);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_GREATER);
            inputPutback(ep, c);
            return TOK_EXPR;

        case '=':                                   /* "==" */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '=') {
                tokenAddChar(ep, EXPR_EQ);
                return TOK_EXPR;
            }
            inputPutback(ep, c);
            return TOK_ASSIGNMENT;

        case '!':                                   /* "!=" or "!"*/
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '=') {
                tokenAddChar(ep, EXPR_NOTEQ);
                return TOK_EXPR;
            }
            inputPutback(ep, c);
            tokenAddChar(ep, EXPR_BOOL_COMP);
            return TOK_EXPR;

        case ';':
            tokenAddChar(ep, c);
            return TOK_SEMI;

        case ',':
            tokenAddChar(ep, c);
            return TOK_COMMA;

        case '|':                                   /* "||" */
            if ((c = inputGetc(ep)) < 0 || c != '|') {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            tokenAddChar(ep, COND_OR);
            return TOK_LOGICAL;

        case '&':                                   /* "&&" */
            if ((c = inputGetc(ep)) < 0 || c != '&') {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            tokenAddChar(ep, COND_AND);
            return TOK_LOGICAL;

        case '\"':                                  /* String quote */
        case '\'':
            quote = c;
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }

            while (c != quote) {
                /*
                    check for escape sequence characters
                 */
                if (c == '\\') {
                    c = inputGetc(ep);

                    if (isdigit((uchar) c)) {
                        /*
                            octal support, \101 maps to 65 = 'A'. put first char back so converter will work properly.
                         */
                        inputPutback(ep, c);
                        c = charConvert(ep, OCTAL, 3);

                    } else {
                        switch (c) {
                        case 'n':
                            c = '\n'; break;
                        case 'b':
                            c = '\b'; break;
                        case 'f':
                            c = '\f'; break;
                        case 'r':
                            c = '\r'; break;
                        case 't':
                            c = '\t'; break;
                        case 'x':
                            /*
                                hex support, \x41 maps to 65 = 'A'
                             */
                            c = charConvert(ep, HEX, 2);
                            break;
                        case 'u':
                            /*
                                unicode support, \x0401 maps to 65 = 'A'
                             */
                            c = charConvert(ep, HEX, 2);
                            c = c * 16 + charConvert(ep, HEX, 2);

                            break;
                        case '\'':
                        case '\"':
                        case '\\':
                            break;
                        default:
                            jsError(ep, "Invalid Escape Sequence");
                            return TOK_ERR;
                        }
                    }
                    if (tokenAddChar(ep, c) < 0) {
                        return TOK_ERR;
                    }
                } else {
                    if (tokenAddChar(ep, c) < 0) {
                        return TOK_ERR;
                    }
                }
                if ((c = inputGetc(ep)) < 0) {
                    jsError(ep, "Unmatched Quote");
                    return TOK_ERR;
                }
            }
            return TOK_LITERAL;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            do {
                if (tokenAddChar(ep, c) < 0) {
                    return TOK_ERR;
                }
                if ((c = inputGetc(ep)) < 0)
                    break;
            } while (isdigit((uchar) c));
            inputPutback(ep, c);
            return TOK_LITERAL;

        default:
            /*
                Identifiers or a function names
             */
            while (1) {
                if (c == '\\') {
                    /*
                        just ignore any \ characters.
                     */
                } else if (tokenAddChar(ep, c) < 0) {
                    break;
                }
                if ((c = inputGetc(ep)) < 0) {
                    break;
                }
                if (!isalnum((uchar) c) && c != '$' && c != '_' && c != '\\') {
                    break;
                }
            }
            if (!isalpha((uchar) * tokq->servp) && *tokq->servp != '$' && *tokq->servp != '_') {
                jsError(ep, "Invalid identifier %s", tokq->servp);
                return TOK_ERR;
            }
            /*
                Check for reserved words (only "if", "else", "var", "for" and "return" at the moment)
             */
            if (state == STATE_STMT) {
                if (strcmp(ep->token, "if") == 0) {
                    return TOK_IF;
                } else if (strcmp(ep->token, "else") == 0) {
                    return TOK_ELSE;
                } else if (strcmp(ep->token, "var") == 0) {
                    return TOK_VAR;
                } else if (strcmp(ep->token, "for") == 0) {
                    return TOK_FOR;
                } else if (strcmp(ep->token, "return") == 0) {
                    if ((c == ';') || (c == '(')) {
                        inputPutback(ep, c);
                    }
                    return TOK_RETURN;
                }
            }
            /*
                Skip white space after token to find out whether this is a function or not.
             */
            while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if ((c = inputGetc(ep)) < 0)
                    break;
            }

            tid = (c == '(') ? TOK_FUNCTION : TOK_ID;
            done++;
        }
    }

    /*
        Putback the last extra character for next time
     */
    inputPutback(ep, c);
    return tid;
}


PUBLIC void jsLexPutbackToken(Js *ep, int tid, cchar *string)
{
    JsInput *ip;

    assert(ep);
    ip = ep->input;
    assert(ip);

    if (ip->putBackToken) {
        wfree(ip->putBackToken);
        ip->putBackToken = NULL;
    }
    ip->putBackTokenId = tid;
    ip->putBackToken = sclone(string);
}


static int tokenAddChar(Js *ep, int c)
{
    JsInput *ip;

    assert(ep);
    ip = ep->input;
    assert(ip);

    if (bufPutc(&ip->tokbuf, (char) c) < 0) {
        jsError(ep, "Token too big");
        return -1;
    }
    *((char*) ip->tokbuf.endp) = '\0';
    ep->token = (char*) ip->tokbuf.servp;

    return 0;
}


static int inputGetc(Js *ep)
{
    JsInput *ip;
    ssize   len;
    int     c;

    assert(ep);
    ip = ep->input;

    if ((len = bufLen(&ip->script)) == 0) {
        return -1;
    }
    c = bufGetc(&ip->script);
    if (c == '\n') {
        ip->lineNumber++;
        ip->lineColumn = 0;
    } else {
        if ((ip->lineColumn + 2) >= ip->lineLength) {
            ip->lineLength += JS_INC;
            ip->line = wrealloc(ip->line, ip->lineLength * sizeof(char));
        }
        ip->line[ip->lineColumn++] = c;
        ip->line[ip->lineColumn] = '\0';
    }
    return c;
}


static void inputPutback(Js *ep, int c)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    bufInsertc(&ip->script, (char) c);
    /* Fix by Fred Sauer, 2002/12/23 */
    if (ip->lineColumn > 0) {
        ip->lineColumn--;
    }
    ip->line[ip->lineColumn] = '\0';
}


/*
    Convert a hex or octal character back to binary, return original char if not a hex digit
 */
static int charConvert(Js *ep, int base, int maxDig)
{
    int i, c, lval, convChar;

    lval = 0;
    for (i = 0; i < maxDig; i++) {
        if ((c = inputGetc(ep)) < 0) {
            break;
        }
        /*
            Initialize to out of range value
         */
        convChar = base;
        if (isdigit((uchar) c)) {
            convChar = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            convChar = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            convChar = c - 'A' + 10;
        }
        /*
            if unexpected character then return it to buffer.
         */
        if (convChar >= base) {
            inputPutback(ep, c);
            break;
        }
        lval = (lval * base) + convChar;
    }
    return lval;
}

#endif /* ME_GOAHEAD_JAVASCRIPT */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
