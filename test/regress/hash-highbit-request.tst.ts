/*
    hash-highbit-request.tst - A high-bit byte in a hashed request key must not break the server

    hashIndex() promoted each key byte through a signed char before shifting it left, so a byte in
    0x80-0xFF sign-extended to a negative int and the shift was undefined. Two request paths put a
    client-supplied high-bit byte into a hash key, and both are unauthenticated:

      A. A query parameter NAME. websValidUriChars() screens the still-encoded URI, where "%e2" is
         three ordinary ASCII characters, and addFormVars() only decodes afterwards. The '=' is
         required - the decode sits inside the strchr(keyword, '=') branch, so "?%e2" alone stays
         encoded and never reaches the defect. That is why the long-standing UTF-8 ISINDEX case in
         basic/cgi.tst.ts never tripped it.

      B. A multipart field NAME. It is body data, so the header token validator never sees it.

    On a build that traps on undefined behavior these aborted the process outright; GoAhead is single
    threaded, so one request took every connection with it. On a stock build there is no crash, which
    is what these assertions therefore check: each request is answered normally and the server is
    still serving afterwards. A regression that reintroduced the signed shift would be caught here
    only under a sanitizer, so regress/hash-highbit-key.tst.c carries the direct proof.

    ST-4 is the negative control. Header names and values ARE screened for printability, which is why
    the header and session-cookie paths are not reachable. If a future change relaxed that screen it
    would open a third path into the same function, so the rejection is pinned here.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Send an exact payload and return the response text. Every request here uses HTTP/1.0 so the
    server closes the connection and the read loop ends on EOF rather than on a timeout.
 */
async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Connection left open with no further data - treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

async function get(uri: string): Promise<string> {
    return await exchange('GET ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
}

/*
    Path A: a single percent-encoded high-bit byte as a query parameter name. This is the reported
    proof of concept.
 */
let response = await get('/index.html?%e2=1')
ttrue(status(response) == 200, 'a high-bit query parameter name must be served normally')

/*
    The server must still be alive. On a trapping build the request above killed the process, so this
    is the assertion that distinguished the defect from a merely odd response.
 */
response = await get('/index.html')
ttrue(status(response) == 200, 'the server must survive a high-bit query parameter name')

/*
    Every high-bit byte, not only the reported 0xE2. The shift amount rotates with position, so the
    parameter name is also varied in length to reach each amount in the cycle.
 */
for (let b = 0x80; b <= 0xff; b++) {
    let hex = b.toString(16).padStart(2, '0')
    response = await get('/index.html?%' + hex + '=1')
    ttrue(status(response) == 200, 'query name %' + hex + ' must be served normally')
}
for (let pad of ['', 'a', 'ab', 'abc', 'abcd']) {
    response = await get('/index.html?' + pad + '%e2=1')
    ttrue(status(response) == 200, 'high-bit byte at offset ' + pad.length + ' must be served normally')
}

/*
    A high-bit byte in the VALUE rather than the name is decoded too, but only the name is hashed.
    Asserted so the pair documents which half actually reaches hashIndex.
 */
response = await get('/index.html?k=%e2')
ttrue(status(response) == 200, 'a high-bit query value must be served normally')

/*
    The no-'=' form stays percent-encoded and never reaches the hash as a high-bit key. It must still
    be served, and it is the case that made this defect survive years of UTF-8 query tests.
 */
response = await get('/index.html?%e2')
ttrue(status(response) == 200, 'a high-bit ISINDEX query must be served normally')

response = await get('/index.html')
ttrue(status(response) == 200, 'the server must survive the full high-bit query sweep')

/*
    Path B: a non-ASCII multipart field name, sent as raw body bytes with no percent-encoding. The
    body is UTF-8, so the field name carries high-bit bytes exactly as a browser would send them.
    Content-Length must count bytes, not UTF-16 code units.
 */
if (thas('ME_GOAHEAD_UPLOAD')) {
    let body = '--BOUNDARY\r\n' +
               'Content-Disposition: form-data; name="â"\r\n' +
               '\r\n' +
               'somevalue\r\n' +
               '--BOUNDARY--\r\n'
    let length = Buffer.byteLength(body, 'utf8')
    response = await exchange('POST /action/uploadTest HTTP/1.0\r\n' +
                              'Host: localhost\r\n' +
                              'Content-Type: multipart/form-data; boundary=BOUNDARY\r\n' +
                              'Content-Length: ' + length + '\r\n' +
                              '\r\n' + body)
    ttrue(status(response) != 0, 'a non-ASCII multipart field name must produce a response')

    response = await get('/index.html')
    ttrue(status(response) == 200, 'the server must survive a non-ASCII multipart field name')
}

/*
    ST-4, negative control: the header path stays closed. A non-printable byte in a header name or a
    header value is rejected before it can become a hash key, which is why session ids arriving in a
    Cookie value cannot reach the defect either.
 */
response = await exchange('GET /index.html HTTP/1.0\r\nHost: localhost\r\nX-â: v\r\n\r\n')
ttrue(status(response) == 400, 'a high-bit byte in a header name must be rejected')

response = await exchange('GET /index.html HTTP/1.0\r\nHost: localhost\r\nX-Test: â\r\n\r\n')
ttrue(status(response) == 400, 'a high-bit byte in a header value must be rejected')

response = await get('/index.html')
ttrue(status(response) == 200, 'the server must survive rejected high-bit headers')
