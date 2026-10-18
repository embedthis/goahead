/*
    undrained-body-framing.tst - The Connection header agrees with what happens to the connection
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

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

function firstHeaders(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return (end < 0 ? raw : raw.slice(0, end)).toLowerCase()
}

function responseCount(raw: string): number {
    return raw.split('HTTP/1.1 ').length - 1
}

const SECOND = 'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'

let raw: string
if (thas('ME_GOAHEAD_AUTH')) {
    /*
        Leave the body incomplete so authentication rejects it before draining.
     */
    raw = await exchange('POST /auth/basic/ HTTP/1.1\r\nHost: x\r\nContent-Length: 50\r\n\r\nshort' + SECOND)
    ttrue(raw.contains('401'), `an unauthenticated POST must be challenged, got [${firstHeaders(raw)}]`)
    ttrue(firstHeaders(raw).contains('connection: close'),
          `a challenge with an undrained body must be framed as closing, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 1, 'the connection must actually close, matching the header')

    raw = await exchange('GET /auth/basic/ HTTP/1.1\r\nHost: x\r\n\r\n' + SECOND)
    ttrue(raw.contains('401'), 'an unauthenticated GET must be challenged')
    ttrue(firstHeaders(raw).contains('connection: keep-alive'),
          `a challenge with no body must keep its connection, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 2, 'the connection must actually be reused, matching the header')
}

/*
    A complete chunked body must permit connection reuse.
 */
raw = await exchange('POST /action/test HTTP/1.1\r\nHost: x\r\n' +
                     'Content-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\n\r\n' +
                     '8\r\nname=joe\r\n0\r\n\r\n' + SECOND)
ttrue(raw.contains('200 OK'), `a chunked request must be served, got [${firstHeaders(raw)}]`)
ttrue(responseCount(raw) == 2, 'a complete chunked request must keep its connection')

raw = await exchange('POST /action/test HTTP/1.1\r\nHost: x\r\n' +
                     'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 8\r\n\r\n' +
                     'name=joe' + SECOND)
ttrue(raw.contains('200 OK'), 'a Content-Length request must be served')
ttrue(responseCount(raw) == 2, 'a complete Content-Length request must keep its connection')

if (thas('ME_GOAHEAD_AUTH')) {
    //  Every response sent before the body is read closes, including a form login redirect
    raw = await exchange('POST /auth/form/index.html HTTP/1.1\r\nHost: x\r\nContent-Type: text/plain\r\n' +
                         'Content-Length: 50\r\n\r\nshort' + SECOND)
    ttrue(raw.contains('302'), `an unauthenticated form POST must be redirected, got [${firstHeaders(raw)}]`)
    ttrue(firstHeaders(raw).contains('connection: close'),
          `a redirect with an undrained body must be framed as closing, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 1, 'the connection must actually close after the redirect')

    raw = await exchange('GET /auth/form/index.html HTTP/1.1\r\nHost: x\r\n\r\n' + SECOND)
    ttrue(raw.contains('302'), 'an unauthenticated form GET must be redirected')
    ttrue(firstHeaders(raw).contains('connection: keep-alive'),
          `a redirect with no body must keep its connection, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 2, 'the connection must be reused after a bodyless redirect')
}
