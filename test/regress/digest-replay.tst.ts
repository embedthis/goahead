/*
    digest-replay.tst - A Digest credential authorizes one request, once
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

async function get(target: string, authorization: string | null = null): Promise<string> {
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

function credential(nonce: string, opaque: string, uriField: string, nc: string): string {
    let cnonce = 'fedcba9876543210'
    let ha1 = md5('joshua:example.com:pass1')
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + md5('GET:' + uriField))
    return `Digest username="joshua", realm="example.com", nonce="${nonce}", uri="${uriField}", ` +
        `response="${response}", opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

if (thas('ME_GOAHEAD_AUTH')) {
    let challenge = await get(TARGET)
    ttrue(status(challenge) == '401', 'an unauthenticated request must be challenged')
    let nonce = field(challenge, 'nonce')
    let opaque = field(challenge, 'opaque')
    let raw: string

    let first = credential(nonce, opaque, TARGET, '00000001')
    raw = await get(TARGET, first)
    ttrue(status(raw) == '200', `a fresh credential must authenticate, got ${status(raw)}`)

    raw = await get(TARGET, first)
    ttrue(status(raw) == '401', `the same request sent again must be refused, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, TARGET, '00000002'))
    ttrue(status(raw) == '200', `the next nonce count must authenticate, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, TARGET, '00000002'))
    ttrue(status(raw) == '401', `a repeated nonce count must be refused, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, TARGET, '00000001'))
    ttrue(status(raw) == '401', `a lower nonce count must be refused, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, TARGET, '00000010'))
    ttrue(status(raw) == '200', `a count may skip ahead, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, TARGET, '11'))
    ttrue(status(raw) == '401', `a nonce count that is not eight hex digits must be refused, got ${status(raw)}`)

    challenge = await get(TARGET)
    let other = field(challenge, 'nonce')
    ttrue(other != '' && other != nonce, 'a new challenge must issue a new nonce')
    raw = await get(TARGET, credential(other, opaque, TARGET, '00000001'))
    ttrue(status(raw) == '200', `a new nonce must accept its first count, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, TARGET, '00000010'))
    ttrue(status(raw) == '401', `a count must stay refused after another nonce is used, got ${status(raw)}`)

    raw = await get(TARGET + '?other=1', credential(nonce, opaque, TARGET, '00000020'))
    ttrue(status(raw) == '401', `a credential for another query must be refused, got ${status(raw)}`)

    raw = await get('/auth/digest/', credential(nonce, opaque, TARGET, '00000021'))
    ttrue(status(raw) == '401', `a credential for another resource must be refused, got ${status(raw)}`)

    raw = await get(TARGET + '?other=1', credential(nonce, opaque, TARGET + '?other=1', '00000022'))
    ttrue(status(raw) == '200', `a credential naming the request's own target must authenticate, got ${status(raw)}`)

    raw = await get(TARGET, credential(nonce, opaque, 'http://127.0.0.1' + TARGET, '00000023'))
    ttrue(status(raw) == '200', `an absolute-form uri= naming the target must authenticate, got ${status(raw)}`)

    raw = await get('/auth/digest/', credential(nonce, opaque, 'http://127.0.0.1' + TARGET, '00000024'))
    ttrue(status(raw) == '401', `an absolute-form uri= naming another resource must be refused, got ${status(raw)}`)
}
