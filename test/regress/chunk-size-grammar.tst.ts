/*
    chunk-size-grammar.tst - A chunk-size line must match the grammar, not whatever hextoi accepts
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Use a fixed five-byte body with variable chunk-size syntax.
 */
async function postChunked(sizeLine: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST /index.html HTTP/1.1\r\nHost: ' + uri.host +
        '\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n' +
        sizeLine + '\r\nhello\r\n0\r\n\r\n')
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

function status(raw: string): number {
    let m = raw.match(/HTTP\/1\.[01] (\d+)/)
    return m ? parseInt(m[1]) : 0
}

for (let bad of ['0x5', '0X5', '5junk', '5 ', '5:1', '5\tx', '+5', ' 5', '5.0']) {
    let raw = await postChunked(bad)
    ttrue(status(raw) == 400, `chunk size [${bad.replace(/\t/, '\\t')}] must be refused, got ${status(raw)}`)
}

let raw = await postChunked('')
ttrue(status(raw) == 400, `an empty chunk size must be refused, got ${status(raw)}`)

for (let good of ['5', '05', '0005', '5;name=value', '5;ext']) {
    let raw = await postChunked(good)
    ttrue(status(raw) != 400, `chunk size [${good}] is well formed and must be accepted, got ${status(raw)}`)
}

let s = new Socket
s.connect(ADDR)
await s.write('GET /index.html HTTP/1.0\r\n\r\n')
let final = new ByteArray(65536, true)
try {
    while ((await s.read(final, -1)) != null) {}
} catch {
}
s.close()
ttrue(final.toString().contains('200 OK'), 'the server must still serve after malformed chunk sizes')
