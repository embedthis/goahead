/*
    vars.tst - Request variables and script variable scope

    jstHandler runs the page with the request variable table (wp->vars) as the engine's global frame
    and pushes a separate frame for the script itself. So a template reads query and form parameters
    by name, an assignment to an undeclared name defines a request variable, and a "var" declaration
    shadows a parameter instead of overwriting it.

    The shadow tests are the regression for the frame defect: a bare "var NAME" colliding with a request
    parameter used to free the parameter's value and then read it back (heap use-after-free).
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

/*
    Long enough that a freed-then-reused chunk is unlikely to still read back intact. The GET value has
    to fit inside ME_GOAHEAD_LIMIT_HEADER (2048) with the rest of the request line; the POST body is
    bounded by the much larger post limit, so it carries the bigger value.
 */
const FILLER = 'A'.repeat(1200)
const BIG = 'B'.repeat(8000)

async function fetch(path: string): Promise<Http> {
    let http: Http = new Http
    http.get(HTTP + path)
    await http.wait()
    return http
}

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    //  A query parameter is visible to the script by name
    let http = await fetch('/jst/query.jst?q=hello')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'q=[hello]')

    //  So is a POST form parameter
    http = new Http
    http.form(HTTP + '/jst/query.jst', {q: 'posted'})
    await http.wait()
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'q=[posted]')

    //  Request variables do not survive into the next request
    http = await fetch('/jst/query.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Undefined variable q')

    //  An assignment to an undeclared name defines a request variable for the rest of the page
    http = await fetch('/jst/setvar.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'v=set')

    //  ... and that variable is gone on the next request
    http = await fetch('/jst/readvar.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Undefined variable newvar')

    /*
        A bare declaration of a name that is also a request parameter must not disturb
        the parameter: text emitted before the declaration sees the supplied value in full, and the
        declared variable reads as undefined afterwards.
     */
    http = await fetch('/jst/shadow.jst?q=' + FILLER)
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'before=[' + FILLER + '] after=[]')

    //  Same through a POST form parameter, with a much larger value
    http = new Http
    http.form(HTTP + '/jst/shadow.jst', {q: BIG})
    await http.wait()
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'before=[' + BIG + '] after=[]')

    //  Repeat the collision so a stale pointer would land in reused heap
    for (let i = 0; i < 50; i++) {
        http = await fetch('/jst/shadow.jst?q=' + FILLER)
        ttrue(http.status == 200)
        ttrue(http.response.trim() == 'before=[' + FILLER + '] after=[]')
    }

    //  The server survived the collision campaign
    http = await fetch('/index.html')
    ttrue(http.status == 200)

    //  A declared variable can be assigned in its own frame without touching the parameter
    http = await fetch('/jst/shadowset.jst?q=fromquery')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'q=local')

    //  A bare re-declaration of a name already declared by the script is rejected
    http = await fetch('/jst/redecl2.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Variable already declared: z')

    //  Declaring a name that only exists as a request parameter is NOT a re-declaration
    http = await fetch('/jst/shadow.jst?q=x')
    ttrue(http.status == 200)
    ttrue(http.response.indexOf('already declared') < 0)
}
