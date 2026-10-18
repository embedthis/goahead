/*
    session-auth-protocol.tst - A session satisfies only routes of the protocol that established it
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

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

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function sessionCookie(raw: string): string {
    let match = raw.match(/Set-Cookie: (-goahead-session-=[^;\r\n]*)/)
    return match ? match[1] : ''
}

function get(target: string, extra: string): Promise<string> {
    return exchange('GET ' + target + ' HTTP/1.1\r\nHost: x\r\n' + extra + 'Connection: close\r\n\r\n')
}

const BASIC_PAGE = '/auth/basic/basic.html'
const FORM_PAGE = '/auth/form/index.html'
const DIGEST_PAGE = '/auth/digest/digest.html'

if (thas('ME_GOAHEAD_AUTH')) {
    let body = 'username=joshua&password=pass1'
    let raw = await exchange('POST /action/login HTTP/1.1\r\nHost: x\r\n' +
                             'Content-Type: application/x-www-form-urlencoded\r\n' +
                             'Content-Length: ' + body.length + '\r\nConnection: close\r\n\r\n' + body)
    let formCookie = sessionCookie(raw)
    ttrue(formCookie != '', `a form login must establish a session, got [${raw.slice(0, 60)}]`)

    raw = await get(FORM_PAGE, 'Cookie: ' + formCookie + '\r\n')
    ttrue(status(raw) == '200', `a form session must satisfy a form route, got ${status(raw)}`)

    raw = await get(BASIC_PAGE, 'Cookie: ' + formCookie + '\r\n')
    ttrue(status(raw) == '401', `a form session must not satisfy a Basic route, got ${status(raw)}`)

    //  A Basic login is cached only in a session the client already holds
    body = 'number=1'
    raw = await exchange('POST /action/sessionTest HTTP/1.1\r\nHost: x\r\n' +
                         'Content-Type: application/x-www-form-urlencoded\r\n' +
                         'Content-Length: ' + body.length + '\r\nConnection: close\r\n\r\n' + body)
    let plainCookie = sessionCookie(raw)
    ttrue(plainCookie != '', 'the test action must issue a session')

    let credentials = Buffer.from('joshua:pass1').toString('base64')
    raw = await get(BASIC_PAGE, 'Authorization: Basic ' + credentials + '\r\nCookie: ' + plainCookie + '\r\n')
    ttrue(status(raw) == '200', `Basic credentials must authenticate a Basic route, got ${status(raw)}`)
    let basicCookie = sessionCookie(raw)
    ttrue(basicCookie != '', 'a Basic authentication must leave a session')

    raw = await get(FORM_PAGE, 'Cookie: ' + basicCookie + '\r\n')
    ttrue(status(raw) != '200', `a Basic session must not satisfy a form route, got ${status(raw)}`)

    raw = await get(BASIC_PAGE, 'Cookie: ' + basicCookie + '\r\n')
    ttrue(status(raw) == '200', `a Basic session must still satisfy a Basic route, got ${status(raw)}`)

    raw = await get(DIGEST_PAGE, 'Cookie: ' + basicCookie + '\r\n')
    ttrue(status(raw) == '401', `a Basic session must not satisfy a Digest route, got ${status(raw)}`)
    ttrue(/WWW-Authenticate: Digest/i.test(raw), 'a Basic session sent to a Digest route must get a Digest challenge')
    ttrue(!raw.contains('Welcome to Digest'), 'a Basic session must not be served the Digest page')

    //  A cached session is still held to the route's abilities
    body = 'username=mary&password=pass2'
    raw = await exchange('POST /action/login HTTP/1.1\r\nHost: x\r\n' +
                         'Content-Type: application/x-www-form-urlencoded\r\n' +
                         'Content-Length: ' + body.length + '\r\nConnection: close\r\n\r\n' + body)
    let viewCookie = sessionCookie(raw)
    ttrue(viewCookie != '', 'a form login without the manage ability must establish a session')

    raw = await get(FORM_PAGE, 'Cookie: ' + viewCookie + '\r\n')
    ttrue(status(raw) != '200', `a session without the manage ability must not satisfy a manage route, got ${status(raw)}`)
}
