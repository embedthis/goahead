/*
    Script errors stop the template and return a generic report. Output already written is retained;
    interpreter diagnostics and template source must never appear in the HTTP body.
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
        failure generically, and the template text after the failing block is dropped.
     */
    let http = await fetch('/jst/syntax.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Before')
    tcontains(http.response, 'mid')
    tcontains(http.response, 'Middle')
    tcontains(http.response, 'Javascript Error')
    ttrue(http.response.indexOf('After') < 0)

    //  An undefined variable read does not disclose its name
    http = await fetch('/jst/undef.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Before')
    tcontains(http.response, 'Javascript Error')
    ttrue(http.response.indexOf('After') < 0)

    /*
        Neither request parameters nor template source are included in a generic error response.
     */
    http = await fetch('/jst/undef.jst?probe=%3Cscript%3Ealert(1)%3C/script%3E')
    ttrue(http.status == 200)
    ttrue(http.response.indexOf('<script>alert(1)</script>') < 0)

    //  A script error still terminates the response cleanly and the connection is reusable
    http = await fetch('/index.html')
    ttrue(http.status == 200)
}
