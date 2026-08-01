/*
    header-prologue-cap.tst - Leading CRLF stripping must be bounded

    parseIncoming() stripped leading empty lines with an unbounded loop that CONSUMED the bytes via
    bufGetc. Because the bytes were consumed, they never survived to be measured by the
    ME_GOAHEAD_LIMIT_HEADER test a few lines below, which reads bufLen(). A client could therefore
    send CRLF pairs forever, at any rate it liked, and hold a socket, an fd, a Webs slot and an
    rxbuf allocation open without ever sending a request line the server could log or time out.
    40,000 bytes of prologue were accepted with no 413.

    RFC 9112 section 2.2 lets a server ignore at least one empty line before the request line, as a
    robustness allowance for clients that emit a trailing CRLF after a body. It does not license an
    unbounded prologue, so the fix caps the strip at MAX_PROLOGUE bytes and rejects with 400.

    Two suites:
      A) The allowance still works - a request preceded by a small prologue is served normally.
      B) The cap is enforced - a prologue past the cap is rejected with 400 and the connection is
         closed, rather than absorbed silently.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { teq, ttrue } from '@embedthis/testme'
import { tget } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const httpUri = new Uri(HTTP)

const REQUEST = 'GET /index.html HTTP/1.0\r\nHost: localhost\r\n\r\n'

/*
    Speak raw TCP: an HTTP client library would normalise away a prologue before it reached the
    wire, which is the whole of what is under test here.
 */
async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: server left the connection open with no further data. End of exchange. */
    }
    s.close()
    return response.toString()
}

function status(raw: string): number {
    let m = raw.match(/^HTTP\/1\.[01] (\d\d\d)/)
    return m ? parseInt(m[1]) : 0
}

/*
    Suite A - the RFC allowance survives. One CRLF is the case the allowance exists for: a client
    that emitted a trailing CRLF after a previous POST body. Four blank lines is the documented cap
    (MAX_PROLOGUE is 8 bytes), so it must still be served rather than sitting one off the boundary.
 */
for (let pairs of [1, 2, 4]) {
    let raw = await exchange('\r\n'.repeat(pairs) + REQUEST)
    teq(status(raw), 200, `a ${pairs}-line prologue must still be served`)
}

/*
    A lone LF prologue - some clients emit bare LF. Two bytes, well inside the cap.
 */
let raw = await exchange('\n\n' + REQUEST)
teq(status(raw), 200, 'a bare-LF prologue within the cap must still be served')

/*
    Suite B - the cap is enforced. The pre-fix behaviour was that every one of these was absorbed
    and the request that followed was served with 200, so a 200 here is the regression signal.
 */
for (let pairs of [5, 64, 20000]) {
    raw = await exchange('\r\n'.repeat(pairs) + REQUEST)
    teq(status(raw), 400, `a ${pairs}-line prologue must be rejected with 400`)
}

/*
    The connection must be closed, not left available for the attacker to keep feeding. Exactly one
    response, and the trailing request must not have been serviced after the rejection.
 */
raw = await exchange('\r\n'.repeat(1000) + REQUEST)
let responses = raw.match(/HTTP\/1\.[01] \d\d\d/g)
teq(responses ? responses.length : 0, 1, 'a rejected prologue must yield exactly one response')
teq(status(raw), 400, 'the one response to a rejected prologue must be the rejection')
ttrue(!raw.includes('Hello /index.html'), 'the request after a rejected prologue must not be served')

/*
    The prologue must not be chargeable in instalments. Sent one pair at a time across separate
    writes, the pre-fix loop reset its effective budget on every read because the bytes were gone
    from the buffer by the next call. The cap is per request-parse, so a dribbled prologue must be
    rejected on the same byte count as a single-write one.
 */
let s = new Socket
s.connect(httpUri.host + ':' + httpUri.port)
/*
    Pause between instalments so each pair lands as its own TCP segment and therefore its own read
    event. Without the pause the writes coalesce and the server sees one buffer, which is the case
    already covered above.
 */
let sent = 0
try {
    for (let i = 0; i < 20; i++) {
        await s.write('\r\n')
        sent++
        await new Promise(resolve => setTimeout(resolve, 20))
    }
    await s.write(REQUEST)
} catch {
    /*
        Expected: the server rejects and closes partway through, so a later write hits a closed peer.
        Reaching all 20 instalments means the budget was never charged across reads.
     */
}
let drip = new ByteArray(65536, true)
try {
    while ((await s.read(drip, -1)) != null) {}
} catch {
    /* Read timeout: treat as end of exchange. */
}
s.close()
teq(status(drip.toString()), 400, 'a dribbled prologue must be rejected on the same budget as a single-write one')
ttrue(sent < 20, 'the server must close mid-drip rather than accept the whole dribbled prologue')
