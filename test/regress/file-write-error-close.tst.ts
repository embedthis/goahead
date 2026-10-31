/*
    Reset a large keep-alive transfer mid-body. The trace must show the request closing without reuse.
 */
import { tget, thas, ttrue } from '@embedthis/testme'
import { closeSync, existsSync, ftruncateSync, mkdtempSync, openSync, readFileSync, rmSync } from 'node:fs'
import { connect, createServer } from 'node:net'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { atExit, startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

const FIXTURE = join(dir, 'web', 'write-error.bin')
const FIXTURE_URL = '/write-error.bin'
const FIXTURE_SIZE = 1024 * 1024 * 1024                 /* sparse: the transfer must outlive the reset */
const ABORT_AFTER = 256 * 1024

async function ephemeralPort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

/*
    One request on a connection the client closes. The server must not keep this connection alive.
 */
async function request(port: number, raw: string, timeout = 5000): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let data = ''
        socket.setTimeout(timeout, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

/*
    Start a keep-alive transfer and reset the connection once part of the body has arrived, so the server's
    next write fails hard rather than with EWOULDBLOCK.
 */
function abortedTransfer(port: number): Promise<number> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let received = 0
        let done = false
        socket.setTimeout(10000, () => socket.destroy())
        socket.on('connect', () => socket.write(`GET ${FIXTURE_URL} HTTP/1.1\r\nHost: localhost\r\n\r\n`))
        socket.on('data', (chunk: Buffer) => {
            received += chunk.length
            if (!done && received >= ABORT_AFTER) {
                done = true
                if (typeof (socket as any).resetAndDestroy == 'function') {
                    (socket as any).resetAndDestroy()
                } else {
                    socket.destroy()
                }
            }
        })
        socket.on('error', () => {})
        socket.on('close', () => resolve(received))
    })
}

const port = Number(process.env.TM_PRIVATE_PORT) || await ephemeralPort()
const routes = String(tget('TM_ROUTES') || 'route.txt')
const scratch = mkdtempSync(join(tmpdir(), 'goahead-write-error-'))
const logFile = join(scratch, 'server.log')
const server = startServer(join(dir, '../build/bin/goahead-test'),
    ['--route', routes, '--log', `${logFile}:5`, 'web', `http://127.0.0.1:${port}`],
    {cwd: dir, stdio: ['ignore', 'ignore', 'ignore']})
server.on('error', error => ttrue(false, String(error)))

const fd = openSync(FIXTURE, 'w')
ftruncateSync(fd, FIXTURE_SIZE)
closeSync(fd)
atExit(() => {
    rmSync(FIXTURE, {force: true})
    rmSync(scratch, {recursive: true, force: true})
})

try {
    let ready = false
    for (let i = 0; i < 50 && !ready; i++) {
        const reply = await request(port, 'GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n', 3000)
        ready = reply.includes(' 200 ')
        if (!ready) {
            await sleep(50)
        }
    }
    ttrue(ready, 'private test server started')

    const received = await abortedTransfer(port)
    ttrue(received >= ABORT_AFTER && received < FIXTURE_SIZE,
          `the transfer must be reset part way, received ${received} of ${FIXTURE_SIZE}`)
    await sleep(500)

    const after = await request(port, 'GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
    ttrue(after.includes(' 200 '), 'the server must still answer after a failed transfer')
    await stopServer(server)

    //  How the connection ended is read from the trace, which a build without logging or tracing does not write
    if (thas('ME_GOAHEAD_LOGGING') && thas('ME_GOAHEAD_TRACING')) {
        const log = readFileSync(logFile, 'utf8')
        const start = log.indexOf(`GET ${FIXTURE_URL}`)
        ttrue(start >= 0, 'the trace must record the aborted request')
        const trace = log.slice(start)
        ttrue(trace.includes('Close connection'), 'the connection the server could not write to must be closed')
        ttrue(!trace.includes('Keep connection alive'),
              'a connection whose response body could not be written must not be kept alive for the next request')
    }
} finally {
    await stopServer(server)
    if (existsSync(FIXTURE)) {
        rmSync(FIXTURE, {force: true})
    }
    rmSync(scratch, {recursive: true, force: true})
}
