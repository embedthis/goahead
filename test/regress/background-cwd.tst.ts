/*
    background-cwd.tst - goahead --background keeps its working directory
 */
import { tcontains, tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { readlinkSync, realpathSync } from 'node:fs'
import { join } from 'node:path'

const HOME = join(import.meta.dir, '..')
//  The route table assembled for this build's feature set
const ROUTES = String(tget('TM_ROUTES'))
//  A build without authentication has no --auth option
const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []
const ENDPOINT = '127.0.0.1:18985'
const PATTERN = 'goahead --background.*18985'

if (process.platform != 'linux') {
    tskip('--background is not built on macOS or Windows, and /proc is Linux only')
} else {
    let result = spawnSync('goahead', ['--background', '--no-strict', '--route', ROUTES, ...AUTH, 'web', ENDPOINT],
        { cwd: HOME, encoding: 'utf8', timeout: 10000 })
    try {
        ttrue(result.status == 0, `goahead --background must start, got status ${result.status}: ${result.stderr}`)
        tcontains(result.stdout + result.stderr, '--background discards log output',
            'logging to stderr must be warned about before detaching')

        let status = 0
        for (let i = 0; i < 50 && status == 0; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                status = (await fetch(`http://${ENDPOINT}/index.html`)).status
            } catch {}
        }
        ttrue(status == 200, `the relative documents directory must be served, got ${status}`)

        let pid = spawnSync('pgrep', ['-n', '-f', PATTERN], { encoding: 'utf8' }).stdout.trim()
        ttrue(pid != '', 'the daemon must be running')
        if (pid) {
            ttrue(readlinkSync(`/proc/${pid}/cwd`) == realpathSync(HOME),
                'the daemon must keep the directory it was started in')
        }
    } finally {
        spawnSync('pkill', ['-f', PATTERN])
    }
}
