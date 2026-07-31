/*
    cgi-headerless-output.tst - Regression test for headerless CGI output.

    When a CGI emitted no recognizable header block, websCgiGatherOutput() wrote a default header
    set and then the program output -- without ever writing the end-of-headers marker. The output
    landed inside the header block, so any "Name: value" line the program emitted became a real
    response header. A CGI echoing a request header was therefore a response-header injection
    needing no CR/LF in the attacker's input. The missing marker also left WEBS_HEADERS_CREATED
    clear (so the output was re-parsed as headers on the next read) and WEBS_CHUNKING clear (so the
    body was sent unchunked despite the advertised Transfer-Encoding, desyncing the connection).

    Two defects on the same path are covered here as well: parseCgiHeaders() wrote its NUL
    terminator before deciding the block held no headers, corrupting the body it handed back; and
    it emitted the response status line at the first pass-through header, silently dropping any
    Status/Content-Type/Content-Length/Location line that followed one.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

if (thas('ME_GOAHEAD_CGI')) {

    /*
        cgitest -r emits its argument as the complete response, verbatim -- no headers or body of
        cgitest's own. getArgv descapes the switch string once and splits it on whitespace, so the
        response text is double-encoded here: %2520 survives the first descape as %20, and
        %250d%250a delivers a raw CRLF.
     */
    async function cgiRaw(encoded: string): Promise<Http> {
        let http: Http = new Http
        http.setHeader('SWITCHES', '-r%20' + encoded)
        http.get(HTTP + '/cgi-bin/cgitest')
        await http.wait()
        return http
    }

    async function serverStillAlive() {
        let http: Http = new Http
        http.get(HTTP + '/index.html')
        await http.wait()
        ttrue(http.status == 200)
        http.close()
    }

    /*
        The injection. A headerless CGI whose output is a single header-shaped line used to deliver
        that line as a response header. It must arrive as body text instead, and the response must
        terminate -- the unclosed header block used to leave the client waiting.
     */
    let http = await cgiRaw('Set-Cookie:%2520session=attacker%250a')
    ttrue(http.status == 200)
    ttrue(http.header('set-cookie') == null)
    ttrue(http.response.indexOf('Set-Cookie: session=attacker') >= 0)
    http.close()

    /* Headerless output is given a Content-Type so it cannot be sniffed as HTML */
    http = await cgiRaw('%253cscript%253ealert(1)%253c/script%253e%250a')
    ttrue(http.header('content-type') == 'text/plain')
    ttrue(http.response.indexOf('<script>') >= 0)
    http.close()

    /*
        Headerless output is passed through byte for byte. The parser used to NUL the first byte of
        an embedded CRLFCRLF before deciding the block held no headers, so the body arrived with
        that byte destroyed.
     */
    http = await cgiRaw('plain%2520body%250d%250a%250d%250amore%2520text')
    ttrue(http.response == 'plain body\r\n\r\nmore text')
    http.close()

    /*
        The framing must be honest. Headerless responses advertise chunked encoding, and the body
        used to be written unchunked -- the connection desynced and could not be reused.
     */
    http = await cgiRaw('Set-Cookie:%2520session=attacker%250a')
    ttrue(http.header('transfer-encoding') == 'chunked')
    http.close()
    await serverStillAlive()

    /*
        A control header following a pass-through header must still shape the response. The status
        line used to be emitted at the first pass-through header, so these were parsed and then
        discarded. RFC 3875 places no ordering requirement on CGI response headers.
     */
    http = await cgiRaw('X-Foo:%25201%250d%250aContent-Type:%2520text/plain%250d%250a%250d%250abody')
    ttrue(http.status == 200)
    ttrue(http.header('x-foo') == '1')
    ttrue(http.header('content-type') == 'text/plain')
    ttrue(http.response == 'body')
    http.close()

    http = await cgiRaw('X-Foo:%25201%250d%250aStatus:%2520404%250d%250a%250d%250abody')
    ttrue(http.status == 404)
    ttrue(http.header('x-foo') == '1')
    http.close()

    /* A control header is consumed, not passed through as well */
    http = await cgiRaw('Content-Type:%2520text/plain%250d%250aX-Foo:%25201%250d%250a%250d%250abody')
    ttrue(http.header('content-type') == 'text/plain')
    ttrue(http.header('x-foo') == '1')
    http.close()

    await serverStillAlive()

} else {
    tskip('CGI not enabled')
}
