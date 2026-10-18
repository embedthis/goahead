/*
    duplicate-headers.tst - A single-valued header may appear once
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

function status(raw: string): string {
    let match = raw.match(/HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

const DUPLICATES = [
    ['Host', 'Host: one.example\r\nHost: two.example\r\n'],
    ['Authorization', 'Authorization: Basic am9zaHVhOnBhc3Mx\r\nAuthorization: Basic bWFyeTpwYXNz\r\n'],
    ['Content-Type', 'Content-Type: text/plain\r\nContent-Type: application/json\r\n'],
    ['Content-Length', 'Content-Length: 0\r\nContent-Length: 5\r\n'],
    ['Transfer-Encoding', 'Transfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n'],
]

for (let [name, headers] of DUPLICATES) {
    let raw = await exchange('GET /index.html HTTP/1.1\r\n' + headers + 'Connection: close\r\n\r\n')
    ttrue(status(raw) == '400', `a duplicate ${name} must be refused, got ${status(raw)}`)
}

let raw = await exchange('GET /index.html HTTP/1.1\r\nHost: one.example\r\n' +
                         'Content-Type: text/plain\r\nConnection: close\r\n\r\n')
ttrue(status(raw) == '200', `a single instance of each must be served, got ${status(raw)}`)

/*
    Join repeated Cookie fields with semicolons for CGI consumers.
 */
if (thas('ME_GOAHEAD_CGI')) {
    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\n' +
                         'Cookie: a=1\r\nCookie: b=2\r\nConnection: close\r\n\r\n')
    ttrue(status(raw) == '200', `duplicate Cookie headers must be accepted, got ${status(raw)}`)
    ttrue(raw.contains('HTTP_COOKIE=a=1; b=2'),
          'duplicate Cookie headers must be joined with the delimiter their own grammar uses')
}

raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(raw.contains('200 OK'), 'the server must still serve afterwards')
