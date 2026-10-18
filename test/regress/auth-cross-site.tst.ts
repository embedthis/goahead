/* An authenticated route refuses a state-changing request sent by another site */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TARGET = '/basicaction/showTest'
const AUTH = 'Basic ' + Buffer.from('joshua:pass1').toString('base64')

async function send(method: string, origin: string | null, body: string = '', site: string = ''): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let headers = 'Host: ' + ADDR + '\r\nAuthorization: ' + AUTH + '\r\n' +
        (origin ? 'Origin: ' + origin + '\r\n' : '') + (site ? 'Sec-Fetch-Site: ' + site + '\r\n' : '') +
        (body ? 'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n' : '')
    await s.write(method + ' ' + TARGET + ' HTTP/1.0\r\n' + headers + '\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not in this build')
} else {
    let raw = await send('POST', 'http://evil.example', 'probe=forged')
    ttrue(status(raw) == 403, `a POST from another site must be refused, got ${status(raw)}`)
    ttrue(!raw.includes('probe=forged'), 'and the action must not run')

    raw = await send('POST', 'http://' + ADDR, 'probe=same')
    ttrue(status(raw) == 200, `a POST from this site must run, got ${status(raw)}`)
    tcontains(raw, 'probe=same', 'with its body')

    raw = await send('POST', null, 'probe=noorigin')
    ttrue(status(raw) == 200, `a POST without Origin must run, got ${status(raw)}`)

    raw = await send('GET', 'http://evil.example')
    ttrue(status(raw) == 200, `a GET from another site is not refused, got ${status(raw)}`)

    //  Without Origin, a browser's Sec-Fetch-Site still names a request from another site
    for (let site of ['cross-site', 'same-site']) {
        raw = await send('POST', null, 'probe=fetchsite', site)
        ttrue(status(raw) == 403, `a POST with Sec-Fetch-Site: ${site} must be refused, got ${status(raw)}`)
        ttrue(!raw.includes('probe=fetchsite'), 'and the action must not run')
    }
    for (let site of ['same-origin', 'none']) {
        raw = await send('POST', null, 'probe=fetchsite', site)
        ttrue(status(raw) == 200, `a POST with Sec-Fetch-Site: ${site} must run, got ${status(raw)}`)
    }
}
