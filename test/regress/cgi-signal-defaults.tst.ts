/*
    cgi-signal-defaults.tst - A CGI program starts with default signal handling
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

//  HTTP/1.0, so the output arrives unframed and no chunk boundary can split the text searched for
async function get(target: string, switches: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: x\r\nSWITCHES: ' + switches + '\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

if (thas('ME_GOAHEAD_CGI') && process.platform != 'win32') {
    let raw = await get('/cgi-bin/cgitest', '-S')
    ttrue(raw.contains('200 OK'), 'cgitest must run')
    ttrue(raw.contains('SIGPIPE=default'), `a CGI program must start with SIGPIPE at its default, got ${
        (raw.match(/SIGPIPE=\w+/) || ['none'])[0]}`)
    ttrue(raw.contains('BLOCKED=0'), 'a CGI program must start with no signals blocked')
}
