/* Methods are case-sensitive and an unimplemented method gets 501 */

import { tget, thas, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = new URL(tget('TM_HTTP') || 'http://127.0.0.1:18080')
const BASIC = 'Basic ' + Buffer.from('joshua:pass1').toString('base64')

function raw(request: string): Promise<string> {
    return new Promise(resolve => {
        let data = ''
        let sock = connect(Number(HTTP.port), HTTP.hostname)
        let finish = () => { sock.destroy(); resolve(data) }
        sock.setTimeout(5000, finish)
        sock.on('connect', () => sock.write(request))
        sock.on('data', (d: any) => data += d)
        sock.on('end', finish)
        sock.on('error', finish)
    })
}

function statuses(response: string): string[] {
    return response.match(/^HTTP\/1\.[01] \d+/gm) || []
}

let r = await raw('head /index.html HTTP/1.1\r\nHost: x\r\n\r\nGET /index.html HTTP/1.1\r\nHost: x\r\n\r\n')
ttrue(statuses(r).length == 1 && r.startsWith('HTTP/1.1 501'), 'lowercase head gets one 501 and the connection closes')

r = await raw(`post /basicaction/showTest HTTP/1.1\r\nHost: x\r\nConnection: close\r\nAuthorization: ${BASIC}\r\n` +
    'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 15\r\n\r\nmarker=METHOD42')
ttrue(r.startsWith('HTTP/1.1 501') && !r.includes('METHOD42'), 'lowercase post gets 501 and the action does not run')

r = await raw('put /tmp/method-case.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Length: 2\r\n\r\nhi')
ttrue(r.startsWith('HTTP/1.1 501'), 'lowercase put gets 501')
r = await raw('GET /tmp/method-case.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(!r.startsWith('HTTP/1.1 200'), 'lowercase put stored nothing')
r = await raw('delete /tmp/method-case.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(r.startsWith('HTTP/1.1 501'), 'lowercase delete gets 501')

r = await raw('FOO /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(r.startsWith('HTTP/1.1 501'), 'an unknown method gets 501')

r = await raw('HEAD /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(r.startsWith('HTTP/1.1 200'), 'HEAD is unchanged')
r = await raw('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
ttrue(r.startsWith('HTTP/1.1 200'), 'GET is unchanged')
if (thas('ME_GOAHEAD_AUTH')) {
    r = await raw(`POST /basicaction/showTest HTTP/1.1\r\nHost: x\r\nConnection: close\r\nAuthorization: ${BASIC}\r\n` +
        'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 15\r\n\r\nmarker=METHOD42')
    ttrue(r.startsWith('HTTP/1.1 200') && r.includes('METHOD42'), 'POST is unchanged')
}

r = await raw('PATCH /putaction/showTest HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Length: 0\r\n\r\n')
ttrue(!r.startsWith('HTTP/1.1 501'), 'a method named by a route is known')
