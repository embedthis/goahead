/*
    TLS requests must preserve read backpressure and avoid spinning while a response is pending.
    Use a large TLS record to leave decrypted input buffered, then measure CPU time during a slow CGI
    response. Skip the CPU check when ps is unavailable. Also verify flood backpressure and pipelining.
 */
import { Config } from '@embedthis/ejscript'
import { tget, thas, tinfo, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { createServer } from 'node:net'
import { connect } from 'node:tls'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

const CGI_SLEEP = 3
const CPU_LIMIT = 1.0
const CHUNK = Buffer.alloc(65536, 0x41)
const FLOOD_LIMIT = 8000        /* ms before the flood is declared unbounded */
const QUIET = 2500              /* ms without a drain that shows the server has stopped reading */
const CAP = 64 << 20            /* bytes accepted that also show the server has not stopped */
const PAD = 'p'.repeat(1500)    /* a header larger than the server's read size leaves decrypted bytes held */

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('measures a server process CPU time with ps; POSIX only')
} else if (!thas('ME_GOAHEAD_CGI') || tget('ME_SSL') == '0') {
    tskip('needs CGI and TLS')
} else {

async function ephemeralPort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

function open(port: number): Promise<any> {
    return new Promise((resolve, reject) => {
        const sock = connect({host: '127.0.0.1', port, rejectUnauthorized: false})
        sock.on('secureConnect', () => resolve(sock))
        sock.on('error', reject)
    })
}

/*
    Return the status-line count once the target arrives, or -1 on timeout.
 */
function responses(sock: any, count: number, limit: number): Promise<number> {
    return new Promise(resolve => {
        let data = ''
        const timer = setTimeout(() => { sock.off('data', onData); resolve(-1) }, limit)
        const onData = (chunk: Buffer) => {
            data += chunk.toString('latin1')
            const seen = (data.match(/HTTP\/1\.1 \d\d\d/g) || []).length
            if (seen >= count) {
                clearTimeout(timer)
                sock.off('data', onData)
                resolve(seen)
            }
        }
        sock.on('data', onData)
    })
}


let nextPort = Number(process.env.TM_PRIVATE_PORT) || await ephemeralPort()

async function launch(): Promise<{server: any, port: number, sock: any}> {
    const port = nextPort++
    const routes = String(tget('TM_ROUTES') || 'route.txt')
    const server = startServer(join(dir, '../build/bin/goahead-test'), ['--cert', 'pki/self.crt', '--key',
        'pki/self.key', '--route', routes, 'web', `https://127.0.0.1:${port}`], {cwd: dir, stdio: 'ignore'})
    let sock: any = null
    for (let i = 0; i < 50 && !sock; i++) {
        try {
            sock = await open(port)
        } catch {
            await sleep(100)
        }
    }
    ttrue(sock != null, 'private test server started')
    sock.on('error', () => {})
    return {server, port, sock}
}

const CGI_REQUEST = `POST /cgi-bin/cgitest HTTP/1.1\r\nHost: localhost\r\nSWITCHES: -t ${CGI_SLEEP}\r\n` +
    'Content-Length: 5\r\n\r\nhello'

async function flood(sock: any): Promise<{sent: number, stop: string}> {
    let sent = 0
    const start = Date.now()
    while (Date.now() - start < FLOOD_LIMIT && sent < CAP) {
        if (sock.write(CHUNK)) {
            sent += CHUNK.length
            continue
        }
        const drained = await new Promise<boolean>(resolve => {
            const timer = setTimeout(() => { sock.off('drain', onDrain); resolve(false) }, QUIET)
            const onDrain = () => { clearTimeout(timer); resolve(true) }
            sock.once('drain', onDrain)
        })
        sent += CHUNK.length
        if (!drained) {
            return {sent, stop: 'quiet'}
        }
    }
    return {sent, stop: sent >= CAP ? 'cap' : 'time'}
}

/*
    CPU seconds the process has used, or -1 if ps cannot report it
 */
function cpuSeconds(pid: number): number {
    const ps = spawnSync('ps', ['-o', 'time=', '-p', String(pid)], {encoding: 'utf8'})
    const fields = ps.status == 0 ? ps.stdout.trim().split(':').map(Number) : []
    if (fields.length < 2 || fields.some(isNaN)) {
        return -1
    }
    return fields.reduce((total, field) => total * 60 + field, 0)
}

/*
    Extra input during a slow CGI response must not cause a CPU spin.
 */
{
    const {server, sock} = await launch()
    try {
        sock.write('GET /index.html HTTP/1.1\r\nHost: localhost\r\nX-Pad: ' + PAD + '\r\n\r\n')
        ttrue(await responses(sock, 1, 5000) == 1, 'the first request is answered')

        sock.write(CGI_REQUEST)
        await sleep(300)
        sock.write('x')
        const started = performance.now()
        const got = await responses(sock, 1, (CGI_SLEEP + 10) * 1000)
        const waited = performance.now() - started
        sock.destroy()
        ttrue(got == 1, `the CGI request is answered (waited ${waited.toFixed(0)} ms)`)
        ttrue(waited >= CGI_SLEEP * 1000 - 500, `the CGI ran for its sleep (${waited.toFixed(0)} ms)`)

        const seconds = cpuSeconds(server.pid!)
        if (seconds < 0) {
            tinfo('ps is unavailable: the CPU check is skipped')
        } else {
            ttrue(seconds < CPU_LIMIT, `the server must idle while the CGI runs: it used ${seconds.toFixed(2)} s of CPU ` +
                `over a ${CGI_SLEEP} s request`)
        }
    } finally {
        await stopServer(server)
    }
}

/*
    After a large record, bytes sent while a request runs are held by the socket, not read
 */
{
    const {server, sock} = await launch()
    try {
        sock.write('GET /index.html HTTP/1.1\r\nHost: localhost\r\nX-Pad: ' + PAD + '\r\n\r\n')
        ttrue(await responses(sock, 1, 5000) == 1, 'the first request is answered')
        sock.write(CGI_REQUEST.replace(`-t ${CGI_SLEEP}`, '-t 12'))
        await sleep(300)
        const result = await flood(sock)
        sock.destroy()
        ttrue(result.stop == 'quiet',
            `the server must stop reading while a request runs over TLS, but it accepted ${result.sent} bytes ` +
            `and the flood stopped on "${result.stop}"`)
    } finally {
        await stopServer(server)
    }
}

/*
    Pipelined requests, some held decrypted by the TLS layer, are all served on one keep-alive connection
 */
{
    const {server, sock} = await launch()
    try {
        const get = 'GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n'
        sock.write('GET /index.html HTTP/1.1\r\nHost: localhost\r\nX-Pad: ' + PAD + '\r\n\r\n' + get + get)
        ttrue(await responses(sock, 3, 5000) == 3, 'three pipelined requests in one record are answered')
        sock.write(CGI_REQUEST.replace(`-t ${CGI_SLEEP}`, '-t 0') + get + get)
        ttrue(await responses(sock, 3, 5000) == 3, 'a request with a body and two requests pipelined behind it are answered')
        sock.write(get)
        ttrue(await responses(sock, 1, 5000) == 1, 'the connection is still usable')
        sock.destroy()
    } finally {
        await stopServer(server)
    }
}

}
