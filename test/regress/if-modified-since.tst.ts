/*
    if-modified-since.tst - An unparseable If-Modified-Since must be ignored
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { ttrue, tget } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function conditional(since: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET /index.html HTTP/1.1\r\nHost: x\r\nIf-Modified-Since: ' + since +
                  '\r\nConnection: close\r\n\r\n')
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

for (let bad of ['2147483647', '0', 'garbage', '2009-05-21T16:06:05.000Z', 'Sun, 06 Nov 1994 08:49:37',
                 'Sun, 06 Nov 1994 08:49:37 EST']) {
    let raw = await conditional(bad)
    ttrue(raw.contains('200 OK'), `an invalid If-Modified-Since "${bad}" must be ignored, got [${status(raw)}]`)
}

let raw = await conditional('Tue, 01 Jan 2115 00:00:00 GMT')
ttrue(raw.contains('304'), `a valid HTTP-date must still be honoured, got [${status(raw)}]`)

raw = await conditional('Sun, 06 Nov 1994 08:49:37 GMT')
ttrue(raw.contains('200 OK'), `a date older than the file must return it, got [${status(raw)}]`)
