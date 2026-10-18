/*
    keepalive-reset.tst - Persistence must be re-derived per request, not inherited from the last one
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Read through close or timeout to distinguish connection persistence.
 */
async function drain(s: Socket): Promise<string> {
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
        /* Read timeout: the server is holding the connection open. */
    }
    return buf.toString()
}

function headers(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return end < 0 ? raw : raw.slice(0, end)
}

/*
    Follow a persistent HTTP/1.1 request with HTTP/1.0 to check state reset.
 */
let s = new Socket
s.connect(ADDR)
await s.write('GET /index.html HTTP/1.1\r\nHost: ' + uri.host + '\r\n\r\n')

let first = new ByteArray(65536, true)
await s.read(first, -1)
let firstRaw = first.toString()
ttrue(firstRaw.contains('200 OK'), 'the HTTP/1.1 request must be served')
ttrue(headers(firstRaw).toLowerCase().contains('connection: keep-alive'),
      'an HTTP/1.1 request must get a persistent connection')

await s.write('GET /index.html HTTP/1.0\r\n\r\n')
let secondRaw = await drain(s)
s.close()

ttrue(secondRaw.contains('200 OK'), 'the HTTP/1.0 request must still be served')
ttrue(!headers(secondRaw).toLowerCase().contains('connection: keep-alive'),
      `an HTTP/1.0 request must not be promised keep-alive, got headers [${headers(secondRaw)}]`)

/*
    Send a third request to distinguish connection close from read timeout.
 */
let s2 = new Socket
s2.connect(ADDR)
await s2.write('GET /index.html HTTP/1.1\r\nHost: ' + uri.host + '\r\n\r\n')
let warm = new ByteArray(65536, true)
await s2.read(warm, -1)
ttrue(warm.toString().contains('200 OK'), 'the warm-up request must be served')

await s2.write('GET /index.html HTTP/1.0\r\n\r\n')
let closing = await drain(s2)
ttrue(closing.contains('200 OK'), 'the HTTP/1.0 request must be served before the close')

let answered = 0
try {
    await s2.write('GET /index.html HTTP/1.1\r\nHost: ' + uri.host + '\r\n\r\n')
    let third = await drain(s2)
    answered = third.contains('200 OK') ? 1 : 0
} catch {
}
s2.close()
ttrue(answered == 0, 'the connection must be closed after the HTTP/1.0 response, not reused')

let s3 = new Socket
s3.connect(ADDR)
await s3.write('GET /index.html HTTP/1.0\r\nConnection: keep-alive\r\n\r\n')
let explicit = new ByteArray(65536, true)
await s3.read(explicit, -1)
s3.close()
ttrue(headers(explicit.toString()).toLowerCase().contains('connection: keep-alive'),
      'an HTTP/1.0 request that asks for keep-alive must still get it')
