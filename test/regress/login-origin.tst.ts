/*
    login-origin.tst - The login action refuses a POST sent by another site's page
 */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const SELF = 'http://' + ADDR
const BODY = 'username=joshua&password=pass1'

async function login(origin: string | null, target: string = '/action/login'): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST ' + target + ' HTTP/1.0\r\nHost: ' + ADDR + '\r\n' +
                  (origin != null ? 'Origin: ' + origin + '\r\n' : '') +
                  'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + BODY.length + '\r\n\r\n' + BODY)
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

function loggedIn(response: string): boolean {
    return status(response) == 302 && /\r\nLocation: [^\r]*\/auth\/form\/index\.html/i.test(response)
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled, so there is no login action')
} else {
    let response = await login('http://attacker.example')
    ttrue(status(response) == 403, `a login from another site must be refused, got ${status(response)}`)
    ttrue(!/-goahead-session-/.test(response), 'and must not issue a session')

    response = await login('null')
    ttrue(status(response) == 403, `a login from an opaque origin must be refused, got ${status(response)}`)

    response = await login(SELF)
    ttrue(loggedIn(response), `a login from this server's own page must succeed, got ${status(response)}`)

    response = await login(null)
    ttrue(loggedIn(response),
          `a login without Origin, from a non-browser client, must succeed, got ${status(response)}`)

    response = await login('http://attacker.example', '/action/login?HTTP_ORIGIN=' + encodeURIComponent(SELF))
    ttrue(status(response) == 403, `an Origin in the query must not override the header, got ${status(response)}`)
}
