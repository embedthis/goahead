/*
    action-post-only.tst - Actions need POST unless their route allows more, and login reads the body only
 */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

// Use HTTP/1.0 to read the complete response through connection close.
async function send(method: string, target: string, body: string = ''): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let request = method + ' ' + target + ' HTTP/1.0\r\nHost: localhost\r\n'
    if (method == 'POST') {
        request += 'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n'
    }
    await s.write(request + '\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

function location(response: string): string {
    let match = response.match(/\r\nLocation: ([^\r]*)\r\n/i)
    return match ? match[1] : ''
}

let response = await send('GET', '/postonly/test?name=forged')
ttrue(status(response) == 405, `GET of an action on a route without methods must be refused, got ${status(response)}`)
tcontains(response, '\r\nAllow: POST\r\n', 'and the 405 must name the allowed method')
ttrue(!response.includes('forged'), 'and the action must not run')

response = await send('HEAD', '/postonly/test')
ttrue(status(response) == 405, `HEAD must be refused too, got ${status(response)}`)

response = await send('POST', '/postonly/test', 'name=posted&address=here')
ttrue(status(response) == 200, `POST must run the action, got ${status(response)}`)
tcontains(response, 'posted', 'and the action sees the form')

response = await send('GET', '/action/test?name=allowed')
ttrue(status(response) == 200, `a route that names GET must still run the action, got ${status(response)}`)

if (thas('ME_GOAHEAD_AUTH')) {
    response = await send('POST', '/action/login?username=joshua&password=pass1', '')
    ttrue(status(response) == 302 && location(response).includes('login.html'),
          `credentials in the query string must not log in, got ${status(response)} to ${location(response)}`)

    response = await send('POST', '/action/login', 'username=joshua&password=pass1')
    ttrue(status(response) == 302 && location(response).includes('/auth/form/index.html'),
          `credentials in the body must log in, got ${status(response)} to ${location(response)}`)
}
