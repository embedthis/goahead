/*
    https-listen-certificate.tst - An HTTPS endpoint is refused without a certificate and key
 */
import { tcontains, tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { connect } from 'node:tls'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const ENDPOINT = 'https://127.0.0.1:14977'
const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const CERTS = join(HOME, 'pki')

function handshake(): Promise<boolean> {
    return new Promise((resolve) => {
        let s = connect({ host: '127.0.0.1', port: 14977, rejectUnauthorized: false }, () => {
            s.destroy()
            resolve(true)
        })
        s.on('error', () => resolve(false))
    })
}

if (!thas('ME_SSL')) {
    tskip('SSL is not enabled')
} else {
    let refused = spawnSync('goahead', ['--no-strict', '--route', ROUTES, 'web', ENDPOINT],
        { cwd: HOME, encoding: 'utf8', timeout: 10000 })
    ttrue(refused.status != 0 && refused.status != null, `goahead must exit when refused, got status ${refused.status}`)
    if (thas('ME_GOAHEAD_LOGGING')) {
        tcontains(refused.stdout + refused.stderr, 'no certificate and key are configured',
            'the refusal must say the certificate and key are missing')
    }

    let server = startServer('goahead', ['--no-strict', '--route', ROUTES, '--cert', join(CERTS, 'self.crt'),
        '--key', join(CERTS, 'self.key'), 'web', ENDPOINT], { cwd: HOME, stdio: 'ignore' })
    try {
        let ok = false
        for (let i = 0; i < 50 && !ok; i++) {
            await new Promise(r => setTimeout(r, 100))
            ok = await handshake()
        }
        ttrue(ok, 'with a certificate and key the endpoint must listen and complete a handshake')
    } finally {
        await stopServer(server)
    }
}
