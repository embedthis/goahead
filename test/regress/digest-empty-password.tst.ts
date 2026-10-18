/* Digest refuses an account whose stored password is not an MD5 hash */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TARGET = '/auth/digest/digest.html'

function md5(s: string): string {
    return createHash('md5').update(s).digest('hex')
}

async function get(target: string, authorization: string | null = null): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let auth = authorization ? 'Authorization: ' + authorization + '\r\n' : ''
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\n' + auth + 'Connection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function field(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '="([^"]*)"'))
    return match ? match[1] : ''
}

function credential(username: string, ha1: string, nonce: string, opaque: string, nc: string): string {
    let cnonce = '0123456789abcdef'
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + md5('GET:' + TARGET))
    return `Digest username="${username}", realm="example.com", nonce="${nonce}", uri="${TARGET}", ` +
        `response="${response}", opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not in this build')
} else {
    let challenge = await get(TARGET)
    ttrue(status(challenge) == '401', 'an unauthenticated request must be challenged')
    let nonce = field(challenge, 'nonce')
    let opaque = field(challenge, 'opaque')

    let raw = await get(TARGET, credential('nopass', '', nonce, opaque, '00000001'))
    ttrue(status(raw) == '401', `a Digest response from an empty password must be refused, got ${status(raw)}`)

    raw = await get(TARGET, credential('nopass', md5('nopass:example.com:'), nonce, opaque, '00000002'))
    ttrue(status(raw) == '401', `a Digest response from the hash of an empty password must be refused, got ${status(raw)}`)

    raw = await get(TARGET, credential('joshua', md5('joshua:example.com:pass1'), nonce, opaque, '00000003'))
    ttrue(status(raw) == '200', `an account with a stored hash must still authenticate, got ${status(raw)}`)
}
