/*
    language.tst - The embedded JavaScript interpreter (src/js.c)

    GoAhead's interpreter is a deliberately small subset of JavaScript. The reserved words are only
    "if", "else", "var", "for" and "return" (src/js.c, jsLexGetToken) and expressions are evaluated
    strictly left to right. These tests pin that subset: what works, and which familiar constructs
    are NOT part of it, so a future language change is visible here.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

async function fetch(path: string): Promise<Http> {
    let http: Http = new Http
    http.get(HTTP + path)
    await http.wait()
    return http
}

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    /*
        Arithmetic. There is no operator precedence: "2 + 3 * 4" evaluates left to right and yields
        20, not 14, and division is integer division. Pinned as the documented behaviour of the
        cut-down interpreter - a template needing precedence must parenthesise, and even then the
        grouping only matters where the left-to-right order differs.
     */
    let http = await fetch('/jst/numbers.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'a=20 b=20 c=2 d=4 e=1')

    //  var with an initialiser, a bare var (declared but undefined), and frame lifetime across blocks
    http = await fetch('/jst/decl.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'a=41 b=[]')

    /*
        Redeclaring with an initialiser is an assignment and is accepted. Only a bare re-declaration
        of a name already declared in the same frame is rejected (see vars.tst).
     */
    http = await fetch('/jst/redecl.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'a=2')

    //  if / else and for
    http = await fetch('/jst/flow.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'then otherwise for: 0 1 2')

    /*
        "while" is not a reserved word in this interpreter. It lexes as an identifier followed by
        "(", so it is dispatched as a procedure call and fails. Pinned so that adding "while" to the
        language is a deliberate change.
     */
    http = await fetch('/jst/while.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Undefined procedure while')

    //  Postfix ++ and -- are supported
    http = await fetch('/jst/incdec.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'i=7 j=2')

    //  Prefix ++ is not - it is a syntax error. Pinned as a known gap.
    http = await fetch('/jst/preinc.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Syntax error')

    //  Logical and relational operators
    http = await fetch('/jst/logical.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'and or not rel')

    //  String literals in both quote styles, escapes, and number-to-string coercion by "+"
    http = await fetch('/jst/strings.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'hello world')
    tcontains(http.response, '[\t]')
    tcontains(http.response, '[\\]')
    tcontains(http.response, '["]')
    tcontains(http.response, 'num=42')
}
