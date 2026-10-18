/* Repeated form keys are joined in linear time */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const REPEATS = 511                 // ME_GOAHEAD_LIMIT_PARAMS - 1

async function post(target: string, body: string): Promise<{raw: string, elapsed: number}> {
    let started = Date.now()
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST ' + target + ' HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' +
                  'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n\r\n')
    for (let offset = 0; offset < body.length; offset += 1024 * 1024) {
        await s.write(body.slice(offset, offset + 1024 * 1024))
    }
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return {raw: response.toString(), elapsed: Date.now() - started}
}

let value = 'a=' + 'x'.repeat(8 * 1024 * 1024)
let control = await post('/index.html', value)
let repeated = await post('/index.html', value + '&a='.repeat(REPEATS))
ttrue(control.raw.contains('200 OK') && repeated.raw.contains('200 OK'), 'both posts must be served')
ttrue(repeated.elapsed <= Math.max(5 * control.elapsed, 1000),
      `repeated keys must cost about as much as the value alone: ${repeated.elapsed} ms against ${control.elapsed} ms`)

let raw = (await post('/action/showTest', 'a=v&b=1&a=w&a=')).raw
ttrue(raw.contains('\na=v w \n'), 'repeated values must join with spaces in order')
ttrue(raw.contains('\nb=1\n'), 'other keys are unchanged')

raw = (await post('/action/showTest?a=Q', 'a=B')).raw
ttrue(raw.contains('\na=Q B\n'), 'a key in the query and the body must join query first')
