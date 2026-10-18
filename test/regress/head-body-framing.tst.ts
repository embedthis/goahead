/* A HEAD response carries no body, so a pipelined request after it is framed correctly */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

for (let target of ['/dir', '/auth/form/index.html', '/secure/index.html', '/test.jst', '/action/test']) {
    let raw = await exchange('HEAD ' + target + ' HTTP/1.1\r\nHost: x\r\n\r\n' +
        'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    let end = raw.indexOf('\r\n\r\n')
    ttrue(end > 0, `HEAD ${target} must be answered`)
    let rest = raw.slice(end + 4)
    ttrue(/^HTTP\/1\.1 200 /.test(rest),
        `HEAD ${target}: the next response must start right after its headers, got [${rest.slice(0, 40)}]`)
    let statuses = raw.match(/^HTTP\/1\.[01] \d+/gm) || []
    ttrue(statuses.length == 2, `HEAD ${target}: exactly two responses, got ${statuses.length}`)
}

//  An error page: HEAD declares the length GET sends, and carries none of it
function contentLength(raw: string): string {
    let m = raw.match(/\r\nContent-Length: (\d+)\r\n/i)
    return m ? m[1] : ''
}
let get = await exchange('GET /no-such-page.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
let head = await exchange('HEAD /no-such-page.html HTTP/1.1\r\nHost: x\r\n\r\n' +
    'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(contentLength(get) != '' && contentLength(head) == contentLength(get),
    `HEAD must declare the error page length GET sends, got ${contentLength(head)} and ${contentLength(get)}`)
ttrue(/^HTTP\/1\.1 200 /.test(head.slice(head.indexOf('\r\n\r\n') + 4)), 'the error HEAD must carry no body')
