/* The file handler never returns template source */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { spawn } from 'node:child_process'
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { createServer } from 'node:net'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function get(target: string, credentials: string | null = null, addr = ADDR): Promise<string> {
    let s = new Socket
    s.connect(addr)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' +
                  (credentials ? 'Authorization: Basic ' + credentials + '\r\n' : '') + '\r\n')
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

if (thas('ME_GOAHEAD_AUTH')) {
    //  joshua:pass1
    let raw = await get('/auth/basic/source.jst', 'am9zaHVhOnBhc3Mx')
    ttrue(status(raw) == '404', `a template under a file-handler route must be refused, got ${status(raw)}`)
    ttrue(!raw.contains('<%'), 'the template source must not be returned')
}
if (thas('ME_GOAHEAD_JAVASCRIPT')) {
    let raw = await get('/test.jst')
    ttrue(status(raw) == '200' && raw.contains('Hello ASP World') && !raw.contains('<%'),
          'the template route must still run templates')
}

/*
    Test the shipped public /pub/ example under strict mode: templates remain private and login remains public.
 */
if (thas('ME_GOAHEAD_AUTH') && thas('ME_GOAHEAD_JAVASCRIPT')) {
    const ROOT = join(import.meta.dir, '..', '..')
    let shipped = readFileSync(join(ROOT, 'src', 'route.txt'), 'utf8').split('\n')
    let start = shipped.findIndex(line => line.includes('to require a form login'))
    let example: string[] = []
    for (let line of shipped.slice(start + 1)) {
        let match = line.match(/^#\s+(route .*)/)
        if (match) {
            example.push(match[1])
        } else if (example.length) {
            break
        }
    }
    ttrue(example.some(route => route.startsWith('route uri=/pub/')), 'found the shipped form-login example')
    let standard = shipped.filter(line => line.startsWith('route '))

    let dir = mkdtempSync(join(tmpdir(), 'goahead-pub-'))
    mkdirSync(join(dir, 'web', 'pub'), {recursive: true})
    writeFileSync(join(dir, 'web', 'pub', 'source.jst'), '<% write("pub-template"); %>\n')
    writeFileSync(join(dir, 'web', 'pub', 'login.html'), 'pub-login\n')
    writeFileSync(join(dir, 'route.txt'), [...example, ...standard].join('\n') + '\n')
    writeFileSync(join(dir, 'auth.txt'), '')

    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    const addr = '127.0.0.1:' + port

    const server = spawn(join(ROOT, 'build', 'bin', 'goahead'), ['--route', 'route.txt', '--auth', 'auth.txt', 'web',
        addr], {cwd: dir, stdio: 'ignore'})
    try {
        let login = ''
        for (let i = 0; i < 100 && status(login) != '200'; i++) {
            try {
                login = await get('/pub/login.html', null, addr)
            } catch {
            }
            if (status(login) != '200') {
                await new Promise(resolve => setTimeout(resolve, 25))
            }
        }
        ttrue(status(login) == '200' && login.contains('pub-login'),
              'the login page under /pub/ must be served without credentials, in strict mode')
        let raw = await get('/pub/source.jst', null, addr)
        ttrue(status(raw) == '404', `a template under /pub/ must be refused, got ${status(raw)}`)
        ttrue(!raw.contains('<%') && !raw.contains('pub-template'), 'the /pub/ template source must not be returned')
    } finally {
        const stopped = new Promise<void>(resolve => server.once('exit', () => resolve()))
        if (server.exitCode == null) {
            server.kill()
            await stopped
        }
        rmSync(dir, {recursive: true, force: true})
    }
}
