/*
    param-count-limit.tst - A request may carry at most ME_GOAHEAD_LIMIT_PARAMS parameters
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

//  The default of ME_GOAHEAD_LIMIT_PARAMS in src/goahead.h.
const LIMIT = 512

async function request(method: string, target: string, body: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let headers = method + ' ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\n'
    if (body) {
        headers += 'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n'
    }
    await s.write(headers + '\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

/*
    End with name=Joe to show whether parameters were parsed.
 */
function params(count: number): string {
    let letters = 'abcdefghijklmnopqrstuvwxyz'
    let list: string[] = []
    for (let i = 0; i < count - 1; i++) {
        list.push(letters[Math.floor(i / 26) % 26] + letters[i % 26])
    }
    list.push('name=Joe')
    return list.join('&')
}

let raw = await request('GET', '/action/test?' + params(LIMIT), '')
ttrue(status(raw) == '200', `a query at the limit must be accepted, got ${status(raw)}`)
ttrue(raw.contains('name: Joe'), 'a query at the limit must be parsed')

raw = await request('GET', '/action/test?' + params(LIMIT + 1), '')
ttrue(status(raw) == '413', `a query over the limit must be refused with 413, got ${status(raw)}`)
ttrue(!raw.contains('name: Joe'), 'the handler must not run for a query over the limit')

raw = await request('POST', '/action/test', params(LIMIT))
ttrue(status(raw) == '200', `a form at the limit must be accepted, got ${status(raw)}`)
ttrue(raw.contains('name: Joe'), 'a form at the limit must be parsed')

raw = await request('POST', '/action/test', params(LIMIT + 1))
ttrue(status(raw) == '413', `a form over the limit must be refused with 413, got ${status(raw)}`)
ttrue(!raw.contains('name: Joe'), 'the handler must not run for a form over the limit')

raw = await request('GET', '/action/test?name=Joe', '')
ttrue(status(raw) == '200' && raw.contains('name: Joe'), 'an ordinary request must still be answered')
