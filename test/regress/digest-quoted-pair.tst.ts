/*
    digest-quoted-pair.tst - An escaped quote does not end a Digest quoted-string
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TARGET = '/auth/digest/digest.html'

function md5(s: string): string {
    return createHash('md5').update(s).digest('hex')
}

async function get(authorization: string | null): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let auth = authorization ? 'Authorization: ' + authorization + '\r\n' : ''
    await s.write('GET ' + TARGET + ' HTTP/1.1\r\nHost: x\r\n' + auth + 'Connection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function field(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '="([^"]*)"'))
    return match ? match[1] : ''
}

function rest(realm: string, nonce: string, opaque: string): string {
    let nc = '00000001', cnonce = 'abcdef0123456789'
    let ha1 = md5('joshua:' + realm + ':pass1')
    let ha2 = md5('GET:' + TARGET)
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + ha2)
    return `realm="${realm}", nonce="${nonce}", uri="${TARGET}", response="${response}", ` +
        `opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

if (thas('ME_GOAHEAD_AUTH')) {
    let challenge = await get(null)
    ttrue(status(challenge) == '401', 'an unauthenticated request must be challenged')
    let realm = field(challenge, 'realm')
    let nonce = field(challenge, 'nonce')
    let opaque = field(challenge, 'opaque')
    ttrue(nonce != '', 'the challenge must carry a nonce')

    /*
        A valid control separates parser rejection from an incorrect digest.
     */
    let raw = await get('Digest username="joshua", ' + rest(realm, nonce, opaque))
    ttrue(status(raw) == '200', `a well-formed Digest credential must authenticate, got ${status(raw)}`)

    /*
        RFC 7616 reads this as one username containing escaped quotes.
     */
    raw = await get('Digest username="x\\", username="joshua", ' + rest(realm, nonce, opaque))
    ttrue(status(raw) == '401',
          `an escaped quote must not end the username and let a second one replace it, got ${status(raw)}`)

    raw = await get('Digest username="joshua\\')
    ttrue(status(raw) == '401', 'a quoted value ending in a lone backslash must be refused, not overrun')

    raw = await get(null)
    ttrue(status(raw) == '401', 'the server must still challenge afterwards')
}
