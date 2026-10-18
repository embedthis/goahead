/*
    header-value-ows.tst - Only SP and HTAB are optional white space in a header value
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

/*
    Retain bare CR for rejection instead of trimming it as whitespace.
 */
let raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\r\n\r\n')
ttrue(raw.contains('400'), `a header value ending in a bare CR must be rejected, got [${status(raw)}]`)
ttrue(!raw.contains('200 OK'), 'the request must not be served')

raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\r\n\r\n')
ttrue(raw.contains('400'), `a bare CR on Transfer-Encoding must be rejected, got [${status(raw)}]`)

raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: 0\n\r\n\r\n')
ttrue(raw.contains('400'), `a header value ending in a bare LF must be rejected, got [${status(raw)}]`)

/*
    Accept SP and HTAB as optional whitespace.
 */
raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: \t0 \t\r\nConnection: close\r\n\r\n')
ttrue(raw.contains('200 OK'), `SP and HTAB must still be trimmed as OWS, got [${status(raw)}]`)
