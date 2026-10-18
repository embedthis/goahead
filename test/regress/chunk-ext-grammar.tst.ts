/* A chunk extension must match the RFC 9112 grammar, and a bare CR never ends a chunk line */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function send(request: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(request)
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

function header(): string {
    return 'POST /index.html HTTP/1.1\r\nHost: ' + uri.host +
        '\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n'
}

function status(raw: string): number {
    let m = raw.match(/HTTP\/1\.[01] (\d+)/)
    return m ? parseInt(m[1]) : 0
}

function show(line: string): string {
    return line.replace(/[\x00-\x1f\x7f]/g, c => '\\x' + c.charCodeAt(0).toString(16).padStart(2, '0'))
}

for (let bad of ['5\rjunk', '5;a\rb', '5;\x00', '5;a\x01', '5;', '5;=b', '5;a=', '5;a="open', '5;a b', '5;a="x\x01"',
        '5;a=b\r']) {
    let raw = await send(header() + bad + '\r\nhello\r\n0\r\n\r\n')
    ttrue(status(raw) == 400, `chunk line [${show(bad)}] must be refused, got ${status(raw)}`)
}

for (let good of ['5;a', '5;a=b', '5;a="q s"', '5;a="q\\"s"', '5;a;b=c', '5;a="\t"']) {
    let raw = await send(header() + good + '\r\nhello\r\n0\r\n\r\n')
    ttrue(status(raw) != 400, `chunk line [${show(good)}] is well formed and must be accepted, got ${status(raw)}`)
}


let raw = await send('POST /index.html HTTP/1.1\r\nHost: ' + uri.host + '\r\nTransfer-Encoding: chunked\r\n\r\n' +
    '5\r\nhello\r\n0\rjunk\r\n\r\nGET /index.html HTTP/1.1\r\nHost: ' + uri.host + '\r\nConnection: close\r\n\r\n')
ttrue(status(raw) == 400, `a bare CR in the last chunk line must be refused, got ${status(raw)}`)
let responses = raw.split(/HTTP\/1\.[01] \d+/).length - 1
ttrue(responses == 1, `only one response may be sent after a refused last chunk, got ${responses}`)

raw = await send('GET /index.html HTTP/1.0\r\n\r\n')
ttrue(raw.contains('200 OK'), 'the server must still serve after malformed chunk lines')
