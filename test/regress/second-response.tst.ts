/*
    second-response.tst - One response per request
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

function declaredLength(raw: string): number {
    let match = raw.match(/[Cc]ontent-[Ll]ength: (\d+)/)
    return match ? parseInt(match[1]) : -1
}

function bodyLength(raw: string): number {
    let end = raw.indexOf('\r\n\r\n')
    return end < 0 ? -1 : raw.length - (end + 4)
}

const SECOND = 'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'

let raw = await exchange('GET /action/doubleTest HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(raw.contains('200 OK'), 'the first response must be sent')
ttrue(raw.contains('FIRSTRESPONSE'), 'the first response body must be delivered')

ttrue(!raw.contains('SECONDRESPONSE'), 'a second websResponse body must not be appended')
ttrue(!raw.contains('Access Error'), 'a later websError page must not be appended')
ttrue(!raw.contains('secondredirect.html'), 'a later websRedirect must not be appended')
ttrue(!raw.contains('Location:'), 'a later redirect must not add a Location header')

ttrue(declaredLength(raw) > 0, 'the response must declare a length')
ttrue(bodyLength(raw) == declaredLength(raw),
      `the body must be exactly the declared length, declared ${declaredLength(raw)} got ${bodyLength(raw)}`)

raw = await exchange('GET /action/doubleTest HTTP/1.1\r\nHost: x\r\n\r\n' + SECOND)
ttrue(raw.split('HTTP/1.1 ').length - 1 == 2, 'the connection must carry exactly two responses')
ttrue(raw.contains('FIRSTRESPONSE'), 'the probe response must still be delivered')
ttrue(raw.contains('Hello /index'), 'the following request must be answered from the right offset')
