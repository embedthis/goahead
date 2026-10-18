/* An authentication denial always answers, whatever the route's redirects */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: ' + ADDR + '\r\n\r\n')
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
    let raw = await get('/wildauth/page.html')
    ttrue(status(raw) == 302, `a denial on a wildcard-redirect route must redirect, got ${status(raw)}`)
    ttrue(/\r\nLocation: [^\r]*\/index\.html\r\n/i.test(raw), 'to the wildcard target')

    raw = await get('/okauth/page.html')
    ttrue(status(raw) == 401, `a denial with no matching redirect must answer 401, got ${status(raw)}`)
    ttrue(/\r\nWWW-Authenticate: Basic/i.test(raw), 'with the Basic challenge')
}
