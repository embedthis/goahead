/*
    request-line-strict.tst - The request-line is three parts separated by exactly one SP
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
    ['HTAB between the method and the target', 'GET\t/index.html HTTP/1.1\r\nHost: x\r\n\r\n'],
    ['a run of SP between the method and the target', 'GET  /index.html HTTP/1.1\r\nHost: x\r\n\r\n'],
    ['a run of SP between the target and the version', 'GET /index.html  HTTP/1.1\r\nHost: x\r\n\r\n'],
    ['HTAB between the target and the version', 'GET /index.html\tHTTP/1.1\r\nHost: x\r\n\r\n'],
    ['white space before the method', ' GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n'],
    ['a method outside the RFC 9110 token charset', 'GE"T /index.html HTTP/1.1\r\nHost: x\r\n\r\n'],
    ['an over-long method', 'G'.repeat(64) + ' /index.html HTTP/1.1\r\nHost: x\r\n\r\n'],
]

for (let [why, payload] of REJECT) {
    let raw = await exchange(payload)
    ttrue(status(raw).contains(' 400 '), `a request-line with ${why} must get 400, got [${status(raw)}]`)
}

const VERSIONS = [
    ['HTTP/2.0', ' 505 '],
    ['HTTP/3.0', ' 505 '],
    ['http/1.1', ' 400 '],
    ['HTTP/1', ' 400 '],
    ['HTTP/1.10', ' 400 '],
]
for (let [version, expect] of VERSIONS) {
    let raw = await exchange(`GET /index.html ${version}\r\nHost: x\r\n\r\n`)
    ttrue(status(raw).contains(expect), `version ${version} must get${expect}, got [${status(raw)}]`)
    ttrue(!raw.contains('Hello /index'), `version ${version} must not be served`)
}

let raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(raw.contains('200 OK'), `a well-formed request-line must be served, got [${status(raw)}]`)
ttrue(raw.contains('Hello /index'), 'the page body must be served')

raw = await exchange('HEAD /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(raw.contains('200 OK'), `HEAD must still be served, got [${status(raw)}]`)

raw = await exchange('GET /index.html HTTP/1.0\r\n\r\n')
ttrue(raw.contains('200 OK'), `an HTTP/1.0 request-line must still be served, got [${status(raw)}]`)
