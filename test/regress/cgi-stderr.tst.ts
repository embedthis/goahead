/* A CGI program's stderr never reaches the response */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function get(target: string, switches: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\nSwitches: ' + switches +
                  '\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

if (thas('ME_GOAHEAD_CGI')) {
    let raw = await get('/cgi-bin/cgitest', '-E%20C:\\x:1:STDERR-MARKER%20-c%20text/plain%20-b%201')
    let end = raw.indexOf('\r\n\r\n')
    let headers = end < 0 ? raw : raw.slice(0, end)
    ttrue(raw.startsWith('HTTP/1.1 200'), `the program must run, got [${raw.slice(0, 60)}]`)
    ttrue(!/^c:/im.test(headers), 'a stderr line must not become a response header')
    ttrue(!raw.contains('STDERR-MARKER'), 'stderr output must not reach the response')
    ttrue(raw.toLowerCase().contains('content-type: text/plain'), 'the program\'s own output must be the response')
}
