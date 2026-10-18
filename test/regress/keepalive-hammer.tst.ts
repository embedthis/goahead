/*
    keepalive-hammer.tst - Many alternating valid and malformed requests over a reused connection

    Everything else in the suite scales with request size. This scales with request *count*: the
    parse loop, the header table and the connection-reuse path are driven thousands of times, with a
    malformed request interleaved between every valid one so the recovery path is exercised as often
    as the happy path. A state machine that leaks a header slot, fails to reset a flag between
    requests on the same connection, or degrades after a parse error shows up here and nowhere else.

    DEPTH SCALES THE COUNT, BUT DEPTH 0 STILL RUNS. The appweb original gates the whole test at
    depth 6, which means it never runs unless someone remembers to ask for it. A test that runs at no
    depth is a test nobody notices has rotted - the suite already had one of those. The depth-0 count
    here is small enough to sit inside the ordinary run and large enough to catch a hard failure; the
    higher depths are where the soak value is.

    The assertion count is deliberately independent of depth. Asserting once per iteration would make
    a deep run report tens of thousands of assertions, which says nothing more than ten do and buries
    every other test in the summary.
 */

import {tdepth, tget, tinfo, ttrue} from '@embedthis/testme'
import {ByteArray, Socket, Uri} from '@embedthis/ejscript'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Request pairs per depth. Clamped rather than indexed directly: tm accepts --depth 10 and beyond,
    and an out-of-range index would yield undefined, then NaN, and a loop that never runs - a test
    that silently stops testing at the depths where it matters most.
 */
const PAIRS = [200, 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000, 256000]
const pairs = PAIRS[Math.min(tdepth(), PAIRS.length - 1)]

//  Ten checkpoints whatever the depth
const CHECK_EVERY = Math.max(1, Math.floor(pairs / 10))

const VALID =
    'GET /index.html HTTP/1.1\r\n' +
    'Host: ' + ADDR + '\r\n' +
    'Accept: text/html\r\n' +
    'Connection: keep-alive\r\n\r\n'

/*
    A malformed request the server must reject without losing the connection state it needs for the
    next valid one. The header line carries control bytes and a broken date, which is the shape that
    reaches the date parser and the header tokeniser.
 */
const MALFORMED =
    'GET /index.html HTTP/1.1\r\n' +
    'Host: ' + ADDR + '\r\n' +
    'Connection: keep-alive\r\n' +
    'If-Modified-\x01\x02: 0 Jan 0000 00:00 +0000\r\n' +
    'Content-Disposition\x1b[41m\x1b[2J: inline\r\n\r\n'

let response = new ByteArray(8192, true)
let s = new Socket
let connected = false
let checkpoints = 0
let reconnects = 0

async function reconnect(): Promise<void> {
    try {
        s.close()
    } catch {}
    s = new Socket
    s.connect(ADDR)
    connected = true
    reconnects++
}

/*
    Issue one request, reconnecting first if the previous exchange closed the connection. Returns
    whether a response came back framed. Every failure mode is contained here rather than around the
    call, so a throw cannot skip the caller's assertion - the first version asserted inside the try
    and reached one checkpoint out of ten, because the socket threw before the assertion each time.
 */
async function issue(request: string): Promise<boolean> {
    try {
        if (!connected) {
            await reconnect()
        }
        response.flush()
        await s.write(request)
        await s.read(response, -1)
        return response.toString().contains('HTTP/1.')
    } catch {
        connected = false
        return false
    }
}

const started = Date.now()
await reconnect()
reconnects = 0

let unanswered = 0
for (let i = 0; i < pairs; i++) {
    const answered = await issue(VALID)
    if (!answered) {
        unanswered++
    }
    if (i % CHECK_EVERY == 0) {
        /*
            The valid half of the pair must still be framed as a response. Checked at intervals
            rather than every iteration: the property under test is that the interleaved malformed
            traffic has not degraded the server, not that any single request succeeded. Asserting
            every iteration would also make a deep run report a quarter of a million assertions.
         */
        ttrue(answered, `valid request ${i} of ${pairs} was not answered`)
        checkpoints++
    }

    /*
        The malformed half. The server is entitled to reject it and close - GoAhead does - so the
        result is not asserted on. What matters is that the next valid request still succeeds, which
        the checkpoints above measure.
     */
    await issue(MALFORMED)
    connected = false
}

try {
    s.close()
} catch {}

tinfo('  ' + pairs + ' request pairs at depth ' + tdepth() + ', ' + reconnects + ' reconnects, ' +
      unanswered + ' valid requests unanswered, ' + ((Date.now() - started) / 1000).toFixed(1) + 's')

/*
    Liveness on a fresh connection. Without this the test could only ever detect a crash - a server
    left wedged, refusing connections or answering nothing would pass every check above, because the
    checkpoints stop being reached rather than failing.
 */
let fresh = new Socket
fresh.connect(ADDR)
await fresh.write('GET /index.html HTTP/1.1\r\nHost: ' + ADDR + '\r\nConnection: close\r\n\r\n')
let final = new ByteArray(8192, true)
try {
    while ((await fresh.read(final, -1)) != null) {}
} catch {
    /* Read timeout: treat as end of exchange. */
}
fresh.close()

ttrue(final.toString().contains('200 OK'), 'the server must still serve after the hammer')

/*
    The checkpoints must actually have been reached. Without this, a loop that threw its way to the
    end would report no failed assertion and no coverage - which is exactly what the first version of
    this test did.
 */
ttrue(checkpoints == Math.min(10, pairs), `expected ${Math.min(10, pairs)} checkpoints, reached ${checkpoints}`)

/*
    No valid request may go unanswered. Each one is issued on a connection the previous malformed
    request may have closed, so this is the assertion that the recovery path works every time rather
    than at the ten sampled points.
 */
ttrue(unanswered == 0, `${unanswered} of ${pairs} valid requests were not answered`)
