/* A peer that pipelines requests and never reads must delay only itself */

import { Uri, Config } from '@embedthis/ejscript'
import { teq, tget, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = new Uri(tget('TM_HTTP') || 'http://127.0.0.1:18080')
const PIPELINED = 20000
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

/* Part 1 */
{
    let one = 'GET /pipelined-missing-page HTTP/1.1\r\nHost: localhost\r\n\r\n'
    let sock = connect(HTTP.port, HTTP.host)
    sock.on('error', () => {})
    try {
        await new Promise(r => sock.once('connect', r))
        //  Never read: pausing stops the client draining its receive buffer
        sock.pause()
        sock.write(one.repeat(PIPELINED))
        await sleep(2000)
        let took = await request(SERVED_WITHIN + 5000)
        ttrue(took >= 0 && took < SERVED_WITHIN,
            `a peer that does not read its pipelined responses must not stall others (took ${took < 0 ? 'timeout' : took + ' ms'})`)
    } finally {
        sock.destroy()
    }
}

/* Part 2 */
{
    let paths = []
    for (let i = 0; i < 20; i++) {
        paths.push(i % 2 ? '/index.html' : '/pipelined-missing-page')
    }
    let batch = paths.map((p, i) => `GET ${p} HTTP/1.1\r\nHost: localhost\r\n` +
        (i == paths.length - 1 ? 'Connection: close\r\n' : '') + '\r\n').join('')
    let data = await new Promise<string>(resolve => {
        let text = ''
        let sock = connect(HTTP.port, HTTP.host)
        sock.setTimeout(10000, () => { sock.destroy(); resolve(text) })
        sock.on('connect', () => sock.write(batch))
        sock.on('data', (d: any) => text += d)
        sock.on('close', () => resolve(text))
        sock.on('error', () => resolve(text))
    })
    let codes = (data.match(/HTTP\/1\.1 \d\d\d/g) || []).map((s: string) => parseInt(s.slice(9)))
    teq(codes.length, paths.length, 'every pipelined request gets a response')
    teq(codes.join(','), paths.map(p => p == '/index.html' ? 200 : 404).join(','), 'responses arrive in request order')
}

}
