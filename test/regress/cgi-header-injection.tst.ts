/*
    cgi-header-injection.tst - Regression test for CGI response header injection.

    parseCgiHeaders() used to split a CGI response header into name and value BEFORE splitting the
    header block into lines, so "value" spanned the whole remaining block, CR/LF included. The
    pass-through branch wrote that entire span as one header value: an injected header rode inside
    the previous header's value and never reached the key sanitizer, each following line was
    re-parsed and re-emitted, and the CGI's own Content-Type was swallowed into the carrier value.

    Assert the parser now tokenizes lines first, validates both name and value, and that a header
    name the sanitizer rejects cannot be smuggled through inside another header's value.

    Note the server cannot tell a deliberately-emitted header from an injected one -- a CGI that
    writes a raw CRLF into its output has written two header lines, and both are delivered if both
    are well formed. What this test pins down is that every delivered name passes the sanitizer and
    no delivered value carries CR/LF.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

if (thas('ME_GOAHEAD_CGI')) {

    /*
        cgitest -H emits its argument verbatim as a response header line. getArgv descapes the
        switch string once and splits it on whitespace, so the header text is double-encoded here:
        %2520 survives the first descape as %20, and %250d%250a delivers a raw CRLF to the parser.
     */
    async function cgiHeader(encoded: string): Promise<Http> {
        let http: Http = new Http
        http.setHeader('SWITCHES', '-H%20' + encoded)
        http.get(HTTP + '/cgi-bin/cgitest')
        await http.wait()
        ttrue(http.status == 200)
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
        A CRLF in the value used to carry the rest of the block through as one value. The carrier
        header must now hold only its own value. A compliant client re-splits spliced bytes into
        separate headers, so these assertions alone cannot distinguish the two builds -- the
        sanitizer checks below are what pin the security control.
     */
    let http = await cgiHeader('X-Reflect:%2520a%250d%250aX-Injected:%2520pwned')
    ttrue(http.header('x-reflect') == 'a')
    ttrue(http.header('x-reflect').indexOf('X-Injected') < 0)
    ttrue(http.header('x-reflect').indexOf('\r') < 0)
    ttrue(http.header('x-reflect').indexOf('\n') < 0)
    http.close()

    /*
        The sanitizer rejects "<evil>" as a header name when the CGI emits it directly. It must
        also reject it when the same bytes arrive inside a preceding header's value -- that was the
        bypass.
     */
    http = await cgiHeader('X-Reflect:%2520a%250d%250a%253cevil%253e:%2520x')
    ttrue(http.header('x-reflect') == 'a')
    ttrue(http.header('<evil>') == null)
    http.close()

    /* A name the sanitizer rejects is dropped when emitted directly */
    http = await cgiHeader('%253cevil%253e:%2520x')
    ttrue(http.header('<evil>') == null)
    http.close()

    /*
        The CGI's Content-Type used to be swallowed into the carrier header's value. cgitest emits
        Content-Type ahead of the -H header, so it must survive intact.
     */
    http = await cgiHeader('X-Reflect:%2520a%250d%250aX-Injected:%2520pwned')
    ttrue(http.header('content-type') == 'text/html')
    http.close()

    /* Each header is emitted once -- the old loop re-parsed and re-emitted every following line */
    http = await cgiHeader('X-Reflect:%2520a%250d%250aX-Injected:%2520pwned')
    ttrue(http.header('x-injected') == 'pwned')
    http.close()

    /* A line with no colon is not a header and must not become one */
    http = await cgiHeader('X-Reflect:%2520a%250d%250agarbage')
    ttrue(http.header('x-reflect') == 'a')
    ttrue(http.header('garbage') == null)
    http.close()

    /* A well-formed custom header is still passed through unchanged */
    http = await cgiHeader('X-CGI-Custom:%2520plain%2520value')
    ttrue(http.header('x-cgi-custom') == 'plain value')
    http.close()

    await serverStillAlive()

} else {
    tskip('CGI not enabled')
}
