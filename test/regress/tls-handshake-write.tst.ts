/*
    tls-handshake-write.tst - Timing out a TLS connection that never finished its handshake must not block
 */

import { Uri, Config } from '@embedthis/ejscript'
import { tget, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = new Uri(tget('TM_HTTP') || 'http://127.0.0.1:18080')
const HTTPS = new Uri(tget('TM_HTTPS') || 'https://127.0.0.1:14443')
const IDLE = parseInt(tget('TM_IDLE_TIMEOUT') || '60') * 1000
const SERVED_WITHIN = 3000

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('drives multi-second socket timing; too timing-sensitive for the Windows runner')
} else {

function sleep(ms: number): Promise<void> {
    return new Promise(r => setTimeout(r, ms))
}

function request(limit: number): Promise<number> {
    return new Promise(resolve => {
        let start = Date.now(), data = '', done = false
        let sock = connect(HTTP.port, HTTP.host)
        let finish = (ok: boolean) => {
            if (!done) { done = true; sock.destroy(); resolve(ok ? Date.now() - start : -1) }
        }
        sock.setTimeout(limit, () => finish(false))
        sock.on('connect', () => sock.write('GET /index.html HTTP/1.0\r\nHost: localhost\r\n\r\n'))
        sock.on('data', (d: any) => { data += d; if (data.indexOf('\r\n\r\n') >= 0) finish(true) })
        sock.on('error', () => finish(false))
    })
}

/* A TLS client that connects and never starts its handshake */
let silent = connect(HTTPS.port, HTTPS.host)
silent.on('error', () => {})
silent.on('data', () => {})

try {
    await new Promise(r => silent.once('connect', r))
    await sleep(IDLE + 3000)
    let took = await request(SERVED_WITHIN + 5000)
    ttrue(took >= 0 && took < SERVED_WITHIN,
        `closing a TLS connection mid-handshake must not stall the server (took ${took < 0 ? 'timeout' : took + ' ms'})`)
} finally {
    silent.destroy()
}

}
