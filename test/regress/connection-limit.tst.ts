/*
    connection-limit.tst - The concurrent-connection cap must hold, and must not leak descriptors.

    There was no connection limit anywhere in the accept path. The only ceiling was socketAlloc
    refusing at FD_SETSIZE, and on a single-threaded select server those descriptors ARE the serving
    capacity, so reaching that ceiling was the outage rather than a margin. Worse, socketAccept
    returned without closing the descriptor it had just accepted when socketAlloc refused, so past the
    ceiling the listening socket stayed readable and every wakeup accepted and abandoned one more.

    Two suites:
      A) the cap holds - connections are refused at the limit, the count is exact, and the count is
         returned when connections close
      B) the refusal path does not leak - cycling connections against a full table leaves the
         server's descriptor count stable and the server still serving

    What suite B does NOT cover, stated so nobody reads it as broader than it is: the descriptor leak
    in socketAccept, where socketAlloc refusing at FD_SETSIZE returned without closing the accepted
    socket. Once the connection cap is in place that path is unreachable, because websAccept refuses
    first and its refusal closes correctly. Reaching it needs the cap set above FD_SETSIZE and ~1024
    live sockets, which drives the server into the region where select() is undefined - the state the
    cap exists to prevent. That fix is defence in depth for a build whose cap is misconfigured above
    FD_SETSIZE, and it is verified by inspection rather than here. Reverting it does not fail this
    suite; do not treat a green run as covering it.

    This suite runs its own server on its own port with a low --connections value. The shared test
    server cannot be used: filling its connection table would starve every test that runs after it,
    and the shipped default is derived from FD_SETSIZE and needs ~1000 sockets to reach.

    Scope note: a total cap bounds resource consumption. It does NOT preserve availability for other
    clients - a flood still occupies the slots it is granted, and legitimate clients are refused
    alongside it. That is what the per-IP limit is for, and it is why this suite does not assert that
    a normal client is served during a flood. See connection-limit-perip.tst.ts for that guarantee.
 */

import { Config } from '@embedthis/ejscript'
import { teq, tget, tskip, ttrue } from '@embedthis/testme'
import { execSync } from 'node:child_process'
import { connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const LIMIT = 16
const PORT = 18081
const HOST = '127.0.0.1'
const TESTDIR = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const SERVER = join(TESTDIR, '..', 'build', 'bin', 'goahead-test')

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('spawns its own server and counts descriptors; not portable to the Windows runner')
} else {

/*
    Hold a raw connection open. Resolves once the peer has either accepted it (no data, no EOF) or
    refused it by closing. `refused` distinguishes the two: the server rejects by closing, so a
    connection that ends without ever being written to was refused.
 */
function hold(): Promise<any> {
    return new Promise((resolve, reject) => {
        let sock = connect(PORT, HOST)
        let settled = false
        sock.on('error', () => { if (!settled) { settled = true; resolve({sock, refused: true}) } })
        sock.on('close', () => { sock.refused = true })
        sock.on('connect', () => {
            /*
                A refusal arrives as a FIN immediately after the accept. Wait briefly for it rather
                than racing: without the delay an accepted and a refused connection look identical.
             */
            setTimeout(() => {
                if (!settled) { settled = true; resolve({sock, refused: sock.destroyed || sock.refused === true}) }
            }, 120)
        })
        setTimeout(() => { if (!settled) { settled = true; reject(new Error('connect timed out')) } }, 5000)
    })
}

/*
    Ask the server for its live connection count. Uses its own connection, which the server counts, so
    the reported figure includes this request.
 */
function count(): Promise<number> {
    return new Promise((resolve, reject) => {
        let sock = connect(PORT, HOST)
        let data = ''
        sock.on('data', d => data += d)
        sock.on('error', reject)
        sock.on('connect', () => {
            sock.write('GET /action/connections HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        })
        sock.on('close', () => {
            let m = data.match(/connections=(\d+)/)
            resolve(m ? parseInt(m[1]) : -1)
        })
        setTimeout(() => { sock.destroy(); reject(new Error('count timed out')) }, 5000)
    })
}

function fdCount(pid: number): number {
    try {
        if (Config.OS == 'linux') {
            return parseInt(execSync(`ls /proc/${pid}/fd | wc -l`).toString().trim())
        }
        return parseInt(execSync(`lsof -p ${pid} 2>/dev/null | wc -l`).toString().trim())
    } catch {
        return -1
    }
}

/*
    The client side needs a descriptor per held connection too, but LIMIT is small enough that any
    system running this suite has the headroom. A pre-flight `ulimit -n` check was tried and removed:
    it is not reliably parseable across the runtimes this suite runs under, and when it failed to
    parse the suite skipped itself and still reported PASS - a green result covering no assertions,
    which is worse than either outcome it was guarding against. A client-side exhaustion now surfaces
    as a connect failure with a message, which is loud and diagnosable.
 */
/*
    Refuse to run against a listener this suite did not start. A stale server left on this port by an
    aborted run answers every probe, and the results look plausible while describing a different
    build's configuration entirely - which is far more expensive to diagnose than a clear failure.
 */
await new Promise<void>((resolve) => {
    let probe = connect(PORT, HOST)
    probe.on('error', () => resolve())
    probe.on('connect', () => {
        probe.destroy()
        ttrue(false, `port ${PORT} is already in use; a stale server would answer this suite's probes`)
        resolve()
    })
    setTimeout(() => { probe.destroy(); resolve() }, 1000)
})

let server = startServer(SERVER, ['--connections', String(LIMIT), '--route', ROUTES, 'web', `${HOST}:${PORT}`],
    {cwd: TESTDIR, stdio: 'ignore'})
let held: any[] = []

try {
    /*
        Wait for the listener rather than sleeping a fixed interval.
     */
    let up = false
    for (let i = 0; i < 50; i++) {
        try { if ((await count()) >= 1) { up = true; break } } catch {}
        await new Promise(r => setTimeout(r, 100))
    }
    ttrue(up, 'test server started')

    /*
        A1 - the count is exact. Hold LIMIT-1 connections, then ask; the query uses the last slot, so
        the server must report exactly LIMIT.
     */
    for (let i = 0; i < LIMIT - 1; i++) {
        held.push((await hold()).sock)
    }
    teq(await count(), LIMIT, 'the live count must include every held connection and the query itself')

    /*
        A2 - at the cap, a further connection is refused rather than queued or hung. The refusal must
        arrive promptly: a pass produced by a stalled connect would be indistinguishable from a hang.
     */
    held.push((await hold()).sock)
    let started = Date.now()
    let refused = await hold()
    ttrue(refused.refused, 'a connection beyond the cap must be refused')
    ttrue(Date.now() - started < 2000, 'the refusal must be prompt, not a stall')
    refused.sock.destroy()

    /*
        A3/A4 - closing connections returns capacity. This is the assertion that catches a counter
        which increments but never decrements: without it the cap is a one-way latch and the server
        refuses everything for the rest of its life.
     */
    for (let s of held) { s.destroy() }
    held = []
    await new Promise(r => setTimeout(r, 300))
    teq(await count(), 1, 'closing every connection must return the count to just the query')

    for (let i = 0; i < LIMIT - 1; i++) {
        held.push((await hold()).sock)
    }
    teq(await count(), LIMIT, 'capacity released by closed connections must be reusable')
    for (let s of held) { s.destroy() }
    held = []
    await new Promise(r => setTimeout(r, 300))

    /*
        B - descriptor stability under sustained refusals. Fill to the cap, then cycle connections
        against a full table many times. Every one is refused through websAccept, so this pins that
        the refusal path releases its descriptor. See the header note for the socketAccept ceiling
        path, which this cannot reach.
     */
    for (let i = 0; i < LIMIT; i++) {
        held.push((await hold()).sock)
    }
    let before = fdCount(server.pid)
    /*
        Connect and drop without waiting to classify each one. Suite A already established that a
        connection at the cap is refused; here only the descriptor accounting matters, and the settle
        delay hold() uses would make this loop dominate the suite runtime.
     */
    for (let i = 0; i < LIMIT * 10; i++) {
        await new Promise(r => {
            let s = connect(PORT, HOST)
            s.on('error', () => r(null))
            s.on('close', () => r(null))
            s.on('connect', () => s.destroy())
        })
    }
    await new Promise(r => setTimeout(r, 300))
    let after = fdCount(server.pid)

    if (before > 0 && after > 0) {
        /*
            B2 - the direct assertion. A small margin absorbs unrelated churn (a log file rotating,
            a CGI staging file); the leak under test was one descriptor per refused connection, which
            is LIMIT * 10 and cannot hide inside it.
         */
        ttrue(after - before < 8,
            `the refusal path must not leak descriptors (before ${before}, after ${after})`)
    } else {
        tskip('cannot sample the server descriptor count on this platform')
    }

    /*
        B1 - the portable oracle. Release the flood and confirm the server still serves. A leak
        exhausts the descriptor table and this fails even where the count cannot be sampled.
     */
    for (let s of held) { s.destroy() }
    held = []
    await new Promise(r => setTimeout(r, 300))
    teq(await count(), 1, 'the server must still serve after sustained refusals at the cap')

} finally {
    /*
        Release every connection and stop the server whatever happened above. A suite that leaves its
        server running holds the port and fails whatever runs next, not itself.
     */
    for (let s of held) { try { s.destroy() } catch {} }
    await stopServer(server)
}

}
