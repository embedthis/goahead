/*
    content-length-overflow.tst - Regression test for oversize Content-Length values.

    Content-Length was validated as all-digits and then converted with atoi(), whose int return
    truncated a large value to a small non-negative number. "4294967297" became 1, so the "< 0"
    guard and the post/PUT limits were both bypassed and the body was framed one byte long. On a
    keep-alive connection the undrained remainder was then parsed as a second, attacker-framed
    request (CWE-190 feeding CWE-444). Assert every out-of-range declaration is rejected and that
    exactly one response is emitted.

    The Ejscript Http client normalises framing headers, so these cases speak raw TCP via Socket.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, teq, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Send an exact byte sequence and read until the server closes or stops sending. Rejection closes
    the connection, so the read loop terminates on EOF.
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

function countResponses(raw: string): number {
    let matches = raw.match(/HTTP\/1\.[01] \d\d\d/g)
    return matches ? matches.length : 0
}

/*
    The smuggling primitive. atoi("4294967297") is 1 on a 64-bit int model, so one filler byte was
    consumed as the body and the trailing request line was serviced as a second request.
 */
let smuggle = 'POST /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: 4294967297\r\n\r\n' +
    'XGET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
let raw = await exchange(smuggle)
teq(countResponses(raw), 1, 'an oversize Content-Length must yield exactly one response')
ttrue(raw.includes(' 413 '), 'an oversize Content-Length must be rejected with 413')

/*
    Values that exceed the accumulator's own range as well as the body limit. Rejection happens on
    the limit, so digit count does not matter.
 */
for (let length of ['4294967296', '9223372036854775808', '18446744073709551616',
                    '000000000000000000004294967297']) {
    raw = await exchange('POST /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: ' + length +
        '\r\nConnection: close\r\n\r\n')
    ttrue(raw.includes(' 413 '), 'Content-Length ' + length + ' must be rejected with 413')
    teq(countResponses(raw), 1, 'Content-Length ' + length + ' must yield exactly one response')
}

/*
    A declaration inside the limit is unaffected: the body is framed at the declared length and a
    following pipelined request is serviced normally, not swallowed.
 */
raw = await exchange('POST /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nhello' +
    'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
teq(countResponses(raw), 2, 'an in-range Content-Length must frame the body and serve the next request')
ttrue(!raw.includes(' 413 '), 'an in-range Content-Length must not be rejected')
