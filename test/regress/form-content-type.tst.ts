/* The body is parsed as a form only for the form media type, compared by equality */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function post(contentType: string, body: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST /action/showTest HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: ' + contentType +
                  '\r\nContent-Length: ' + body.length + '\r\nConnection: close\r\n\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

for (let type of ['Application/X-WWW-Form-Urlencoded', 'application/x-www-form-urlencoded; charset=utf-8',
                  ' application/x-www-form-urlencoded ']) {
    let raw = await post(type, 'ctprobe=parsed')
    ttrue(raw.contains('ctprobe=parsed'), `a form posted as [${type}] must be parsed`)
}
for (let type of ['text/plain; x=application/x-www-form-urlencoded', 'application/x-www-form-urlencodedx',
                  'text/application/x-www-form-urlencoded']) {
    let raw = await post(type, 'ctprobe=parsed')
    ttrue(!raw.contains('ctprobe=parsed'), `a body posted as [${type}] must not be parsed as a form`)
}
