/*
    chunk-header-segmentation.tst - The chunk parse depends on the bytes, not on how they arrive
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(parts: string[]): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    for (let i = 0; i < parts.length; i++) {
        try {
            await s.write(parts[i])
        } catch {
            break
        }
        if (i + 1 < parts.length) {
            //  Let the server parse what it has before the rest arrives.
            await new Promise(resolve => setTimeout(resolve, 120))
        }
    }
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

const HEAD = 'POST /action/test HTTP/1.1\r\nHost: x\r\n' +
    'Content-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\n' +
    'Connection: close\r\n\r\n'

/*
    Pad chunk extensions to test the header length boundary.
 */
function chunked(extLength: number): string {
    return '8;' + 'x'.repeat(extLength) + '\r\nname=joe\r\n0\r\n\r\n'
}

/*
    The same header must yield the same result whole or split before CRLF.
 */
for (let extLength of [8, 40, 70, 76, 80, 200]) {
    let body = chunked(extLength)
    let cut = body.indexOf('\r\nname=joe')

    let whole = await exchange([HEAD + body])
    let split = await exchange([HEAD + body.slice(0, cut), body.slice(cut)])

    ttrue(status(whole) == status(split),
          `a chunk header with a ${extLength} byte extension must parse the same whole (${status(whole)}) ` +
          `and split (${status(split)})`)
}

let raw = await exchange([HEAD + '8\r\nname=joe\r\n0\r\n\r\n'])
ttrue(status(raw) == '200', `a plain chunked request must be served, got ${status(raw)}`)

raw = await exchange([HEAD + '8\r\nname=', 'joe\r\n0\r\n\r\n'])
ttrue(status(raw) == '200', `a chunked request split inside its data must be served, got ${status(raw)}`)

raw = await exchange([HEAD + '8', '\r\nname=joe\r\n0\r\n\r\n'])
ttrue(status(raw) == '200', `a chunked request split inside its header must be served, got ${status(raw)}`)

raw = await exchange(['GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'])
ttrue(raw.contains('200 OK'), 'the server must still serve afterwards')
