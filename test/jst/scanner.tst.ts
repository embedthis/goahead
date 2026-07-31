/*
    scanner.tst - JST template scanning (src/jst.c)

    Covers how jstHandler splits a page into HTML text and <% %> script blocks: text passthrough,
    multiple blocks, the "language=" prefix, empty blocks, backslash line continuation, the .asp
    extension, and the unterminated-block error.

    Several assertions pin current behaviour that is a scanner limitation rather than a requirement.
    Each is called out in a comment so a future change to the scanner is a deliberate decision and
    not a silent break.
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
    //  Surrounding HTML is passed through and the block is replaced by its output
    let http = await fetch('/jst/text.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == '<html>ABC</html>')

    //  The .asp extension routes to the same handler and behaves identically
    http = await fetch('/jst/text.asp')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == '<html>ABC</html>')

    //  Several blocks in one page, each with literal text between them
    http = await fetch('/jst/multi.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == '1a2b3')

    //  A "language=javascript" prefix is consumed and the remainder runs as script
    http = await fetch('/jst/lang.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'ok')

    /*
        Any other language is NOT consumed: the scanner rewinds and the whole block, "language"
        token included, is handed to the interpreter, which fails on the unknown identifier.
        Pinned as current behaviour - an unsupported language is reported as a script error rather
        than being skipped or rejected by the scanner.
     */
    http = await fetch('/jst/langbad.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Javascript Error')
    tcontains(http.response, 'Undefined variable vbscript')

    //  A block with only whitespace produces no output and is not an error
    http = await fetch('/jst/empty.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'XY')

    //  A backslash before a newline splices the script line
    http = await fetch('/jst/cont.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'onetwo')

    /*
        The scanner looks for the first "%>" with no knowledge of string literals, so a "%>" inside
        a quoted string ends the block early and the truncated script fails to parse. Pinned as a
        known scanner limitation: templates must not embed "%>" in a string.
     */
    http = await fetch('/jst/strpct.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Javascript Error')
    tcontains(http.response, 'Unmatched Quote')

    /*
        A block with no closing "%>" is reported through websError. The response headers were
        already flushed before the scan began, so the status stays 200 and the error page is
        appended to the text emitted so far.
     */
    http = await fetch('/jst/unterm.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Internal Server Error')

    //  The server is still healthy after every scanner error above
    http = await fetch('/index.html')
    ttrue(http.status == 200)
}
