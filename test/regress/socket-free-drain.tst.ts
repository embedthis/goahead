/* Closing a connection must not drain its input without bound */

import { Uri, Config } from '@embedthis/ejscript'
import { tget, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const uri = new Uri(HTTP)
const HOST = uri.host
const PORT = uri.port

/* ME_GOAHEAD_LIMIT_LINGER is 2 s. The second request is sent well after it expires */
const SEND_FOR = 8000
const PROBE_AT = 4500
const SERVED_WITHIN = 2000

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('drives multi-second socket timing; too timing-sensitive for the Windows runner')
} else {

function sleep(ms: number): Promise<void> {
    return new Promise(r => setTimeout(r, ms))
}

/*
    Keep sending until the deadline or close; sender failure is not the assertion under test.
 */
function sender(until: number): any {
    /* Half-open: the server shuts down its side first and the sender must keep writing */
    let h: any = {sock: connect({port: PORT, host: HOST, allowHalfOpen: true}), done: false}
    let block = Buffer.alloc(256 * 1024, 'Z')
    h.sock.on('error', () => { h.done = true })
    h.sock.on('close', () => { h.done = true })
    h.sock.on('data', () => {})
    h.sock.on('connect', () => {

        h.sock.write('GET /index.html HTTP/1.1\r\nHost: localhost\r\nX-Fill: ' + 'a'.repeat(4000))
        let pump = () => {
            while (!h.done && Date.now() < until) {
                if (!h.sock.write(block)) {
                    h.sock.once('drain', pump)
                    return
                }
            }
            h.sock.destroy()
        }
        pump()
    })
    return h
}

function request(): Promise<number> {
    return new Promise(resolve => {
        let start = Date.now(), data = ''
        let sock = connect(PORT, HOST)
        let finish = () => { sock.destroy(); resolve(Date.now() - start) }
        sock.setTimeout(SEND_FOR + 5000, finish)
        sock.on('connect', () => sock.write('GET /index.html HTTP/1.0\r\nHost: localhost\r\n\r\n'))
        sock.on('data', (d: any) => { data += d; if (data.indexOf('\r\n\r\n') >= 0) finish() })
        sock.on('error', finish)
    })
}

let h = sender(Date.now() + SEND_FOR)
try {
    await sleep(PROBE_AT)
    let took = await request()
    ttrue(took < SERVED_WITHIN,
        `a peer that keeps sending while being closed must not stall other clients (served in ${took} ms)`)
} finally {
    h.done = true
    h.sock.destroy()
}

}
