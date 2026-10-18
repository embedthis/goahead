/*
    cgi-inherited-files.tst - A CGI program inherits only stdin, stdout and stderr
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

if (thas('ME_GOAHEAD_CGI')) {
    /*
        Hold another client connection to detect unintended CGI inheritance.
     */
    let idle = new Socket
    idle.connect(ADDR)
    try {
        let raw = await get(process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest', '-S')
        ttrue(raw.contains('200 OK'), 'cgitest must run')
        let match = raw.match(/INHERITED=(\d+)/)
        ttrue(match != null, 'cgitest must report its inherited descriptors')
        ttrue(match != null && match[1] == '0',
              `a CGI program must inherit no descriptor above 2 or socket handle, got ${match ? match[1] : 'none'}`)
    } finally {
        idle.close()
    }
}
