/*
    rewrite-canonicalization.tst - A rewrite target goes through the same pipeline as a request target
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function rawGet(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
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

function rewriteTo(target: string): string {
    return '/test/probe?rewrite=' + target
}

let raw = await rawGet(rewriteTo('/home.html'))
ttrue(status(raw) == '200', `a rewrite to a real page must be served, got ${status(raw)}`)
ttrue(raw.contains('Hello'), 'the rewritten target must be the document served')

for (let target of ['/../../../etc/passwd', '/%2e%2e/%2e%2e/etc/passwd', '/home.html%00.txt',
                    '/dir/../../etc/passwd', 'home.html']) {
    raw = await rawGet(rewriteTo(target))
    ttrue(status(raw) != '200', `a rewrite to "${target}" must not be served, got ${status(raw)}`)
    ttrue(!raw.contains('root:'), `a rewrite to "${target}" must not reach the file system outside the root`)
}

if (thas('ME_GOAHEAD_JAVASCRIPT')) {
    raw = await rawGet(rewriteTo('/test.jst'))
    ttrue(status(raw) == '200', `a rewrite to a template must be served, got ${status(raw)}`)
    ttrue(!raw.contains('<%'), 'a rewrite to a template must run it, not disclose its source')

    raw = await rawGet('/test/probe.html?rewrite=/test.jst')
    ttrue(status(raw) == '200', `a rewrite from a page to a template must be served, got ${status(raw)}`)
    ttrue(!raw.contains('<%'), 'a rewrite that changes the extension must run the template, not disclose its source')
}

if (thas('ME_GOAHEAD_AUTH')) {
    raw = await rawGet(rewriteTo('/auth/basic/basic.html'))
    ttrue(status(raw) == '401', `a rewrite into a protected area must be challenged, got ${status(raw)}`)

    for (let target of ['/x/../auth/basic/basic.html', '//auth//basic//basic.html', '/auth/%62asic/basic.html']) {
        raw = await rawGet(rewriteTo(target))
        ttrue(status(raw) == '401' || status(raw) == '400' || status(raw) == '404',
              `a rewrite to "${target}" must be challenged or refused, got ${status(raw)}`)
        ttrue(!raw.contains('Welcome to Basic'), `a rewrite to "${target}" must not serve the protected page`)
    }
}

raw = await rawGet('/index.html')
ttrue(raw.contains('200 OK'), 'the server must still serve afterwards')
