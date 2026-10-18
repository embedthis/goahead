/*
    host-required.tst - An HTTP/1.1 request must carry exactly one non-empty Host (RFC 9112 3.2)
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { ttrue, tget } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let end = raw.indexOf('\r\n')
    return end < 0 ? raw : raw.slice(0, end)
}

const REJECT = [
    ['no Host', 'GET /index.html HTTP/1.1\r\nConnection: close\r\n\r\n'],
    ['an empty Host', 'GET /index.html HTTP/1.1\r\nHost:\r\nConnection: close\r\n\r\n'],
    ['two Host fields', 'GET /index.html HTTP/1.1\r\nHost: x\r\nHost: y\r\nConnection: close\r\n\r\n'],
    ['an absolute-form target and no Host', `GET http://${ADDR}/index.html HTTP/1.1\r\nConnection: close\r\n\r\n`],
]
for (let [why, payload] of REJECT) {
    let raw = await exchange(payload)
    ttrue(status(raw).contains(' 400 '), `HTTP/1.1 with ${why} must get 400, got [${status(raw)}]`)
    ttrue(!raw.contains('Hello /index'), `HTTP/1.1 with ${why} must not be served`)
}

let raw = await exchange('GET /index.html HTTP/1.0\r\n\r\n')
ttrue(status(raw).contains(' 200 '), `HTTP/1.0 without Host must still be served, got [${status(raw)}]`)

raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(status(raw).contains(' 200 '), `HTTP/1.1 with a Host must be served, got [${status(raw)}]`)
ttrue(raw.contains('Hello /index'), 'the page body must be served')
