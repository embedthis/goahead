/*
    parse-deadline.tst - A request that has started arriving must complete within PARSE_TIMEOUT

    checkTimeout only ever tested the 60 second idle mark, and readEvent refreshed that mark on every
    read before parsing anything. A client dribbling two bytes a minute therefore renewed its slot
    forever without ever sending a request the server could log or time out. PARSE_TIMEOUT existed but
    was only the first callback delay - the handler concluded "not yet" and rescheduled 60 seconds
    out, so it never enforced anything.

    The fix holds the request start separately from the resettable idle mark. The subtlety is where
    the clock starts, and the obvious answer is wrong: stamping at accept, or on keep-alive reuse,
    applies the deadline to a persistent connection waiting for its next request and closes it seconds
    after its previous response - which disables HTTP/1.1 keep-alive without failing any test that
    makes a single request. The clock therefore starts on the first byte of a request.

    That division is what this suite pins, in both directions:

      D1  a dribbled header block is closed at PARSE_TIMEOUT, not at the idle timeout
      D2  a dribbled BODY still completes - the deadline covers headers only
      D3  an idle keep-alive connection is NOT closed at PARSE_TIMEOUT       <- guards the design error
      D4  a second request on a keep-alive connection gets a fresh deadline
      D5  a connection that has sent nothing is governed by the idle timeout, not PARSE_TIMEOUT
      D6  an ordinary single-write request is unaffected

    D3 and D5 together pin that the clock starts on the first byte rather than on the connection.

    Timings assume the shipped ME_GOAHEAD_LIMIT_PARSE_TIMEOUT of 5 s and LIMIT_TIMEOUT of 60 s. The
    assertions use wide margins between the two so they distinguish the deadlines rather than
    measuring them.
 */

import { Uri, Config } from '@embedthis/ejscript'
import { teq, tskip, ttrue } from '@embedthis/testme'
import { tget } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const uri = new Uri(HTTP)
const HOST = uri.host
const PORT = uri.port

/* Comfortably past the 5 s parse deadline, comfortably short of the 60 s idle timeout. */
const PAST_PARSE = 8000

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('drives multi-second socket timing; too slow and too timing-sensitive for the Windows runner')
} else {

function sleep(ms: number): Promise<void> {
    return new Promise(r => setTimeout(r, ms))
}

/*
    Open a connection and return a handle that records what the server did and when. Never rejects: a
    close by the server is the observation under test, not an error.
 */
function open(): any {
    let h: any = {data: '', closed: false, closedAt: 0, opened: Date.now()}
    h.sock = connect(PORT, HOST)
    h.sock.on('data', (d: any) => h.data += d)
    h.sock.on('error', () => { if (!h.closed) { h.closed = true; h.closedAt = Date.now() } })
    h.sock.on('close', () => { if (!h.closed) { h.closed = true; h.closedAt = Date.now() } })
    h.send = (s: string) => { try { h.sock.write(s) } catch {} }
    h.status = () => { let m = h.data.match(/HTTP\/1\.[01] (\d\d\d)/); return m ? parseInt(m[1]) : 0 }
    h.done = () => { try { h.sock.destroy() } catch {} }
    return h
}

async function ready(h: any): Promise<void> {
    for (let i = 0; i < 50; i++) {
        if (!h.sock.connecting) return
        await sleep(20)
    }
}

let handles: any[] = []
function track(h: any): any { handles.push(h); return h }

try {
    /*
        D1 - a dribbled header block is closed on the parse deadline. Send a request line and then
        one header byte per second, never completing the block. The idle mark is refreshed by every
        one of those bytes, so before the fix this ran until the client gave up.
     */
    {
        let h = track(open())
        await ready(h)
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\n')
        let start = Date.now()
        for (let i = 0; i < 12 && !h.closed; i++) {
            h.send('X')
            await sleep(700)
        }
        ttrue(h.closed, 'a dribbled header block must be closed, not held open indefinitely')
        let took = h.closedAt - start
        ttrue(took < 20000, `the close must come from the parse deadline, not the idle timeout (took ${took}ms)`)
        h.done()
    }

    /*
        D2 - the deadline must not cover the body. Send complete headers, then dribble a declared body
        for longer than PARSE_TIMEOUT. A naive implementation that tests every pre-response state
        closes this, which would break every slow upload.
     */
    {
        let body = 'name=value-that-arrives-slowly'
        let h = track(open())
        await ready(h)
        h.send('POST /action/showTest HTTP/1.1\r\nHost: localhost\r\n' +
               'Content-Type: application/x-www-form-urlencoded\r\n' +
               'Content-Length: ' + body.length + '\r\nConnection: close\r\n\r\n')
        for (let i = 0; i < body.length; i++) {
            await sleep(300)
            h.send(body[i])
        }
        for (let i = 0; i < 40 && h.status() == 0; i++) {
            await sleep(100)
        }
        teq(h.status(), 200, 'a slowly dribbled body must still complete - the deadline covers headers only')
        h.done()
    }

    /*
        D3 - the guard for the design error. Complete one request, hold the connection idle for longer
        than PARSE_TIMEOUT, then use it again. Stamping the clock at accept or on keep-alive reuse
        closes this connection while it waits, silently disabling persistent connections.
     */
    {
        let h = track(open())
        await ready(h)
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n')
        for (let i = 0; i < 40 && h.status() == 0; i++) {
            await sleep(50)
        }
        teq(h.status(), 200, 'the first keep-alive request must succeed')

        await sleep(PAST_PARSE)
        ttrue(!h.closed, 'an idle keep-alive connection must not be closed by the parse deadline')

        h.data = ''
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        for (let i = 0; i < 40 && h.status() == 0; i++) {
            await sleep(50)
        }
        teq(h.status(), 200, 'a request after a long idle period must still be served')
        h.done()
    }

    /*
        D4 - the deadline re-arms per request. The second request on a reused connection must get its
        own budget: neither inherited-and-already-expired, nor absent.
     */
    {
        let h = track(open())
        await ready(h)
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n')
        for (let i = 0; i < 40 && h.status() == 0; i++) {
            await sleep(50)
        }
        teq(h.status(), 200, 'the first request on the connection must succeed')

        h.data = ''
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\n')
        let start = Date.now()
        for (let i = 0; i < 12 && !h.closed; i++) {
            h.send('X')
            await sleep(700)
        }
        ttrue(h.closed, 'a dribbled second request must also be closed by the parse deadline')
        ttrue(h.closedAt - start < 20000, 'the second request must get a fresh, enforced deadline')
        h.done()
    }

    /*
        D5 - a connection that has sent nothing is not a half-sent request. It must be governed by the
        idle timeout, which the connection cap bounds the count of, rather than closed at the much
        shorter parse deadline. Pairs with D3 to pin that the clock starts on the first byte.
     */
    {
        let h = track(open())
        await ready(h)
        await sleep(PAST_PARSE)
        ttrue(!h.closed, 'a silent connection must not be closed by the parse deadline')
        h.data = ''
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        for (let i = 0; i < 40 && h.status() == 0; i++) {
            await sleep(50)
        }
        teq(h.status(), 200, 'a connection that waited before speaking must still be served')
        h.done()
    }

    /*
        D6 - the ordinary path is untouched. A request written in one go must be served normally, with
        no interaction with either deadline.
     */
    {
        let h = track(open())
        await ready(h)
        h.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        for (let i = 0; i < 40 && h.status() == 0; i++) {
            await sleep(50)
        }
        teq(h.status(), 200, 'an ordinary request must be unaffected by the parse deadline')
        h.done()
    }

} finally {
    for (let h of handles) { h.done() }
}

}
