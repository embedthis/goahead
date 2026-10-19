/*
    privilege-cli.tst - goahead --user and --group change the account after listening, or refuse to start
 */
import { tcontains, tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { userInfo } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const ENDPOINT = '127.0.0.1:18978'
const ROOT_WARNING = 'running as root'
const LOGGING = thas('ME_GOAHEAD_LOGGING')
const CGI = thas('ME_GOAHEAD_CGI')

async function serves(args: string[]): Promise<{ up: boolean, log: string, cgi: string }> {
    let log = '', cgi = ''
    let server = startServer('goahead', ['--no-strict', '--route', ROUTES, '-v', ...args, 'web', ENDPOINT], { cwd: HOME })
    server.stderr.on('data', (data) => log += data)
    server.stdout.on('data', (data) => log += data)
    let up = false
    try {
        for (let i = 0; i < 50 && !up; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                await fetch(`http://${ENDPOINT}/`)
                up = true
            } catch {}
        }
        if (up && CGI) {
            cgi = await (await fetch(`http://${ENDPOINT}/cgi-bin/cgitest`, { headers: { SWITCHES: '-S' } })).text()
        }
    } finally {
        await stopServer(server)
    }
    return { up, log, cgi }
}

function refused(args: string[], expect: string, what: string) {
    let result = spawnSync('goahead', ['--no-strict', '--route', ROUTES, ...args, 'web', ENDPOINT],
        { cwd: HOME, encoding: 'utf8', timeout: 10000 })
    ttrue(result.status != 0 && result.status != null, `${what} must stop goahead, got status ${result.status}`)
    if (LOGGING) {
        tcontains(result.stdout + result.stderr, expect, `${what} must be reported`)
    }
}

if (process.platform == 'win32') {
    tskip('--user and --group are Unix only')
} else {
    refused(['--user', 'goahead-no-such-user'], 'Cannot find user', 'an unknown user')
    refused(['--group', 'goahead-no-such-group'], 'Cannot find group', 'an unknown group')

    if (process.getuid!() != 0) {
        let result = await serves(['--user', userInfo().username])
        ttrue(result.up, '--user naming the current user must start')
        ttrue(!result.log.includes(ROOT_WARNING), 'a server not running as root must not warn about root')
        let uid = result.cgi.match(/UID=(\d+)/)
        ttrue(!CGI || (uid != null && uid[1] == String(process.getuid!())), 'a CGI program runs as the server\'s user')
        refused(['--user', 'root'], 'not running as root', 'a change of user without root')
    } else {
        let result = await serves(['--user', 'nobody'])
        ttrue(result.up, 'root with --user nobody must start')
        ttrue(!result.log.includes(ROOT_WARNING), 'a server that dropped root must not warn about root')
        let nobody = spawnSync('id', ['-u', 'nobody'], { encoding: 'utf8' }).stdout.trim()
        let uid = result.cgi.match(/UID=(\d+)/)
        ttrue(!CGI || (uid != null && uid[1] == nobody),
              `a CGI program must run as nobody (${nobody}), got ${uid ? uid[1] : 'no report: ' + result.cgi.slice(0, 200)}`)

        result = await serves([])
        ttrue(result.up, 'root without --user still starts')
        if (LOGGING) {
            tcontains(result.log, ROOT_WARNING, 'and warns that it is running as root')
        }
    }
}
