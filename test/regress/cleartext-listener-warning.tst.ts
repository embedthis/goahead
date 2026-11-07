/*
    cleartext-listener-warning.tst - A plain HTTP listener that can carry an authenticated route is warned about

    Basic and Digest credentials, form passwords and session cookies cross the network unencrypted when an
    authenticated route is reachable over plain HTTP. A build without TLS already warned about every authenticated
    route; a build with TLS that opened only a plain listener served the same routes in silence, although the
    deployment guide asks for authentication over HTTPS only. The warning now follows the listener, not the
    build. Starts private servers on a scratch home and reads their startup output.
 */
import { tcontains, thas, tskip, ttrue } from '@embedthis/testme'
import { chmodSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = mkdtempSync(join(tmpdir(), 'goahead-cleartext-'))
const PORT = process.env.CLEARTEXT_PORT || '18972'
const ENDPOINT = 'http://127.0.0.1:' + PORT
const AUTH = 'role name=administrator abilities=manage\nuser name=alice password=x roles=administrator\n'
const PROTECTED = 'route uri=/auth/basic/ auth=basic abilities=manage dir=private\n'
const CATCHALL = 'route uri=/\n'
const WARNING = 'Security warning: route "/auth/basic/" uses basic authentication and ' + ENDPOINT +
    ' is plain HTTP, so its credentials and session cookies cross the network unencrypted'

async function startup(routes: string, args: string[] = []): Promise<{ up: boolean, log: string }> {
    writeFileSync(join(HOME, 'route.txt'), routes)
    let log = ''
    let server = startServer('goahead', [...args, '--route', 'route.txt', '--auth', 'auth.txt', 'web', ENDPOINT],
        { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)
    let up = false
    try {
        for (let i = 0; i < 50 && !up && server.exitCode == null; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                await fetch(`${ENDPOINT}/index.html`, { redirect: 'manual' })
                up = true
            } catch {}
        }
    } finally {
        await stopServer(server)
    }
    return { up, log }
}

if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_LOGGING')) {
    tskip('The warning needs an authenticated route to load and logging to be seen')
} else {
    mkdirSync(join(HOME, 'web'), { recursive: true })
    mkdirSync(join(HOME, 'private', 'auth', 'basic'), { recursive: true })
    writeFileSync(join(HOME, 'web', 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(HOME, 'private', 'auth', 'basic', 'secret.txt'), 'secret\n')
    writeFileSync(join(HOME, 'auth.txt'), AUTH)
    chmodSync(join(HOME, 'auth.txt'), 0o600)
    try {
        //  The shipped per-directory example, on a plain listener, with no redirect ahead of it
        let result = await startup(PROTECTED + CATCHALL)
        ttrue(result.up, `the server must start: ${result.log}`)
        tcontains(result.log, WARNING, 'an authenticated route on a plain HTTP listener must be warned about')
        tcontains(result.log, 'protocol=https', 'and the warning must say how to serve it over HTTPS only')

        //  Controls: the shipped http-to-https redirect ahead of it, a route restricted to https, a trusted proxy
        result = await startup('route uri=/ protocol=http redirect=*@https handler=redirect\n' + PROTECTED + CATCHALL)
        ttrue(result.up, `the server must start: ${result.log}`)
        ttrue(!result.log.includes('plain HTTP'), `a route behind an http-to-https redirect must not be warned about: ${result.log}`)

        result = await startup('route uri=/auth/basic/ protocol=https auth=basic abilities=manage dir=private\n' + CATCHALL)
        ttrue(result.up, `the server must start: ${result.log}`)
        ttrue(!result.log.includes('plain HTTP'), `a protocol=https route must not be warned about: ${result.log}`)

        result = await startup(PROTECTED + CATCHALL, ['--proxy', '127.0.0.1'])
        ttrue(result.up, `the server must start: ${result.log}`)
        ttrue(!result.log.includes('plain HTTP'), `behind a trusted proxy the plain listener must not be warned about: ${result.log}`)
    } finally {
        rmSync(HOME, { recursive: true, force: true })
    }
}
