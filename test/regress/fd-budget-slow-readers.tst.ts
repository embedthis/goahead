/*
    Under a 256-descriptor limit, admitted slow readers must receive 200 without exhausting file descriptors.
    After one reader releases its slot, a control request must succeed. Excess connections may be refused.
 */
import { Config } from '@embedthis/ejscript'
import { teq, tget, tskip, ttrue } from '@embedthis/testme'
import { closeSync, ftruncateSync, openSync, rmSync } from 'node:fs'
import { connect, createServer, Socket } from 'node:net'
import { join } from 'node:path'
import { atExit, startServer, stopServer } from '../utils/server'

const TESTDIR = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const HOST = '127.0.0.1'
const DESCRIPTORS = 256                 /* RLIMIT_NOFILE for the private server */
const READERS = 160                     /* more than any limit a 256 descriptor process can serve */
const FIXTURE = join(TESTDIR, 'web', 'fd-budget.bin')
const FIXTURE_SIZE = 64 * 1024 * 1024   /* sparse: larger than the socket buffers so the document stays open */

const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

async function ephemeralPort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, HOST, resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

/*
    One request on its own connection; the status line, or '' when the server closed without answering.
 */
function request(port: number, path: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: HOST, port})
        let data = ''
        socket.setTimeout(5000, () => socket.destroy())
        socket.on('connect', () => socket.write(`GET ${path} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n`))
        socket.on('data', chunk => data += chunk.toString('latin1'))
        socket.on('error', () => {})
        socket.on('close', () => resolve(data.split('\r\n')[0]))
    })
}

/*
    A slow reader: request the document, take the status line and stop reading. Resolves with the status
    line, 'refused' when the server closed the connection without a response, or 'silent' on no answer.
 */
function slowReader(port: number): Promise<{socket: Socket, status: string}> {
    return new Promise(resolve => {
        const socket = connect({host: HOST, port})
        let settled = false
        const settle = (status: string) => {
            if (!settled) {
                settled = true
                resolve({socket, status})
            }
        }
        socket.setTimeout(4000, () => settle('silent'))
        socket.on('connect', () => socket.write(`GET /fd-budget.bin HTTP/1.1\r\nHost: x\r\n\r\n`))
        socket.once('data', chunk => {
            socket.pause()
            settle(chunk.toString('latin1').split('\r\n')[0])
        })
        socket.on('error', () => settle('refused'))
        socket.on('close', () => settle('refused'))
    })
}

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('lowers RLIMIT_NOFILE for a private server; POSIX only')
} else {
    const fd = openSync(FIXTURE, 'w')
    ftruncateSync(fd, FIXTURE_SIZE)
    closeSync(fd)
    atExit(() => rmSync(FIXTURE, {force: true}))

    const port = await ephemeralPort()
    const server = startServer('sh', ['-c', `ulimit -n ${DESCRIPTORS} && exec goahead-test "$@"`, 'goahead-test',
        '--route', ROUTES, '--connections-per-ip', '0', 'web', `http://${HOST}:${port}`],
        {cwd: TESTDIR, stdio: 'ignore'})
    let readers: Socket[] = []
    try {
        let up = false
        for (let i = 0; i < 50 && !up; i++) {
            up = (await request(port, '/index.html')).startsWith('HTTP/1.1 200')
            if (!up) {
                await sleep(100)
            }
        }
        ttrue(up, 'the private server must start')

        const results = await Promise.all(Array.from({length: READERS}, () => slowReader(port)))
        readers = results.map(r => r.socket)
        const admitted = results.filter(r => r.status.startsWith('HTTP/'))
        const served = admitted.filter(r => r.status.startsWith('HTTP/1.1 200'))
        const failed = admitted.filter(r => !r.status.startsWith('HTTP/1.1 200')).map(r => r.status)
        const silent = results.filter(r => r.status == 'silent').length

        ttrue(served.length >= 32,
            `the server must admit and serve a reasonable number of readers, served ${served.length}`)
        teq(silent, 0, 'an admitted reader must be answered or refused, never left without a response')
        teq(failed.length, 0, `every admitted slow reader must be served its document; ${failed.length} of ` +
            `${admitted.length} admitted readers got ${failed.slice(0, 3).join(' | ')}`)

        /*
            Release one slot so the control request can be admitted while other readers keep their documents open.
         */
        served[0].socket.destroy()
        await sleep(300)
        const control = await request(port, '/index.html')
        ttrue(control.startsWith('HTTP/1.1 200'),
            `a document must be served while ${served.length - 1} slow readers hold theirs, got "${control}"`)

        for (const socket of readers) {
            socket.destroy()
        }
        readers = []
        await sleep(500)
        ttrue((await request(port, '/index.html')).startsWith('HTTP/1.1 200'),
            'service must resume once the readers leave')
    } finally {
        for (const socket of readers) {
            socket.destroy()
        }
        await stopServer(server)
    }
}
