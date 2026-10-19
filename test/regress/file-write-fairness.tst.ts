/*
    A large static transfer must yield often enough to serve a second connection promptly.
 */
import { tget, tskip, ttrue } from '@embedthis/testme'
import { createServer, connect } from 'node:net'
import { closeSync, existsSync, ftruncateSync, openSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { atExit, startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

const FIXTURE = join(dir, 'web', 'zfairness.bin')
const FIXTURE_URL = '/zfairness.bin'
const FIXTURE_SIZE = 256 * 1024 * 1024
const PROBE_LIMIT_MS = 1000

async function request(port: number, raw: string, timeout = 60000): Promise<string> {
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

const get = (path: string) => `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`

function drain(port: number, path: string): Promise<{bytes: number, ms: number}> {
    return new Promise(resolve => {
        const started = performance.now()
        const socket = connect({host: '127.0.0.1', port})
        let bytes = 0
        let headers = ''
        let inBody = false
        socket.setTimeout(120000, () => socket.destroy())
        socket.on('connect', () => socket.write(get(path)))
        socket.on('data', (chunk: Buffer) => {
            if (inBody) {
                bytes += chunk.length
                return
            }
            headers += chunk.toString('latin1')
            const end = headers.indexOf('\r\n\r\n')
            if (end >= 0) {
                inBody = true
                bytes += headers.length - end - 4
            }
        })
        socket.on('error', () => {})
        socket.on('close', () => resolve({bytes, ms: performance.now() - started}))
    })
}

async function ephemeralPort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

const port = Number(process.env.TM_PRIVATE_PORT) || await ephemeralPort()
const routes = String(tget('TM_ROUTES') || 'route.txt')
const server = startServer(join(dir, '../build/bin/goahead-test'),
    ['--route', routes, 'web', `http://127.0.0.1:${port}`], {cwd: dir, stdio: ['ignore', 'ignore', 'ignore']})
server.on('error', error => ttrue(false, String(error)))

/*
    Failed assertions skip finally blocks, so also remove the fixture on exit.
 */
const fd = openSync(FIXTURE, 'w')
ftruncateSync(fd, FIXTURE_SIZE)
closeSync(fd)
atExit(() => rmSync(FIXTURE, {force: true}))

try {
    let ready = false
    for (let i = 0; i < 50; i++) {
        if ((await request(port, get('/index.html'), 3000)).includes(' 200 ')) {
            ready = true
            break
        }
        await sleep(50)
    }
    ttrue(ready, 'private test server started')

    let transfer: {bytes: number, ms: number} | null = null
    const streaming = drain(port, FIXTURE_URL).then(result => transfer = result)
    await sleep(100)

    /*
        Probe latency on a second connection throughout the transfer.
     */
    const latencies: number[] = []
    let unserved = 0
    while (transfer == null) {
        const started = performance.now()
        const raw = await request(port, get('/index.html'))
        latencies.push(performance.now() - started)
        if (!raw.includes(' 200 ')) {
            unserved++
        }
    }
    await streaming
    ttrue(unserved == 0, `${unserved} probe requests were not served`)

    ttrue(transfer!.bytes == FIXTURE_SIZE, `the transfer must deliver the whole document, got ${transfer!.bytes}`)
    if (transfer!.ms < 500) {
        tskip(`the ${FIXTURE_SIZE} byte transfer took only ${transfer!.ms.toFixed(0)} ms, too fast to judge fairness`)
    } else {
        const slowest = Math.max(...latencies)
        ttrue(slowest < PROBE_LIMIT_MS && slowest < transfer!.ms / 2,
              `a request on another connection waited ${slowest.toFixed(0)} ms during a ` +
              `${transfer!.ms.toFixed(0)} ms transfer`)
        ttrue(latencies.length >= 2, `probes must be answered throughout a ${transfer!.ms.toFixed(0)} ms transfer, ` +
              `got ${latencies.length}`)
    }
} finally {
    await stopServer(server)
    if (existsSync(FIXTURE)) {
        rmSync(FIXTURE, {force: true})
    }
}
