/*
    procs.tst - JST procedure definition and dispatch

    Templates cannot define functions; the interpreter has no "function" keyword. A name followed by
    "(" is dispatched to a procedure registered by the application with websDefineJst. The test
    server registers "aspTest" and "bigTest" (src/test.c) alongside the built-in "write"
    (websJstWrite, src/jst.c).
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
    //  write with several arguments joins them with a single space
    let http = await fetch('/jst/write2.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'a b')

    //  A registered procedure receives its evaluated arguments in order
    http = await fetch('/jst/asp1.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'Name: John, Address 700 Park Ave')

    /*
        Too few arguments: aspTest's jsArgs("%s %s") check fails and the procedure answers with
        websError(400). No body has been written, so the response retains the callback's 400 status.
     */
    http = await fetch('/jst/asp0.jst')
    ttrue(http.status == 400)
    tcontains(http.response, 'Bad Request')

    //  A procedure that writes a large response streams it all
    http = await fetch('/jst/bigtest.jst')
    ttrue(http.status == 200)
    ttrue(http.response.length > 60000)
    tcontains(http.response, 'Line: 00000')
    tcontains(http.response, 'Line: 00799')

    //  An unregistered name is a script error, and output written before it is kept
    http = await fetch('/jst/nofunc.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Before')
    tcontains(http.response, 'Javascript Error')

    http = await fetch('/index.html')
    ttrue(http.status == 200)
}
