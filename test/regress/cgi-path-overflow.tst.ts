/*
    cgi-path-overflow.tst - An over-long CGI path is refused, not parsed out of stack residue
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function rawGet(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
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

if (thas('ME_GOAHEAD_CGI')) {
    /*
        Exceed the CGI path buffer while remaining within the URI limit.
     */
    let long = '/cgi-bin/' + 'a'.repeat(400)
    let raw = await rawGet(long)
    ttrue(status(raw) == '414', `an over-long CGI path must be refused with 414, got ${status(raw)}`)

    ttrue(!raw.contains('SCRIPT_FILENAME'), 'no CGI environment may be composed from an over-long path')

    raw = await rawGet('/cgi-bin/' + 'a'.repeat(200))
    ttrue(status(raw) != '414', `a path inside the buffer must not be refused for length, got ${status(raw)}`)

    raw = await rawGet('/cgi-bin/cgitest')
    ttrue(status(raw) == '200', `an ordinary CGI request must still be served, got ${status(raw)}`)

    raw = await rawGet('/index.html')
    ttrue(raw.contains('200 OK'), 'the server must still serve afterwards')
}
