/*
    Refuse PUT with 404 for both unresolved parents and parents outside the document root.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { existsSync, mkdirSync, readdirSync, rmSync, symlinkSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'containment-missing-parent.dir')
const WEB = join(HOME, 'web')
const OUTSIDE = join(HOME, 'private')
const ENDPOINT = '127.0.0.1:' + (process.env.MISSING_PARENT_PORT || '18996')
const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []

async function put(path: string): Promise<number> {
    let response = await fetch(`http://${ENDPOINT}${path}`, { method: 'PUT', body: 'planted' })
    await response.text()
    return response.status
}

if (process.platform == 'win32') {
    tskip('Containment is checked on POSIX systems, and links need privilege on Windows')
} else {
    rmSync(HOME, { recursive: true, force: true })
    mkdirSync(join(WEB, 'tmp'), { recursive: true })
    mkdirSync(join(OUTSIDE, 'exists'), { recursive: true })
    writeFileSync(join(WEB, 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(OUTSIDE, 'exists', 'keep.txt'), 'outside\n')
    writeFileSync(join(HOME, 'route.txt'), 'route uri=/tmp/ methods=PUT|DELETE\nroute uri=/\n')
    writeFileSync(join(HOME, 'auth.txt'), '')
    //  A link out of the root, as an operator mistake would leave it
    symlinkSync('../../private', join(WEB, 'tmp', 'zlink'))

    let log = ''
    let server = startServer('goahead', ['--no-strict', '--route', 'route.txt', ...AUTH, 'web', ENDPOINT], { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)
    try {
        let index: Response | null = null
        for (let i = 0; i < 50 && !index; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                index = await fetch(`http://${ENDPOINT}/index.html`)
                await index.text()
            } catch {}
        }
        ttrue(index != null, `the server must start: ${log}`)

        let present = await put('/tmp/zlink/exists/zplanted.txt')
        let absent = await put('/tmp/zlink/absent/zplanted.txt')
        ttrue(present == absent,
            `a PUT through a link out of the root must answer the same whether the directory exists (${present}) or not (${absent})`)
        ttrue(present == 404, `a PUT through a link out of the root must be refused as not found, got ${present}`)
        ttrue(!existsSync(join(OUTSIDE, 'exists', 'zplanted.txt')), 'the PUT must not create a file outside the root')
        ttrue(readdirSync(OUTSIDE).length == 1, 'the PUT must not create a directory outside the root')

        let missing = await put('/tmp/nodir/zplanted.txt')
        ttrue(missing == 404, `a PUT into a directory that does not exist must be refused as not found, got ${missing}`)
        ttrue(!existsSync(join(WEB, 'tmp', 'nodir')), 'the PUT must not create the directory')

        let created = await put('/tmp/zok.txt')
        ttrue(created == 201, `a PUT into an existing directory inside the root must still succeed, got ${created}`)
    } finally {
        await stopServer(server)
        rmSync(HOME, { recursive: true, force: true })
    }
}
