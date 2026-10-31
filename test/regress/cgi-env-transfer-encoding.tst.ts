/*
    CGI stdin is decoded and framed by CONTENT_LENGTH; withhold HTTP_TRANSFER_ENCODING.
    A control header confirms the environment dump is valid.
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function cgi(headers: string, body: string): Promise<string> {
    let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST ' + program + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nSwitches: -e\r\n' +
                  'Content-Type: text/plain\r\n' + headers + '\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

//  Every environment entry the program printed for a name, from its full environment dump
function entries(raw: string, name: string): string[] {
    let result: string[] = []
    let re = new RegExp('<P>' + name + '=([^<\\r\\n]*)</P>', 'g')
    let match
    while ((match = re.exec(raw)) != null) {
        result.push(match[1])
    }
    return result
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    let raw = await cgi('Transfer-Encoding: chunked\r\nX-Env-Probe: present\r\n', '5\r\nhello\r\n0\r\n\r\n')
    ttrue(/^HTTP\/1\.1 200 /.test(raw), `the CGI program must run, got [${raw.split('\r\n')[0]}]`)
    ttrue(raw.contains('HTTP_X_ENV_PROBE=present'), 'an ordinary request header is exported as HTTP_*')
    ttrue(entries(raw, 'CONTENT_LENGTH').length > 0 && entries(raw, 'CONTENT_LENGTH').every(v => v == '5'),
          `CONTENT_LENGTH must be the decoded size, got [${entries(raw, 'CONTENT_LENGTH')}]`)
    ttrue(entries(raw, 'HTTP_TRANSFER_ENCODING').length == 0,
          'a transfer coding the server has decoded must not be described to the CGI program')
}
