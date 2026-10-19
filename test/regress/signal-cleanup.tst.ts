/*
    signal-cleanup.tst - SIGINT, SIGHUP and SIGTERM stop goahead through websClose
 */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { readdirSync } from 'node:fs'
import { join } from 'node:path'
import { startServer } from '../utils/server'

const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []
const ENDPOINT = '127.0.0.1:18983'
const TEMP = '/tmp'

function staged(): Set<string> {
    return new Set(readdirSync(TEMP).filter(name => /^cgi-.*\.tmp$/.test(name)))
}

function sleep(ms: number) {
    return new Promise(r => setTimeout(r, ms))
}

async function stopWith(signal: NodeJS.Signals) {
    let before = staged()
    let log = ''
    let server = startServer('goahead', ['--no-strict', '--route', ROUTES, ...AUTH, 'web', ENDPOINT],
        { cwd: HOME })
    server.stdout.on('data', (data) => log += data)
    server.stderr.on('data', (data) => log += data)
    let exited = new Promise<{ code: number | null, signal: string | null }>(r =>
        server.on('exit', (code, signal) => r({ code, signal })))
    let up = false
    for (let i = 0; i < 50 && !up; i++) {
        await sleep(100)
        try {
            await fetch(`http://${ENDPOINT}/index.html`)
            up = true
        } catch {}
    }
    ttrue(up, `goahead must start before ${signal}` + (up ? '' : `: ${log}`))


    let request: Promise<any> = Promise.resolve(null)
    if (thas('ME_GOAHEAD_CGI')) {
        request = fetch(`http://${ENDPOINT}/cgi-bin/cgitest`, { headers: { SWITCHES: '-t 3' } }).catch(() => null)
        let inFlight = false
        for (let i = 0; i < 40 && !inFlight; i++) {
            await sleep(50)
            inFlight = [...staged()].some(name => !before.has(name))
        }
        ttrue(inFlight, `the CGI request must be staged before ${signal}`)
    }

    server.kill(signal)
    let status = await exited
    await request
    ttrue(status.code == 0, `${signal} must stop goahead with status 0, got ${status.code} ${status.signal || ''}`)
    let left = [...staged()].filter(name => !before.has(name))
    ttrue(left.length == 0, `${signal} must remove the CGI staging files, left ${left.join(', ')}`)
}

if (process.platform == 'win32') {
    tskip('POSIX signals')
} else {
    for (let signal of ['SIGINT', 'SIGHUP', 'SIGTERM'] as NodeJS.Signals[]) {
        await stopWith(signal)
    }
}
