/*
    Bad TLS handshakes must not appear at the default log level. Inspect a private server's level-0 log.
 */

import { Cmd } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, readFileSync, rmSync } from 'node:fs'
import { get } from 'node:https'
import { connect } from 'node:net'
import { createServer } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const ROOT = join(import.meta.dir, '..', '..')
const TEST = join(ROOT, 'test')
const LOG = join(TEST, 'ztls-handshake.log')
const ATTEMPTS = 20

async function freePort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

/*
    Connect to the TLS port and send bytes that cannot complete a handshake, then wait for the server to close.
 */
function badHandshake(port: number, bytes: Buffer): Promise<void> {
    return new Promise(resolve => {
        let s = connect(port, '127.0.0.1', () => s.write(bytes))
        s.setTimeout(2000, () => s.destroy())
        s.on('error', () => {})
        s.on('close', () => resolve())
    })
}

function tlsGet(port: number, path: string): Promise<number> {
    return new Promise((resolve, reject) => {
        const req = get({ host: '127.0.0.1', port, path, rejectUnauthorized: false }, res => {
            res.resume()
            res.on('end', () => resolve(res.statusCode || 0))
        })
        req.on('error', reject)
        req.setTimeout(3000, () => req.destroy(new Error('request timed out')))
    })
}

if (tget('ME_SSL', '1') == '0' || !existsSync(join(TEST, 'pki', 'self.crt'))) {
    tskip('TLS is not enabled')
} else if (!thas('ME_GOAHEAD_LOGGING')) {
    tskip('Logging is not in this build')
} else {
    rmSync(LOG, { force: true })
    const port = await freePort()
    const server = startServer(join(ROOT, 'build/bin/goahead-test'),
        ['--log', LOG + ':0', '--route', String(tget('TM_ROUTES')), '--cert', 'pki/self.crt', '--key', 'pki/self.key', 'web', `https://127.0.0.1:${port}`],
        { cwd: TEST, stdio: 'ignore' })
    try {
        let ready = false
        for (let i = 0; i < 100 && !ready; i++) {
            try {
                ready = (await tlsGet(port, '/index.html')) == 200
            } catch {}
            if (!ready) {
                await new Promise(resolve => setTimeout(resolve, 25))
            }
        }
        ttrue(ready, 'the private TLS server must start')

        //  Plain text on the TLS port, a truncated record, and a ClientHello offering only TLS 1.2 where available
        const openssl = Cmd.locate('openssl')
        for (let i = 0; i < ATTEMPTS; i++) {
            await badHandshake(port, Buffer.from('GET /index.html HTTP/1.0\r\n\r\n'))
            await badHandshake(port, Buffer.from([0x16, 0x03, 0x01, 0x00, 0x05, 0x6a, 0x75, 0x6e, 0x6b]))
            if (openssl) {
                spawnSync(String(openssl), ['s_client', '-connect', `127.0.0.1:${port}`, '-tls1_2'],
                    { input: '', timeout: 5000 })
            }
        }
        let status = await tlsGet(port, '/index.html')
        ttrue(status == 200, `the server must still serve TLS clients, got ${status}`)

        let log = readFileSync(LOG, 'utf8')
        ttrue(!log.includes('Cannot upgrade to TLS'),
            'a failed client handshake must not be logged as a server fault at level 0')
    } finally {
        await stopServer(server)
        rmSync(LOG, { force: true })
    }
}
