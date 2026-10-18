/* Only an absolute-form target supplies the request authority */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const SELF = 'http://' + ADDR
const EVIL = 'http://evil.example'
const BASIC = 'Basic ' + Buffer.from('joshua:pass1').toString('base64')
const BODY = 'probe=forged'

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

function post(target: string, origin: string, authorization: string, host = ADDR): Promise<string> {
    return exchange('POST ' + target + ' HTTP/1.1\r\nHost: ' + host + '\r\nOrigin: ' + origin +
        '\r\nAuthorization: ' + authorization + '\r\nContent-Type: application/x-www-form-urlencoded\r\n' +
        'Content-Length: ' + BODY.length + '\r\nConnection: close\r\n\r\n' + BODY)
}

function get(target: string, host: string): Promise<string> {
    return exchange('GET ' + target + ' HTTP/1.1\r\nHost: ' + host + '\r\nConnection: close\r\n\r\n')
}

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

function md5(s: string): string {
    return createHash('md5').update(s).digest('hex')
}

function digest(method: string, target: string, nonce: string, opaque: string, nc: string): string {
    let cnonce = '0123456789abcdef'
    let ha1 = md5('joshua:example.com:pass1')
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + md5(method + ':' + target))
    return `Digest username="joshua", realm="example.com", nonce="${nonce}", uri="${target}", ` +
        `response="${response}", opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

const EMBEDDED = ['/x://evil.example', '/a://evil.example', '/http://evil.example']

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    //  (a) Basic: the control is refused, and no path-embedded variant runs the action
    let raw = await post('/basicaction/showTest', EVIL, BASIC)
    ttrue(status(raw) == 403, `a cross-site POST must be refused, got ${status(raw)}`)
    for (let prefix of EMBEDDED) {
        raw = await post(prefix + '/basicaction/showTest', EVIL, BASIC)
        ttrue(status(raw) != 200 && !raw.contains(BODY),
            `cross-site POST to ${prefix}/basicaction/showTest must not run the action, got ${status(raw)}`)
    }

    //  (b) Digest: the browser signs the literal target, so the uri check alone does not stop it
    let target = '/auth/digest/digest.html'
    let challenge = await get(target, ADDR)
    let nonce = (challenge.match(/nonce="([^"]*)"/) || [])[1] || ''
    let opaque = (challenge.match(/opaque="([^"]*)"/) || [])[1] || ''
    raw = await post(target, EVIL, digest('POST', target, nonce, opaque, '00000001'))
    ttrue(status(raw) == 403, `a cross-site Digest POST must be refused, got ${status(raw)}`)
    for (let [i, prefix] of EMBEDDED.entries()) {
        let embedded = prefix + target
        raw = await post(embedded, EVIL, digest('POST', embedded, nonce, opaque, '0000000' + (i + 2)))
        ttrue(status(raw) != 200, `cross-site Digest POST to ${embedded} must not be served, got ${status(raw)}`)
    }

    //  (c) An embedded authority never becomes the request host
    raw = await get('/x://evil.example/action/hostTest', 'h.example')
    ttrue(!raw.contains('host=evil.example'), 'a path-embedded authority must not become the host')

    //  (d) A query holding "://" leaves the Host header in charge
    raw = await get('/action/hostTest?r=http://q', 'h.example')
    ttrue(raw.contains('host=h.example'), `Host must identify the site when the query holds "://", got ${raw}`)

    //  (e) A same-origin POST whose query holds "://" is allowed
    raw = await post('/basicaction/showTest?next=http://x', SELF, BASIC)
    ttrue(status(raw) == 200 && raw.contains(BODY),
        `a same-origin POST with "://" in the query must be served, got ${status(raw)}`)

    //  A genuine absolute-form target names the authority, port included, that Origin is compared with
    raw = await post(SELF + '/basicaction/showTest', SELF, BASIC, 'attacker.example')
    ttrue(status(raw) == 200 && raw.contains(BODY),
        `an absolute-form POST whose authority matches Origin must be served, got ${status(raw)}`)
    raw = await get('http://target.example:8080/action/showTest', 'attacker.example')
    ttrue(raw.contains('HTTP_HOST=target.example:8080'), 'a CGI or action must see the absolute-form authority')

    //  (f) Only http and https absolute-form targets are accepted
    for (let bad of ['ftp://h.example/index.html', 'x://h.example/index.html', 'http:///index.html',
            'http://user@h.example/index.html']) {
        raw = await get(bad, 'h.example')
        ttrue(status(raw) == 400, `target ${bad} must be refused, got ${status(raw)}`)
    }

    //  (g) The target authority passes the Host header check
    raw = await get('http://bad_host!/index.html', 'h.example')
    ttrue(status(raw) == 400, `an absolute-form authority with a bad character must be refused, got ${status(raw)}`)
}
