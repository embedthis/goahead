/*
    request-target-fragment.tst - A request target has no fragment, so a target containing # is refused
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

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

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

function responseCount(raw: string): number {
    return raw.split('HTTP/1.1 ').length - 1 + raw.split('HTTP/1.0 ').length - 1
}

for (let target of ['/action/showTest#?role=admin', '/index.html#frag', '/action/showTest?a=1#b=2', '/p#a?b=1']) {
    //  Pipeline a second request to prove the refusal also closes the connection
    let raw = await exchange('GET ' + target + ' HTTP/1.1\r\nHost: x\r\n\r\n' +
        'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    ttrue(status(raw) == 400, `"${target}" must be refused with 400, got ${status(raw)}`)
    ttrue(responseCount(raw) == 1, `"${target}" must close the connection, got ${responseCount(raw)} responses`)
    ttrue(!raw.contains('role=admin') && !raw.contains('b=2'), `"${target}" must not reach the handler`)
}

//  Control - a query without a fragment is served as before
let raw = await exchange('GET /action/showTest?a=1 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(status(raw) == 200, `a query without a fragment must be served, got ${status(raw)}`)
ttrue(raw.contains('a=1'), 'the query must reach the handler')
