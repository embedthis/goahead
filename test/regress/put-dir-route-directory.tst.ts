/*
    PUT onto a directory under a dir= route must return 409.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'put-dir-route-directory.dir')
const WEB = join(HOME, 'web')
const STORE = join(HOME, 'store')
const ENDPOINT = '127.0.0.1:' + (process.env.PUT_DIR_ROUTE_PORT || '18997')
const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []

async function put(path: string): Promise<number> {
    let response = await fetch(`http://${ENDPOINT}${path}`, { method: 'PUT', body: 'planted' })
    await response.text()
    return response.status
}

if (process.platform == 'win32') {
    tskip('The goahead program is driven with POSIX paths here')
} else {
    rmSync(HOME, { recursive: true, force: true })
    mkdirSync(WEB, { recursive: true })
    //  A dir= route keeps the request path: /up/subdir is STORE/up/subdir
    mkdirSync(join(STORE, 'up', 'subdir'), { recursive: true })
    writeFileSync(join(WEB, 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(HOME, 'route.txt'), `route uri=/up/ dir=${STORE} methods=PUT|DELETE\nroute uri=/\n`)
    writeFileSync(join(HOME, 'auth.txt'), '')

    let log = ''
    let server = startServer('goahead', ['--no-strict', '--route', 'route.txt', ...AUTH, 'web', ENDPOINT],
        { cwd: HOME })
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

        let status = await put('/up/subdir')
        ttrue(status == 409, `a PUT onto a directory under a dir= route must be refused with 409, got ${status}`)

        status = await put('/up/zok.txt')
        ttrue(status == 201, `a PUT of a file under a dir= route must still succeed, got ${status}`)
        ttrue(existsSync(join(STORE, 'up', 'zok.txt')) && readFileSync(join(STORE, 'up', 'zok.txt'), 'utf8') == 'planted',
            'the file must be written in the route directory')
        ttrue(!existsSync(join(WEB, 'up')), 'nothing must be written under the documents')
    } finally {
        await stopServer(server)
        rmSync(HOME, { recursive: true, force: true })
    }
}
