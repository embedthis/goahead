/*
    server-orphan-child.ts - Start a server with utils/server.ts, then end as server-orphan.tst.ts directs

    usage: bun server-orphan-child.ts MODE PORT
    MODE is assert (fail an assertion), throw (throw an exception) or wait (wait to be signalled).
 */
import { tget, ttrue } from '@embedthis/testme'
import { existsSync } from 'node:fs'
import { connect } from 'node:net'
import { join } from 'node:path'
import { SERVERS, startServer } from '../utils/server'

const [mode, port] = process.argv.slice(2)
const TESTDIR = join(import.meta.dir, '..')
const GOAHEAD = join(TESTDIR, '..', 'build', 'bin', 'goahead')

function listening(): Promise<boolean> {
    return new Promise(resolve => {
        let socket = connect(Number(port), '127.0.0.1')
        socket.on('connect', () => { socket.destroy(); resolve(true) })
        socket.on('error', () => resolve(false))
    })
}

let server = startServer(GOAHEAD, ['--no-strict', '--route', String(tget('TM_ROUTES')), 'web', `127.0.0.1:${port}`],
    { cwd: TESTDIR, stdio: 'ignore' })
let up = false
for (let i = 0; i < 50 && !up; i++) {
    await new Promise(r => setTimeout(r, 100))
    up = await listening()
}
console.log(`ready ${up ? server.pid : 0} ${existsSync(join(SERVERS, String(server.pid))) ? 'recorded' : 'unrecorded'}`)

if (mode == 'assert') {
    ttrue(false, 'deliberate failure')
} else if (mode == 'throw') {
    throw new Error('deliberate exception')
} else {
    await new Promise(r => setTimeout(r, 60000))
}
