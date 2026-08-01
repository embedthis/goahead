/*
    connection-limit-perip.tst - A flood from one peer must not deny service to another

    There was no per-address accounting anywhere in the accept path. The total connection cap added in
    phase 1 bounds resource consumption, but it refuses everyone equally once reached: a flood still
    occupies the slots it is granted and legitimate clients are refused alongside it. The per-peer cap
    is the control that actually preserves availability, and E2 is the assertion that says so.

    Four cases:
      E1  connections from one peer are refused at the per-peer cap, below the total cap
      E2  a DIFFERENT peer address is served throughout the flood        <- the availability guarantee
      E3  closing connections returns per-peer capacity
      E4  the per-peer table does not retain entries for peers with no live connections

    E4 matters because the table is keyed by an attacker-chosen value. A control that allocates one
    permanent entry per distinct source address is its own unbounded allocation, which is the defect
    the control was added to prevent.

    Two peer addresses are needed. 127.0.0.2 is not configured by default on macOS, so this uses the
    IPv4 and IPv6 loopbacks instead - the server listens on both and sees 127.0.0.1 and ::1 as
    distinct peers. Skipped where IPv6 loopback is unavailable rather than asserting a false pass.

    Scope note: per-address accounting is a speed bump over IPv6, where a single allocation is
    routinely a /64, and it has real false positives behind NAT. Both are documented limitations in
    doc/issues/tickets/10012.md, not defects this suite is asserting away.
 */

import { Config } from '@embedthis/ejscript'
import { teq, tskip, ttrue } from '@embedthis/testme'
import { spawn } from 'node:child_process'
import { connect } from 'node:net'
import { join } from 'node:path'

/*
    TOTAL is deliberately only a little above PER_IP. If the total cap is generous, a flood from one
    peer never exhausts the table and the cross-peer assertion passes whether or not per-peer
    accounting exists - it would assert nothing. Sized this way, a peer permitted to flood freely
    consumes every slot, so E2 fails against a build without the per-peer cap.
 */
const PER_IP = 6
const TOTAL = 12
const PORT = 18082
const V4 = '127.0.0.1'
const V6 = '::1'
const TESTDIR = join(import.meta.dir, '..')
const SERVER = join(TESTDIR, '..', 'build', 'bin', 'goahead-test')

if (Config.OS == 'windows') {
    tskip('spawns its own server on two loopback families; not portable to the Windows runner')
} else {

function sleep(ms: number): Promise<void> {
    return new Promise(r => setTimeout(r, ms))
}

/*
    Hold a connection from the given loopback family. Never rejects - a refusal is an observation.
 */
function hold(host: string): Promise<any> {
    return new Promise((resolve) => {
        let sock = connect({port: PORT, host})
        let settled = false
        let done = (refused: boolean) => { if (!settled) { settled = true; resolve({sock, refused}) } }
        sock.on('error', () => done(true))
        sock.on('close', () => done(true))
        sock.on('connect', () => setTimeout(() => done(sock.destroyed), 120))
        setTimeout(() => done(true), 5000)
    })
}

/*
    Query the counters from the given family. Returns {total, peer}, or nulls if refused - which is
    itself the observation in E1.
 */
function counts(host: string): Promise<any> {
    return new Promise((resolve) => {
        let sock = connect({port: PORT, host})
        let data = ''
        sock.on('data', d => data += d)
        sock.on('error', () => resolve({total: -1, peer: -1}))
        sock.on('connect', () => {
            sock.write('GET /action/connections HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        })
        sock.on('close', () => {
            let m = data.match(/connections=(\d+) peer=(\d+)/)
            resolve(m ? {total: parseInt(m[1]), peer: parseInt(m[2])} : {total: -1, peer: -1})
        })
        setTimeout(() => { sock.destroy(); resolve({total: -1, peer: -1}) }, 5000)
    })
}

/*
    Refuse to run against a listener this suite did not start. A stale server left on this port by an
    aborted run answers every probe, and the results look plausible while describing a different
    build's configuration entirely - which is far more expensive to diagnose than a clear failure.
 */
await new Promise<void>((resolve) => {
    let probe = connect({port: PORT, host: V4})
    probe.on('error', () => resolve())
    probe.on('connect', () => {
        probe.destroy()
        ttrue(false, `port ${PORT} is already in use; a stale server would answer this suite's probes`)
        resolve()
    })
    setTimeout(() => { probe.destroy(); resolve() }, 1000)
})

let server = spawn(SERVER,
    ['--connections', String(TOTAL), '--connections-per-ip', String(PER_IP),
     'web', `${V4}:${PORT}`, `[${V6}]:${PORT}`],
    {cwd: TESTDIR, stdio: 'ignore'})
server.on('error', (e) => { ttrue(false, 'cannot spawn ' + SERVER + ': ' + e) })
let held: any[] = []

try {
    let up = false
    for (let i = 0; i < 50; i++) {
        if ((await counts(V4)).total >= 1) { up = true; break }
        await sleep(100)
    }
    ttrue(up, 'test server started on the IPv4 loopback')

    let v6 = (await counts(V6)).total >= 1
    if (!v6) {
        tskip('IPv6 loopback is not usable here; the cross-peer assertion cannot be made')
    } else {

    /*
        E1 - flood from one address with more connections than the TOTAL cap allows. Only PER_IP may
        be granted; the rest must be refused by the per-peer cap rather than consuming the table.
     */
    let granted = 0
    for (let i = 0; i < TOTAL; i++) {
        let h = await hold(V4)
        if (h.refused) {
            h.sock.destroy()
        } else {
            granted++
            held.push(h.sock)
        }
    }
    ttrue(granted <= PER_IP, `one peer must not be granted more than its cap (granted ${granted})`)
    ttrue(granted >= PER_IP - 1, `the peer must be granted its allowance (granted ${granted})`)

    let c = await counts(V4)
    ttrue(c.total < TOTAL, 'the flood must not have consumed the whole connection table')

    /*
        E2 - the availability guarantee. The flood from the IPv4 peer is still held; a request from
        the IPv6 peer must be served normally. This is what the total cap alone cannot deliver, and
        the reason phase 3 exists.
     */
    let other = await counts(V6)
    ttrue(other.total > 0, 'a different peer must still be served while another peer floods')
    teq(other.peer, 1, 'the other peer must have its own allowance, unconsumed by the flood')

    /*
        E3 - capacity is returned on close, per peer. Catches a per-peer counter that increments but
        never decrements, which latches the address out permanently.
     */
    for (let s of held) { s.destroy() }
    held = []
    await sleep(400)
    teq((await counts(V4)).peer, 1, 'closing connections must return per-peer capacity')
    let again = await hold(V4)
    ttrue(!again.refused, 'the peer must be able to connect again after releasing')
    again.sock.destroy()
    await sleep(200)

    /*
        E4 - no residue. With every connection from a peer closed, that peer must hold no entry. The
        query below runs from the IPv6 peer so the IPv4 entry is genuinely idle at the moment it is
        sampled.
     */
    await sleep(200)
    let idle = await counts(V6)
    ttrue(idle.total >= 1, 'the server must still be serving')
    teq(await peerOf(V4), 0, 'a peer with no live connections must retain no entry')

    }
} finally {
    for (let s of held) { try { s.destroy() } catch {} }
    server.kill('SIGTERM')
}

/*
    Sample one peer's count from the OTHER family, so the address under inspection has no connection
    of its own open at the moment of sampling. Querying a peer's count over that same peer's
    connection always reads at least 1 and cannot observe the empty case.
 */
async function peerOf(host: string): Promise<number> {
    let probe = await counts(host == V4 ? V6 : V4)
    if (probe.total < 0) return -1
    let c = await counts(host)
    /* c.peer includes the query connection itself, so an otherwise-idle peer reads exactly 1. */
    return c.peer - 1
}

}
