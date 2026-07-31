/*
    cgi-content-type-format.tst - Regression test for the CGI Content-Type format string.

    writeCgiHeaders() used to pass the CGI's Content-Type value as the printf *format* argument of
    websWriteHeader(). A CGI emitting "Content-Type: %s" then walked undefined vararg slots and
    copied heap memory into the response header; "Content-Type: %n" wrote through an undefined slot
    and crashed the single-threaded server. Assert the value is echoed verbatim and the server
    survives to serve the next request.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

if (thas('ME_GOAHEAD_CGI')) {

    /*
        The switch string is descaped before it is split on whitespace, so "%20" is the argument
        separator and "%25" delivers a literal '%' into the CGI's argv.
     */
    async function contentTypeIsNotAFormat(encoded: string, expected: string) {
        let http: Http = new Http
        http.setHeader('SWITCHES', '-c%20' + encoded)
        http.get(HTTP + '/cgi-bin/cgitest')
        await http.wait()
        ttrue(http.status == 200)
        ttrue(http.header('content-type') == expected)
        http.close()
    }

    async function serverStillAlive() {
        let http: Http = new Http
        http.get(HTTP + '/index.html')
        await http.wait()
        ttrue(http.status == 200)
        http.close()
    }

    /* %s leaked heap memory into the header value */
    await contentTypeIsNotAFormat('%25s%25s%25s', '%s%s%s')
    await serverStillAlive()

    /* %n wrote through an undefined vararg slot and crashed the server */
    await contentTypeIsNotAFormat('%25s%25s%25s%25n', '%s%s%s%n')
    await serverStillAlive()

    /* Width and precision specifiers also consume vararg slots */
    await contentTypeIsNotAFormat('text/plain;%25999999999d', 'text/plain;%999999999d')
    await serverStillAlive()

    /* A well-formed Content-Type is still passed through unchanged */
    await contentTypeIsNotAFormat('application/json', 'application/json')

} else {
    tskip('CGI not enabled')
}
