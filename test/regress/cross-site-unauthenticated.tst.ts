/* A state-changing request from another site is refused on every route */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function send(method: string, target: string, origin: string | null, body: string = ''): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let headers = 'Host: ' + ADDR + '\r\n' + (origin ? 'Origin: ' + origin + '\r\n' : '') +
        (body ? 'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n' : '')
    await s.write(method + ' ' + target + ' HTTP/1.0\r\n' + headers + '\r\n' + body)
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
    return match ? Number(match[1]) : 0
}

for (let origin of ['https://evil.example', 'null']) {
    let raw = await send('POST', '/action/showTest', origin, 'marker=CSRF1')
    ttrue(status(raw) == 403, `a POST with Origin ${origin} must be refused, got ${status(raw)}`)
    ttrue(!raw.includes('marker=CSRF1'), 'and the action must not run')
}

let raw = await send('POST', '/action/showTest', 'http://' + ADDR, 'marker=SAME')
ttrue(status(raw) == 200 && raw.includes('marker=SAME'), `a POST from this site must run, got ${status(raw)}`)

raw = await send('POST', '/action/showTest', null, 'marker=NOORIGIN')
ttrue(status(raw) == 200 && raw.includes('marker=NOORIGIN'), `a POST without Origin must run, got ${status(raw)}`)

raw = await send('GET', '/action/showTest?marker=GET', 'https://evil.example')
ttrue(status(raw) == 200, `a GET from another site is not refused, got ${status(raw)}`)

raw = await send('POST', '/crosssite/showTest', 'https://evil.example', 'marker=ALLOWED')
ttrue(status(raw) == 200 && raw.includes('marker=ALLOWED'),
    `a route with cross-site=allow must accept a POST from another site, got ${status(raw)}`)
