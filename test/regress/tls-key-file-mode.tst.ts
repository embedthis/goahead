/*
    Warn at startup when the TLS private key grants group or other permissions; accept mode 0600 silently.
 */
import { tcontains, tfalse, tget, thas, tskip, ttrue } from '@embedthis/testme'
import { copyFileSync, chmodSync, mkdtempSync, rmSync } from 'node:fs'
import { createServer } from 'node:net'
import { connect } from 'node:tls'
import { join } from 'node:path'
import { tmpdir } from 'node:os'
import { startServer, stopServer } from '../utils/server'

const PORT = await ephemeralPort()
const ENDPOINT = `https://127.0.0.1:${PORT}`
const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const CERTS = join(HOME, 'pki')

async function ephemeralPort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

function handshake(): Promise<boolean> {
    return new Promise((resolve) => {
        let s = connect({ host: '127.0.0.1', port: PORT, rejectUnauthorized: false }, () => {
            s.destroy()
            resolve(true)
        })
        s.on('error', () => resolve(false))
    })
}

/*
    Start the server with the given key, wait until it answers a handshake, stop it and return its log output
 */
async function startupLog(key: string): Promise<string> {
    let output = ''
    let server = startServer('goahead', ['--no-strict', '--route', ROUTES, '--cert', join(CERTS, 'test.crt'),
        '--key', key, 'web', ENDPOINT], { cwd: HOME, stdio: ['ignore', 'pipe', 'pipe'] })
    server.stdout?.on('data', (d) => output += d)
    server.stderr?.on('data', (d) => output += d)
    try {
        let ok = false
        for (let i = 0; i < 50 && !ok; i++) {
            await new Promise(r => setTimeout(r, 100))
            ok = await handshake()
        }
        ttrue(ok, 'the server must listen with the key ' + key)
    } finally {
        await stopServer(server)
    }
    return output
}

if (!thas('ME_SSL') || !thas('ME_GOAHEAD_LOGGING') || process.platform == 'win32') {
    tskip('Requires SSL, logging and POSIX file modes')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-key-mode-'))
    try {
        const shared = join(dir, 'shared.key')
        copyFileSync(join(CERTS, 'test.key'), shared)
        chmodSync(shared, 0o644)
        let log = await startupLog(shared)
        tcontains(log, `"${shared}" holds the TLS private key and is readable or writable by other accounts`,
            'a key readable by other accounts must be reported at startup')

        const owned = join(dir, 'owned.key')
        copyFileSync(join(CERTS, 'test.key'), owned)
        chmodSync(owned, 0o600)
        log = await startupLog(owned)
        tfalse(log.includes('holds the TLS private key'), 'a key with mode 0600 must not be reported')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
