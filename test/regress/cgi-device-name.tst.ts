/*
    Refuse Windows CGI device names before lookup. Check the refusal trace on Windows and 404 everywhere.
 */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { createServer, connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))
const WINDOWS = process.platform == 'win32'

async function request(port: number, raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let data = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

const get = (path: string) => `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
    process.exit(0)
}
let port = Number(tget('TM_PRIVATE_PORT') || 0)
if (!port) {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
}

const server = startServer(join(dir, '../build/bin/goahead' + (WINDOWS ? '.exe' : '')),
    ['--no-strict', '--log', 'stdout:2', '--route', String(tget('TM_ROUTES')), 'web', `127.0.0.1:${port}`],
    {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
let output = ''
server.stdout.on('data', data => output += data.toString())
server.stderr.on('data', data => output += data.toString())
try {
    let ready = false
    for (let i = 0; i < 50 && !ready; i++) {
        ready = (await request(port, get('/index.html'))).includes(' 200 ')
        if (!ready) await sleep(50)
    }
    ttrue(ready, 'private test server started')

    for (let name of ['COM1', 'nul.txt', 'LPT1/extra', 'con']) {
        let mark = output.length
        let response = await request(port, get(`/cgi-bin/${name}`))
        ttrue(response.startsWith('HTTP/1.1 404 '), `/cgi-bin/${name} must be refused, got ${response.split('\r\n')[0]}`)
        if (WINDOWS && thas('ME_GOAHEAD_LOGGING') && thas('ME_GOAHEAD_TRACING')) {
            await sleep(100)
            ttrue(output.slice(mark).includes('reserved device name'),
                  `/cgi-bin/${name} must be refused as a device name before the program lookup`)
        }
    }
    let response = await request(port, get('/cgi-bin/cgitest'))
    ttrue(response.startsWith('HTTP/1.1 200 '), `an ordinary CGI program must still run, got ${response.split('\r\n')[0]}`)
} finally {
    await stopServer(server)
}
