/*
    errors.tst - Script error reporting and output semantics

    On a script error jstHandler appends an error report to whatever the page has already written
    and stops processing the rest of the template. The response headers were flushed before the
    first block ran, so the status is always 200.

    The error report echoes the failing script back to the client inside <pre>. These tests pin what
    reaches the browser, including that a request parameter cannot get into that echo.
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
        A syntax error. Everything written before the failing block is kept, the error names the
        problem and echoes the script, and the template text after the failing block is dropped.
     */
    let http = await fetch('/jst/syntax.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Before')
    tcontains(http.response, 'mid')
    tcontains(http.response, 'Middle')
    tcontains(http.response, 'Javascript Error')
    ttrue(http.response.indexOf('After') < 0)

    //  An undefined variable read names the variable
    http = await fetch('/jst/undef.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Before')
    tcontains(http.response, 'Undefined variable nosuchvariable')
    ttrue(http.response.indexOf('After') < 0)

    /*
        The error echo prints the script text from the template, never the request. A parameter
        value cannot reach the echoed <pre> block, so a query string is not an injection vector into
        the error page.
     */
    http = await fetch('/jst/undef.jst?probe=%3Cscript%3Ealert(1)%3C/script%3E')
    ttrue(http.status == 200)
    ttrue(http.response.indexOf('<script>alert(1)</script>') < 0)

    //  A script error still terminates the response cleanly and the connection is reusable
    http = await fetch('/index.html')
    ttrue(http.status == 200)
}
