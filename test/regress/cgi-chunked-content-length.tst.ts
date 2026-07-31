/*
    cgi-chunked-content-length.tst - Regression test for CONTENT_LENGTH on chunked and body-less requests.

    wp->rxLen was initialised to -1 and assigned only from a Content-Length header. A chunked request
    carries no Content-Length, so rxLen stayed -1 while the decoded body was written to the CGI's stdin,
    and websSetEnv then published CONTENT_LENGTH=-1. The canonical CGI idioms turn that negative length
    into malloc(0) followed by an out-of-bounds write, and read(0, buf, len) widens -1 to SIZE_MAX. The
    same -1 was published for every body-less request. Assert the gateway now sees the decoded byte count
    for chunked bodies and 0 when there is no body.

    The Ejscript Http client normalises framing headers and cannot emit a chunked request body, so these
    cases speak raw TCP via Socket.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, teq } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Send an exact byte sequence and read until the server closes. Every case sets "Connection: close" so
    the read loop terminates on EOF without relying on timing.
 */
async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while (await s.read(response, -1) != null) {}
    } catch {
        /* Read timeout: connection left open with no further data. Treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

/*
    cgitest echoes its environment as "<P>CONTENT_LENGTH=value</P>" when invoked with no switches.
    Return the echoed value, or null if the variable was never emitted.
 */
function echoedContentLength(raw: string): string | null {
    let m = /CONTENT_LENGTH=([^<]*)</.exec(raw)
    return m ? m[1] : null
}

if (thas('ME_GOAHEAD_CGI')) {

    /*
        Single chunk. The header block's terminating CRLF doubles as the leading CRLF of the first chunk
        header, matching the "\r\nSIZE\r\n" shape the chunk filter expects.
     */
    let raw = await exchange(
        'POST /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n' +
        'Content-Type: text/plain\r\nConnection: close\r\n\r\n9\r\nSome data\r\n0\r\n\r\n')
    teq(echoedContentLength(raw), '9', 'single chunk must publish the decoded byte count')

    /*
        Several chunks. The published length must be the cumulative decoded total, not the last chunk size.
     */
    raw = await exchange(
        'POST /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n' +
        'Content-Type: text/plain\r\nConnection: close\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n')
    teq(echoedContentLength(raw), '11', 'multiple chunks must publish the cumulative decoded total')

    /*
        A chunked request whose body is only the terminating chunk has a zero length body, not an unknown one.
     */
    raw = await exchange(
        'POST /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n' +
        'Content-Type: text/plain\r\nConnection: close\r\n\r\n0\r\n\r\n')
    teq(echoedContentLength(raw), '0', 'empty chunked body must publish 0')

    /*
        Body-less request. rxLen is never assigned at all, so this is the case folded in from the duplicate
        report: CONTENT_LENGTH was -1 for every GET.
     */
    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    teq(echoedContentLength(raw), '0', 'body-less request must publish 0, never -1')

    /*
        Content-Length framing is unchanged by the fix.
     */
    raw = await exchange(
        'POST /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nContent-Length: 9\r\n' +
        'Content-Type: text/plain\r\nConnection: close\r\n\r\nSome data')
    teq(echoedContentLength(raw), '9', 'Content-Length framing must still publish the declared length')

    /*
        Whatever the framing, the gateway must never see a negative length.
     */
    for (let payload of [
        'GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n',
        'HEAD /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n',
        'POST /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n' +
            'Connection: close\r\n\r\n0\r\n\r\n']) {
        ttrue(!(await exchange(payload)).includes('CONTENT_LENGTH=-'), 'CONTENT_LENGTH must never be negative')
    }

} else {
    tskip('CGI not enabled')
}

/*
    websSetFormVars gated form parsing on rxLen > 0, so a chunked urlencoded POST had its form variables
    silently dropped. /action/test echoes the "name" and "address" variables it resolves.
 */
let raw = await exchange(
    'POST /action/test HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n' +
    'Content-Type: application/x-www-form-urlencoded\r\nConnection: close\r\n\r\n' +
    '19\r\nname=Julie&address=Sydney\r\n0\r\n\r\n')
ttrue(raw.includes('name: Julie'), 'chunked form post must resolve the name variable')
ttrue(raw.includes('address: Sydney'), 'chunked form post must resolve the address variable')
