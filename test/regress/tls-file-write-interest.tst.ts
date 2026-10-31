/*
    Fetch more than one TLS write slice, sending each request immediately after the handshake.
    Every response must arrive in full.
 */
import { tget, tskip, ttrue } from '@embedthis/testme'
import { statSync } from 'node:fs'
import { createServer } from 'node:net'
import { connect } from 'node:tls'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

const DOCUMENT = '/big.txt'                             /* 117016 bytes: more than one 64 KB write slice */
const SIZE = statSync(join(dir, 'web', 'big.txt')).size
const ATTEMPTS = 5
const LIMIT_MS = 8000                                   /* well inside the server's 60 s idle timeout */

if (tget('ME_SSL') == '0') {
    tskip('needs TLS')
} else {
    async function ephemeralPort(): Promise<number> {
        const probe = createServer()
        await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
        const port = (probe.address() as any).port
        await new Promise<void>(resolve => probe.close(() => resolve()))
        return port
    }

    /*
        Fetch the document, sending the request as soon as the handshake completes so that the request
        follows the client Finished on the wire. Resolve with the bytes received and whether the response stalled.
     */
    function fetch(port: number): Promise<{status: string, received: number, declared: number, stalled: boolean}> {
        return new Promise((resolve, reject) => {
            const sock = connect({host: '127.0.0.1', port, rejectUnauthorized: false})
            let data = Buffer.alloc(0)
            let headerEnd = -1
            let status = ''
            let declared = -1
            let settled = false
            const settle = (stalled: boolean) => {
                if (settled) {
                    return
                }
                settled = true
                clearTimeout(timer)
                sock.destroy()
                const received = headerEnd < 0 ? 0 : data.length - headerEnd - 4
                resolve({status, received, declared, stalled})
            }
            const timer = setTimeout(() => settle(true), LIMIT_MS)
            sock.on('secureConnect', () => {
                sock.write(`GET ${DOCUMENT} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`)
            })
            sock.on('data', (chunk: Buffer) => {
                data = Buffer.concat([data, chunk])
                if (headerEnd < 0) {
                    headerEnd = data.indexOf('\r\n\r\n')
                    if (headerEnd >= 0) {
                        const head = data.subarray(0, headerEnd).toString('latin1')
                        status = head.split('\r\n')[0]
                        const length = head.match(/^Content-Length:\s*(\d+)/im)
                        declared = length ? Number(length[1]) : -1
                    }
                }
                if (headerEnd >= 0 && declared >= 0 && data.length - headerEnd - 4 >= declared) {
                    settle(false)
                }
            })
            sock.on('close', () => settle(false))
            sock.on('error', (error: Error) => {
                if (!settled) {
                    settled = true
                    clearTimeout(timer)
                    reject(error)
                }
            })
        })
    }

    const port = Number(process.env.TM_PRIVATE_PORT) || await ephemeralPort()
    const routes = String(tget('TM_ROUTES') || 'route.txt')
    const server = startServer(join(dir, '../build/bin/goahead-test'), ['--cert', 'pki/self.crt', '--key',
        'pki/self.key', '--route', routes, 'web', `https://127.0.0.1:${port}`], {cwd: dir, stdio: 'ignore'})
    server.on('error', error => ttrue(false, String(error)))

    try {
        let ready = false
        for (let i = 0; i < 50 && !ready; i++) {
            try {
                await new Promise<void>((resolve, reject) => {
                    const sock = connect({host: '127.0.0.1', port, rejectUnauthorized: false})
                    sock.on('secureConnect', () => { sock.destroy(); resolve() })
                    sock.on('error', reject)
                })
                ready = true
            } catch {
                await sleep(100)
            }
        }
        ttrue(ready, 'private TLS test server started')

        for (let i = 0; i < ATTEMPTS; i++) {
            const reply = await fetch(port)
            ttrue(reply.status.startsWith('HTTP/1.1 200'), `attempt ${i + 1}: expected 200, got [${reply.status}]`)
            ttrue(reply.declared == SIZE, `attempt ${i + 1}: Content-Length must be ${SIZE}, got ${reply.declared}`)
            ttrue(!reply.stalled && reply.received == SIZE,
                  `attempt ${i + 1}: a TLS file transfer ${reply.stalled ? 'stalled' : 'ended'} after ` +
                  `${reply.received} of ${SIZE} bytes`)
        }
    } finally {
        await stopServer(server)
    }
}
