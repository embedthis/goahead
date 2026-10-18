/* A header name with an underscore creates no HTTP_ variable */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

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

if (thas('ME_GOAHEAD_CGI')) {
    let raw = await exchange('POST /cgi-bin/cgitest HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: text/plain\r\n' +
        'Transfer_Encoding: chunked\r\nContent_Length: 99999\r\nContent-Length: 5\r\nSwitches: -e\r\n' +
        'Connection: close\r\n\r\nhello')
    ttrue(raw.contains('200 OK'), `cgitest must run, got [${raw.slice(0, 80)}]`)
    ttrue(entries(raw, 'HTTP_TRANSFER_ENCODING').length == 0,
          'an underscore spelling of Transfer-Encoding must not create HTTP_TRANSFER_ENCODING')
    ttrue(entries(raw, 'HTTP_CONTENT_LENGTH').every(v => v == '5'),
          `HTTP_CONTENT_LENGTH must hold only the real header, got [${entries(raw, 'HTTP_CONTENT_LENGTH')}]`)
    ttrue(entries(raw, 'CONTENT_LENGTH').every(v => v == '5'), 'CONTENT_LENGTH must be the framed length')

    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: 127.0.0.1\r\nX_Forwarded_For: 10.0.0.1\r\n' +
        'X-Forwarded-For: 192.0.2.1\r\nSwitches: -e\r\nConnection: close\r\n\r\n')
    let forwarded = entries(raw, 'HTTP_X_FORWARDED_FOR')
    ttrue(forwarded.length > 0 && forwarded.every(v => v == '192.0.2.1'),
          `an underscore header must not join the hyphen header's variable, got [${forwarded}]`)

    raw = await exchange('POST /cgi-bin/cgitest HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: text/plain\r\n' +
        'Transfer-Encoding: chunked\r\nSwitches: -e\r\nConnection: close\r\n\r\n5\r\nhello\r\n0\r\n\r\n')
    ttrue(entries(raw, 'HTTP_TRANSFER_ENCODING').some(v => v == 'chunked'),
          'a real Transfer-Encoding header must still create its variable')
}
