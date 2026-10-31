/*
    log-file-size-limit.tst - a log write refused by the process file size limit fails the write, not the server
 */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { rmSync, statSync } from 'node:fs'
import { createServer } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []
//  The file size limit in ulimit -f blocks, whose size the shell chooses (1024 bytes in bash, 512 in dash)
const LIMIT_BLOCKS = 16
const LOG = join(HOME, 'zlog-size-limit.log')

function sleep(ms: number) {
    return new Promise(r => setTimeout(r, ms))
}

async function ephemeralPort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

function logSize(): number {
    try {
        return statSync(LOG).size
    } catch {
        return -1
    }
}

if (process.platform == 'win32') {
    tskip('Resource limits are POSIX')
} else if (!thas('ME_GOAHEAD_LOGGING')) {
    tskip('Needs ME_GOAHEAD_LOGGING')
} else {
    /*
        Measure the limit in bytes: a write past it is cut short, leaving the file at the limit
     */
    spawnSync('sh', ['-c', `ulimit -f ${LIMIT_BLOCKS} && trap '' XFSZ && head -c 65536 /dev/zero > ${LOG}`])
    const LIMIT = logSize()
    rmSync(LOG, { force: true })
    const PORT = await ephemeralPort()
    let output = ''
    let server = startServer('sh', ['-c', `ulimit -f ${LIMIT_BLOCKS} && exec goahead-test --log ${LOG}:2 ` +
        `--route ${ROUTES} ${AUTH.join(' ')} web 127.0.0.1:${PORT}`], { cwd: HOME })
    server.stderr!.on('data', (data) => output += data)
    server.stdout!.on('data', (data) => output += data)
    let exited = new Promise<{ code: number | null, signal: string | null }>(r =>
        server.on('exit', (code, signal) => r({ code, signal })))
    let up = false
    for (let i = 0; i < 50 && !up; i++) {
        await sleep(100)
        try {
            up = (await fetch(`http://127.0.0.1:${PORT}/index.html`)).status == 200
        } catch {}
    }
    ttrue(up, `goahead-test must start under a file size limit of ${LIMIT} bytes: ${output}`)

    /*
        Every request is traced at level 2, so the log reaches the limit after a few hundred requests
     */
    let served = 0
    let failure = ''
    for (let i = 0; i < 600 && logSize() < LIMIT + 4096 && !failure; i++) {
        try {
            let response = await fetch(`http://127.0.0.1:${PORT}/index.html`)
            if (response.status == 200) {
                served++
            } else {
                failure = `status ${response.status} on request ${i + 1}`
            }
        } catch (err: any) {
            failure = `${err.code || err.message} on request ${i + 1}`
        }
    }
    ttrue(LIMIT > 0 && logSize() >= LIMIT,
        `the log must reach the file size limit, size ${logSize()} after ${served} requests`)
    let status = await Promise.race([exited, sleep(500).then(() => null)])
    ttrue(status == null, `the server must survive a log write refused by the file size limit, but it exited ` +
        `${status ? `(code ${status.code}, signal ${status.signal})` : ''} after ${served} requests`)
    ttrue(failure == '', `requests must be served once the log can no longer grow, got ${failure}`)
    let after = 0
    try {
        after = (await fetch(`http://127.0.0.1:${PORT}/index.html`)).status
    } catch {}
    ttrue(after == 200, `the server must keep serving after the log is full, got ${after}`)
    await stopServer(server)
    rmSync(LOG, { force: true })
}
