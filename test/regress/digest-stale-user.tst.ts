/*
    digest-stale-user.tst - Digest verifies the user its header names, not a user an earlier route bound
 */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TARGET = '/stale/admin/index.html'
const ADMIN = '/auth/digest/admin/index.html'

function md5(s: string): string {
    return createHash('md5').update(s).digest('hex')
}

async function send(request: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(request)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

async function get(target: string, cookie: string, authorization: string = ''): Promise<string> {
    return send('GET ' + target + ' HTTP/1.0\r\nHost: ' + ADDR + '\r\n' +
                (cookie ? 'Cookie: ' + cookie + '\r\n' : '') +
                (authorization ? 'Authorization: ' + authorization + '\r\n' : '') + '\r\n')
}

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

function field(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '="([^"]*)"'))
    return match ? match[1] : ''
}

function session(raw: string): string {
    let match = raw.match(/Set-Cookie: (-goahead-session-=[^;\r]*)/i)
    return match ? match[1] : ''
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not in this build')
} else {
    let body = 'username=mary&password=pass2'
    let login = await send('POST /action/login HTTP/1.0\r\nHost: ' + ADDR +
        '\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n\r\n' + body)
    let cookie = session(login)
    ttrue(cookie != '', 'mary must be able to log in with a form')

    let challenge = await get(TARGET, cookie)
    ttrue(status(challenge) == 401, `the Digest route must challenge a form session, got ${status(challenge)}`)
    let nonce = field(challenge, 'nonce')
    let opaque = field(challenge, 'opaque')

    //  Name joshua, answer with mary's password
    let cnonce = '0123456789abcdef'
    let nc = '00000001'
    let response = md5(md5('mary:example.com:pass2') + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' +
                       md5('GET:' + TARGET))
    let forged = `Digest username="joshua", realm="example.com", nonce="${nonce}", uri="${TARGET}", ` +
                 `response="${response}", opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
    let raw = await get(TARGET, cookie, forged)
    ttrue(status(raw) != 200, `a Digest header naming another user must not be accepted, got ${status(raw)}`)

    let issued = session(raw) || cookie
    raw = await get(ADMIN, issued)
    ttrue(status(raw) != 200, `the session must not pass another user's Digest pages, got ${status(raw)}`)
}
