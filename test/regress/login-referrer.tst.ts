/*
    login-referrer.tst - After login, return only to a page on this server
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function post(target: string, body: string, cookie: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST ' + target + ' HTTP/1.1\r\nHost: x\r\n' +
                  (cookie ? 'Cookie: ' + cookie + '\r\n' : '') +
                  'Content-Type: application/x-www-form-urlencoded\r\n' +
                  'Content-Length: ' + body.length + '\r\nConnection: close\r\n\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function sessionCookie(raw: string): string {
    let match = raw.match(/Set-Cookie: (-goahead-session-=[^;\r\n]*)/)
    return match ? match[1] : ''
}

function location(raw: string): string {
    let match = raw.match(/[Ll]ocation: ([^\r\n]*)/)
    return match ? match[1] : ''
}

/*
    Record a session referrer, then return the login redirect Location.
 */
async function loginWithReferrer(referrer: string | null): Promise<string> {
    let body = 'number=1' + (referrer != null ? '&referrer=' + encodeURIComponent(referrer) : '')
    let raw = await post('/action/sessionTest', body, '')
    let cookie = sessionCookie(raw)
    ttrue(cookie != '', 'the probe must establish a session')
    raw = await post('/action/login', 'username=joshua&password=pass1', cookie)
    let target = location(raw)
    //  Log out: sessions outlive the test on the shared server and an account holds a limited number
    await post('/action/logout', '', sessionCookie(raw) || cookie)
    return target
}

if (thas('ME_GOAHEAD_AUTH')) {
    let loc = await loginWithReferrer('/home.html')
    ttrue(loc.endsWith('/home.html'), `a local referrer must be returned to, got [${loc}]`)

    loc = await loginWithReferrer('/home.html?next=http:///elsewhere')
    ttrue(loc.endsWith('/home.html?next=http:///elsewhere'),
          `a local target must not be cut apart at a scheme in its query, got [${loc}]`)

    for (let referrer of ['http://evil.example/', 'https://evil.example/', '//evil.example/', '/\\evil.example/',
                          'javascript:alert(1)']) {
        loc = await loginWithReferrer(referrer)
        ttrue(!loc.contains('evil.example') && !loc.contains('javascript'),
              `a referrer "${referrer}" must not be followed, got [${loc}]`)
        ttrue(loc.endsWith('/auth/form/index.html'),
              `a refused referrer must fall back to the route's success page, got [${loc}]`)
    }

    loc = await loginWithReferrer(null)
    ttrue(loc.endsWith('/auth/form/index.html'), `no referrer must use the route's success page, got [${loc}]`)
}
