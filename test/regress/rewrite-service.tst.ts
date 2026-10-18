/* A rewrite from a service callback is authorized by its target route and stays in the root */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { existsSync, rmSync, writeFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TEST = fileURLToPath(new URL('..', import.meta.url))

async function rawRequest(method: string, target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(method + ' ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function rawGet(target: string): Promise<string> {
    return rawRequest('GET', target)
}

function status(raw: string): string {
    let match = raw.match(/HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function rewriteTo(target: string): string {
    return '/svcrewrite/probe?rewrite=' + target
}

let raw = await rawGet(rewriteTo('/home.html'))
ttrue(status(raw) == '200', `a service rewrite to a real page must be served, got ${status(raw)}`)
ttrue(raw.contains('Hello'), 'the rewritten target must be the document served')

if (thas('ME_GOAHEAD_AUTH')) {
    for (let target of ['/auth/basic/basic.html', '/x/../auth/basic/basic.html', '/auth/basic/admin/']) {
        raw = await rawGet(rewriteTo(target))
        ttrue(status(raw) == '401', `a service rewrite to "${target}" must be challenged, got ${status(raw)}`)
        ttrue(!raw.contains('Welcome to Basic'), `a service rewrite to "${target}" must not serve the protected page`)
    }
}

let victim = TEST + 'zrewrite-victim.txt'
try {
    writeFileSync(victim, 'victim')
    for (let target of ['/tmp/../../zrewrite-victim.txt', '/tmp/%2e%2e/%2e%2e/zrewrite-victim.txt',
                        '/tmp/..%2f..%2fzrewrite-victim.txt']) {
        raw = await rawRequest('DELETE', rewriteTo(target))
        ttrue(!status(raw).startsWith('2'), `a DELETE rewritten to "${target}" must be refused, got ${status(raw)}`)
        ttrue(existsSync(victim), `a DELETE rewritten to "${target}" must not remove a file outside the root`)
    }
} finally {
    rmSync(victim, { force: true })
}

raw = await rawGet('/index.html')
ttrue(raw.contains('200 OK'), 'the server must still serve afterwards')
