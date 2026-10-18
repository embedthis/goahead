/* Only the nonce exactly as issued is accepted, and a refused request records no count */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const PAGE = '/auth/digest/digest.html'
const ADMIN = '/auth/digest/admin/index.html'

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

function status(raw: string): number {
    let m = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return m ? parseInt(m[1]) : 0
}

function credential(user: string, password: string, nonce: string, target: string, nc: string): string {
    let cnonce = 'a1b2c3d4e5f60718'
    let ha1 = md5(user + ':example.com:' + password)
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + md5('GET:' + target))
    return `Digest username="${user}", realm="example.com", nonce="${nonce}", uri="${target}", ` +
        `response="${response}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

async function freshNonce(): Promise<string> {
    let raw = await get(PAGE)
    let m = raw.match(/nonce="([^"]*)"/)
    return m ? m[1] : ''
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    let nonce = await freshNonce()
    let plain = Buffer.from(nonce, 'base64').toString()
    let [hash, time, counter] = plain.split(':')
    let variants = [
        nonce + (nonce.endsWith('=') ? '' : '=') + 'A'.repeat(100),
        Buffer.from(plain + ':x').toString('base64'),
        Buffer.from(hash + ':0' + time + ':' + counter).toString('base64'),
    ]
    for (let variant of variants) {
        let raw = await get(PAGE, credential('joshua', 'pass1', variant, PAGE, '00000001'))
        ttrue(status(raw) == 401, `a nonce spelled other than as issued must be refused, got ${status(raw)}`)
    }
    let raw = await get(PAGE, credential('joshua', 'pass1', nonce, PAGE, '00000001'))
    ttrue(status(raw) == 200, `the nonce as issued must be accepted, got ${status(raw)}`)

    //  mary may view but not manage: a request refused for its abilities must not consume the count
    nonce = await freshNonce()
    raw = await get(ADMIN, credential('mary', 'pass2', nonce, ADMIN, '00000001'))
    ttrue(status(raw) == 401, `mary must be refused the admin page, got ${status(raw)}`)
    raw = await get(PAGE, credential('mary', 'pass2', nonce, PAGE, '00000001'))
    ttrue(status(raw) == 200, `a count refused for its abilities must still be usable, got ${status(raw)}`)
}
