/*
    anonymous-write-route-warning.tst - A route that accepts writes without authentication is warned about

    A route that admits PUT or DELETE through the file handler, or parses multipart uploads, writes request data
    to the server. The shipped route.txt example for such a route carried no auth=, and the startup lint, which
    names an anonymous writable route in a build without authentication, said nothing about one in a build with
    it. Starts private servers on a scratch home and reads their startup output.
 */
import { tcontains, thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

//  Outside the tree, so a run that exits early leaves no password file for the shipped-file checks to find
const HOME = mkdtempSync(join(tmpdir(), 'goahead-write-warning-'))
const SHIPPED = join(import.meta.dir, '..', '..', 'src', 'route.txt')
const ENDPOINT = '127.0.0.1:' + (process.env.WRITE_WARNING_PORT || '18992')
const AUTH = 'role name=administrator abilities=manage\nuser name=alice password=x roles=administrator\n'

async function startup(routes: string): Promise<{ up: boolean, log: string }> {
    writeFileSync(join(HOME, 'route.txt'), routes)
    let log = ''
    let server = startServer('goahead', ['--route', 'route.txt', '--auth', 'auth.txt', 'web', ENDPOINT], { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)
    let up = false
    try {
        for (let i = 0; i < 50 && !up && server.exitCode == null; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                await fetch(`http://${ENDPOINT}/index.html`, { redirect: 'manual' })
                up = true
            } catch {}
        }
    } finally {
        await stopServer(server)
    }
    return { up, log }
}

if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_LOGGING')) {
    tskip('The warning needs authentication to be worth giving and logging to be seen')
} else {
    rmSync(HOME, { recursive: true, force: true })
    mkdirSync(join(HOME, 'web', 'put'), { recursive: true })
    mkdirSync(join(HOME, 'private'), { recursive: true })
    mkdirSync(join(HOME, 'tmp'), { recursive: true })
    writeFileSync(join(HOME, 'web', 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(HOME, 'auth.txt'), AUTH)
    try {
        //  The shipped example, placed as route.txt says, beside an authenticated route and before the catch-all
        let result = await startup('route uri=/put/ methods=PUT|DELETE\n' +
            'route uri=/auth/basic/ auth=basic abilities=manage dir=private\nroute uri=/\n')
        ttrue(result.up, `the table must start: ${result.log}`)
        tcontains(result.log, 'Security warning: route "/put/"',
            'an anonymous PUT and DELETE route must be warned about at startup')
        tcontains(result.log, 'PUT or DELETE without authentication', 'and the warning must say what it accepts')

        if (thas('ME_GOAHEAD_UPLOAD')) {
            result = await startup('route uri=/action/upload methods=POST upload=true handler=action\nroute uri=/\n')
            ttrue(result.up, `the upload table must start: ${result.log}`)
            tcontains(result.log, 'Security warning: route "/action/upload"',
                'an anonymous upload route must be warned about at startup')
            tcontains(result.log, 'file uploads without authentication', 'and the warning must say what it accepts')
        }

        //  Controls: a writable route with its own auth=, and one after a login gate that covers it, are not warned about
        result = await startup('route uri=/put/ methods=PUT|DELETE auth=basic abilities=manage dir=private\nroute uri=/\n')
        ttrue(result.up, `a self-authenticated writable route must start: ${result.log}`)
        ttrue(!result.log.includes('Security warning: route "/put/"'),
            `a writable route with auth= must not be warned about: ${result.log}`)

        result = await startup('route uri=/ auth=form handler=continue redirect=401@/login.html\n' +
            'route uri=/put/ methods=PUT|DELETE\nroute uri=/\n')
        ttrue(result.up, `a gated writable route must start: ${result.log}`)
        ttrue(!result.log.includes('Security warning: route "/put/"'),
            `a writable route after a gate that covers it must not be warned about: ${result.log}`)

        //  The shipped example itself must show the authenticated form
        let example = readFileSync(SHIPPED, 'utf8').split('\n').find((line) => /^#\s+route uri=\/put\/ /.test(line))
        ttrue(example != null, 'src/route.txt must still carry the PUT and DELETE example')
        ttrue(example != null && example.includes('auth='), `the shipped PUT and DELETE example must require authentication: ${example}`)
    } finally {
        rmSync(HOME, { recursive: true, force: true })
    }
}
