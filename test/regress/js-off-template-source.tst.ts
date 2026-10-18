/*
    js-off-template-source.tst - A build without templates never serves template source
 */

import { tskip, ttrue } from '@embedthis/testme'
import { spawn } from 'node:child_process'
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { get } from 'node:http'
import { createServer } from 'node:net'
import { join } from 'node:path'
import { tmpdir } from 'node:os'
import { cc, compile, isWindows, ROOT } from '../utils/amalgamation'

async function request(port: number, path: string): Promise<{status: number, body: string}> {
    return await new Promise((resolve, reject) => {
        const req = get({host: '127.0.0.1', port, path}, res => {
            let body = ''
            res.on('data', data => body += data)
            res.on('end', () => resolve({status: res.statusCode || 0, body}))
        })
        req.on('error', reject)
        req.setTimeout(3000, () => req.destroy(new Error('request timed out')))
    })
}

if (isWindows) {
    tskip('Rebuilding the amalgamation is a POSIX-only test')
} else if (!cc) {
    tskip('No C compiler on PATH to rebuild the amalgamation at ME_GOAHEAD_JAVASCRIPT=0')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-jsoff-'))
    const web = join(dir, 'web')
    mkdirSync(web)
    writeFileSync(join(web, 'index.html'), 'js-off-index\n')
    writeFileSync(join(web, 'page.jst'), '<% write("js-off-template"); %>\n')
    writeFileSync(join(web, 'page.asp'), '<% write("js-off-template"); %>\n')
    writeFileSync(join(dir, 'route.txt'), 'route uri=/\n')
    writeFileSync(join(dir, 'auth.txt'), '')

    const exe = join(dir, 'goahead-jsoff')
    const build = compile(exe, '-DME_GOAHEAD_JAVASCRIPT=0', join(ROOT, 'src', 'goahead.c'))
    ttrue(build.status == 0, 'Cannot build the ME_GOAHEAD_JAVASCRIPT=0 server:\n' + String(build.error))
    if (build.status == 0) {
        const probe = createServer()
        await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
        const port = (probe.address() as any).port
        await new Promise<void>(resolve => probe.close(() => resolve()))

        const server = spawn(exe, ['--route', 'route.txt', '--auth', 'auth.txt', 'web', `127.0.0.1:${port}`],
            {cwd: dir, stdio: 'ignore'})
        try {
            let ready = false
            for (let i = 0; i < 100 && !ready; i++) {
                try {
                    ready = (await request(port, '/index.html')).status == 200
                } catch {
                    await new Promise(resolve => setTimeout(resolve, 25))
                }
            }
            ttrue(ready, 'the ME_GOAHEAD_JAVASCRIPT=0 server serves static files')
            for (const path of ['/page.jst', '/page.asp']) {
                const response = await request(port, path)
                ttrue(response.status == 404, `${path} must be refused without templates, got ${response.status}`)
                ttrue(!response.body.includes('<%') && !response.body.includes('js-off-template'),
                    `${path} must not return the template source`)
            }
        } finally {
            const stopped = new Promise<void>(resolve => server.once('exit', () => resolve()))
            if (server.exitCode == null) {
                server.kill()
                await stopped
            }
        }
    }
    rmSync(dir, {recursive: true, force: true})
}
