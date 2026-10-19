/*
    Refuse startup when a loaded route or credential file is inside a served directory.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'config-file-in-documents.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.CONFIG_FILE_PORT || '18990')
const USERS = 'role name=administrator abilities=manage\nuser name=alice password=x roles=administrator\n'

async function startup(args: string[]): Promise<{ up: boolean, log: string, fetch: (path: string) => Promise<number> }> {
    let log = ''
    let server = startServer('goahead', [...args, 'web', ENDPOINT], { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)
    let up = false
    for (let i = 0; i < 50 && !up && server.exitCode == null; i++) {
        await new Promise(r => setTimeout(r, 100))
        try {
            await fetch(`http://${ENDPOINT}/index.html`, { redirect: 'manual' })
            up = true
        } catch {}
    }
    return {
        up, log,
        fetch: async (path: string) => {
            let status = 0
            try {
                if (up) {
                    let response = await fetch(`http://${ENDPOINT}${path}`, { redirect: 'manual' })
                    await response.text()
                    status = response.status
                }
            } finally {
                await stopServer(server)
            }
            return status
        },
    }
}

async function check(name: string, args: string[], path: string) {
    let result = await startup(args)
    let status = await result.fetch(path)
    ttrue(!result.up || status != 200,
        `${name}: the server must refuse to start or must not serve ${path}, got ${status}: ${result.log}`)
    if (!result.up && thas('ME_GOAHEAD_LOGGING')) {
        ttrue(result.log.includes(path.slice(1)) && /inside the served directory/.test(result.log),
            `${name}: the refusal must name the file and say it is inside a served directory: ${result.log}`)
    }
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('The credential file is only loaded by a build with authentication')
} else {
    rmSync(HOME, { recursive: true, force: true })
    mkdirSync(join(HOME, 'web'), { recursive: true })
    mkdirSync(join(HOME, 'private'), { recursive: true })
    mkdirSync(join(HOME, 'tmp'), { recursive: true })
    writeFileSync(join(HOME, 'web', 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(HOME, 'route.txt'), 'route uri=/\n')
    writeFileSync(join(HOME, 'auth.txt'), USERS)
    writeFileSync(join(HOME, 'web', 'auth.txt'), USERS)
    writeFileSync(join(HOME, 'web', 'route.txt'), 'route uri=/\n')
    writeFileSync(join(HOME, 'private', 'auth.txt'), USERS)
    writeFileSync(join(HOME, 'private', 'secret.txt'), 'PRIVATE\n')
    try {
        await check('credential file in the documents', ['--route', 'route.txt', '--auth', 'web/auth.txt'], '/auth.txt')
        await check('route file in the documents', ['--route', 'web/route.txt', '--auth', 'auth.txt'], '/route.txt')

        //  A credential file inside a route's own dir=, served by that route to anyone holding the ability
        writeFileSync(join(HOME, 'dir-route.txt'), 'route uri=/private/ auth=basic abilities=manage dir=private\nroute uri=/\n')
        let result = await startup(['--route', 'dir-route.txt', '--auth', 'private/auth.txt'])
        let status = await result.fetch('/private/auth.txt')
        ttrue(!result.up || status == 401,
            `credential file in a route dir=: the server must refuse to start or the route must still challenge, got ${status}: ${result.log}`)
        ttrue(!result.up, `credential file in a route dir=: the server must refuse to start: ${result.log}`)

        //  Control: the files outside the served directories load, and the documents are served
        rmSync(join(HOME, 'web', 'auth.txt'))
        rmSync(join(HOME, 'web', 'route.txt'))
        result = await startup(['--route', 'route.txt', '--auth', 'auth.txt'])
        ttrue(result.up, `the files outside the documents must load: ${result.log}`)
        status = await result.fetch('/auth.txt')
        ttrue(status == 404, `nothing named auth.txt is in the documents, got ${status}`)
    } finally {
        rmSync(HOME, { recursive: true, force: true })
    }
}
