/*
    rom-method-body.tst - A ROM build cannot modify a document, so PUT and DELETE on a route that admits them are
    refused rather than answered with the document body as a GET would be. Builds a ROM server without TLS from the
    sources and the compiled image of a small document set, then exercises it over HTTP.
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { teq, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { createServer, connect } from 'node:net'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const ROOT = join(import.meta.dir, '..', '..')
const ccPath = Cmd.locate('cc') || Cmd.locate('gcc') || Cmd.locate('clang')
const cc = ccPath ? String(ccPath) : null
const webcompPath = Cmd.locate('webcomp')
const webcomp = webcompPath ? String(webcompPath) : join(ROOT, 'build', 'bin', 'webcomp')
const PROGRAMS = ['test.c', 'cgitest.c', 'rom.c']

function sleep(ms: number) {
    return new Promise(resolve => setTimeout(resolve, ms))
}

async function request(port: number, raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let data = ''
        socket.setTimeout(5000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('close', () => resolve(data))
        socket.on('error', () => resolve(data))
    })
}

function raw(method: string, path: string, body = ''): string {
    return `${method} ${path} HTTP/1.1\r\nHost: localhost\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n${body}`
}

function status(response: string): number {
    const m = response.match(/^HTTP\/1\.[01] (\d{3})/)
    return m ? Number(m[1]) : 0
}

function body(response: string): string {
    const at = response.indexOf('\r\n\r\n')
    return at < 0 ? '' : response.slice(at + 4)
}

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX host compiler')
} else if (spawnSync(webcomp, [], {encoding: 'utf8'}).status === null) {
    tskip('webcomp is not built')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-rom-'))
    try {
        //  A document set with a writable route, and the route file itself, compiled into the image
        mkdirSync(join(dir, 'web', 'put'), {recursive: true})
        writeFileSync(join(dir, 'web', 'index.html'), 'index\n')
        writeFileSync(join(dir, 'web', 'put', 'doc.txt'), 'DOC\n')
        writeFileSync(join(dir, 'route.txt'), 'route uri=/put/ methods=GET|PUT|DELETE\nroute uri=/\n')
        writeFileSync(join(dir, 'auth.txt'), '# no users\n')
        writeFileSync(join(dir, 'files.txt'),
            ['web', 'web/put', 'web/index.html', 'web/put/doc.txt', 'route.txt', 'auth.txt'].join('\n') + '\n')
        const image = spawnSync(webcomp, ['files.txt'], {cwd: dir, encoding: 'utf8'})
        teq(image.status, 0, 'webcomp compiles the document set: ' + image.stderr)
        writeFileSync(join(dir, 'rom.c'), image.stdout)

        //  The server itself, from the sources, without TLS or PAM so the build needs no third-party library
        const sources = readdirSync(join(ROOT, 'src')).filter(f => f.endsWith('.c') && !PROGRAMS.includes(f))
            .map(f => join(ROOT, 'src', f))
        const exe = join(dir, 'goahead-rom')
        const args = ['-O0', '-DME_ROM=1', '-DME_COM_SSL=0', '-DME_COM_OPENSSL=0', '-DME_COM_MBEDTLS=0',
            '-DME_COMPILER_HAS_PAM=0', '-DME_VERSION="0.0.0"', '-DME_DEBUG=0', '-I' + join(ROOT, 'src'), '-I' + join(ROOT, 'src', 'osdep'),
            ...sources, join(dir, 'rom.c'), '-lm', '-lpthread', '-o', exe]
        if (process.platform == 'linux') {
            args.push('-ldl')
        }
        const built = spawnSync(cc, args, {encoding: 'utf8'})
        teq(built.status, 0, 'the ROM server builds: ' + built.stderr)

        const probe = createServer()
        await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
        const port = (probe.address() as any).port
        await new Promise<void>(resolve => probe.close(() => resolve()))
        const server = startServer(exe, ['--no-strict', '--log', 'stderr:1', 'web', `http://127.0.0.1:${port}`],
            {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
        let logs = ''
        server.stdout!.on('data', data => logs += data.toString())
        server.stderr!.on('data', data => logs += data.toString())
        try {
            let ready = false
            for (let i = 0; i < 50 && !ready; i++) {
                ready = status(await request(port, raw('GET', '/index.html'))) == 200
                if (!ready) {
                    await sleep(100)
                }
            }
            ttrue(ready, 'the ROM server serves its documents: ' + logs)

            let response = await request(port, raw('PUT', '/put/doc.txt', 'NEW\n'))
            ttrue(status(response) == 405, `PUT of a ROM document is refused with 405, got ${status(response)}`)
            ttrue(!body(response).startsWith('DOC'), 'PUT of a ROM document does not answer with the document')

            response = await request(port, raw('DELETE', '/put/doc.txt'))
            ttrue(status(response) == 405, `DELETE of a ROM document is refused with 405, got ${status(response)}`)
            ttrue(!body(response).startsWith('DOC'), 'DELETE of a ROM document does not answer with the document')

            response = await request(port, raw('GET', '/put/doc.txt'))
            teq(status(response), 200, 'the document is still served by GET')
            ttrue(body(response).startsWith('DOC'), 'the document is unchanged')
        } finally {
            await stopServer(server)
        }
    } finally {
        rmSync(dir, {recursive: true, force: true})
    }
}
