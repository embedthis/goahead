/*
    document-containment.tst - A request cannot reach a file outside the document root through a link
 */

import { ByteArray, Config, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync, rmSync, symlinkSync, writeFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const TEST = fileURLToPath(new URL('..', import.meta.url))
const WEB = TEST + 'web/'

async function request(method: string, target: string, body: string = ''): Promise<string> {
    let s = new Socket
    s.connect(uri.host + ':' + uri.port)
    let length = method == 'PUT' ? 'Content-Length: ' + body.length + '\r\n' : ''
    await s.write(`${method} ${target} HTTP/1.1\r\nHost: x\r\n${length}Connection: close\r\n\r\n${body}`)
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

if (String(Config.OS).startsWith('win')) {
    tskip('Containment is checked on POSIX systems, and links need privilege on Windows')
} else {
    let links = [
        [WEB + 'zcontain-out.txt', '../auth.txt'],
        [WEB + 'zcontain-dir', '..'],
        [WEB + 'zcontain-in.txt', 'index.html'],
        [WEB + 'zcontain-indir', 'jst'],
        [WEB + 'tmp/zcontain-escape', '../..'],
    ]
    let victim = TEST + 'zcontain-victim.txt'
    let planted = TEST + 'zcontain-put.txt'
    try {
        for (let [path, target] of links) {
            rmSync(path, { force: true })
            symlinkSync(target, path)
        }
        let secret = readFileSync(TEST + 'auth.txt', 'utf8').split('\n').find(l => l.startsWith('user ')) || ''

        let raw = await request('GET', '/zcontain-out.txt')
        ttrue(status(raw) == '404', `a link to a file outside the root must not be served, got ${status(raw)}`)
        ttrue(!secret || !raw.includes(secret), 'no content of the outside file may be sent')
        raw = await request('HEAD', '/zcontain-out.txt')
        ttrue(status(raw) == '404', `HEAD must not describe a file outside the root, got ${status(raw)}`)
        raw = await request('GET', '/zcontain-dir/auth.txt')
        ttrue(status(raw) == '404', `a file through a linked directory outside the root must not be served, got ${status(raw)}`)
        raw = await request('GET', '/zcontain-dir')
        ttrue(status(raw) == '404', `a linked directory outside the root must not be redirected into, got ${status(raw)}`)

        raw = await request('GET', '/zcontain-in.txt')
        ttrue(status(raw) == '200', `a link inside the root must be served, got ${status(raw)}`)
        if (thas('ME_GOAHEAD_JAVASCRIPT')) {
            raw = await request('GET', '/zcontain-indir/query.jst?q=1')
            ttrue(status(raw) == '200' && raw.includes('q=[1]'),
                  `a linked directory inside the root must be served, got ${status(raw)}`)
        }

        raw = await request('PUT', '/tmp/zcontain-escape/zcontain-put.txt', 'planted')
        ttrue(status(raw) == '404', `a PUT through a link out of the root must be refused, got ${status(raw)}`)
        ttrue(!existsSync(planted), 'the PUT must not create a file outside the root')

        writeFileSync(victim, 'keep')
        raw = await request('DELETE', '/tmp/zcontain-escape/zcontain-victim.txt')
        ttrue(status(raw) == '404', `a DELETE through a link out of the root must be refused, got ${status(raw)}`)
        ttrue(existsSync(victim), 'the DELETE must not remove a file outside the root')

        raw = await request('PUT', '/tmp/zcontain-ok.txt', 'inside')
        ttrue(status(raw) == '201' || status(raw) == '204', `a PUT inside the root must succeed, got ${status(raw)}`)
        ttrue(existsSync(WEB + 'tmp/zcontain-ok.txt'), 'the PUT must create the file inside the root')

        raw = await request('GET', '/zcontain-missing/zcontain-missing.txt')
        ttrue(status(raw) == '404', `a missing file under a missing directory must be 404, got ${status(raw)}`)
        raw = await request('GET', '/index.html/zcontain-missing.txt')
        ttrue(status(raw) == '404', `a path through a file must be 404, got ${status(raw)}`)
    } finally {
        for (let [path] of links) {
            rmSync(path, { force: true })
        }
        for (let path of [victim, planted, WEB + 'tmp/zcontain-ok.txt']) {
            rmSync(path, { force: true })
        }
    }
}
