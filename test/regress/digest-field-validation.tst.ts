/*
    digest-field-validation.tst - Digest fields are validated, and hashed, as the client sent them
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

async function get(authorization: string | null, target: string = TARGET): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let auth = authorization ? 'Authorization: ' + authorization + '\r\n' : ''
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\n' + auth + 'Connection: close\r\n\r\n')
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

function field(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '="([^"]*)"'))
    return match ? match[1] : ''
}

/*
    Hash with the server realm, vary the claimed realm and URI, and advance nonce counts.
 */
let count = 0

function credential(realmField: string, nonce: string, opaque: string, uriField: string, a2: string): string {
    let nc = (++count).toString(16).padStart(8, '0'), cnonce = 'abcdef0123456789'
    let ha1 = md5('joshua:example.com:pass1')
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + md5(a2))
    return `Digest username="joshua", realm="${realmField}", nonce="${nonce}", uri="${uriField}", ` +
        `response="${response}", opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

if (thas('ME_GOAHEAD_AUTH')) {
    let challenge = await get(null)
    ttrue(status(challenge) == '401', 'an unauthenticated request must be challenged')
    let nonce = field(challenge, 'nonce')
    let opaque = field(challenge, 'opaque')
    ttrue(!challenge.contains('domain="null"'), 'the challenge must not advertise domain="null"')

    let raw = await get(credential('example.com', nonce, opaque, TARGET, 'GET:' + TARGET))
    ttrue(status(raw) == '200', `a correct Digest credential must authenticate, got ${status(raw)}`)

    raw = await get('Digest username="joshua", realm="example.com", nonce="", uri="' + TARGET +
                    '", response="00000000000000000000000000000000", qop=auth, nc=00000001, cnonce="c"')
    ttrue(status(raw) == '401', `an empty nonce must be refused, got ${status(raw)}`)
    raw = await get(null)
    ttrue(status(raw) == '401', 'the server must survive an empty nonce')

    /*
        Keep the digest valid while testing a mismatched realm.
     */
    raw = await get(credential('other.example', nonce, opaque, TARGET, 'GET:' + TARGET))
    ttrue(status(raw) == '401', `a credential for another realm must be refused, got ${status(raw)}`)

    /*
        Reject a digest over a truncated URI; accept one over the complete URI.
     */
    let longUri = TARGET + '?pad=' + 'x'.repeat(400)
    let full = 'GET:' + longUri
    raw = await get(credential('example.com', nonce, opaque, longUri, full.slice(0, 255)), longUri)
    ttrue(status(raw) == '401', `a response over a truncated URI must be refused, got ${status(raw)}`)

    raw = await get(credential('example.com', nonce, opaque, longUri, full), longUri)
    ttrue(status(raw) == '200', `a response over the full long URI must authenticate, got ${status(raw)}`)
}
