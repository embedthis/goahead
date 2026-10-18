/* Building against a TLS library older than the supported minimum must fail */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')

const BACKENDS = [
    {
        name: 'OpenSSL',
        source: 'openssl.c',
        defines: ['-DME_COM_OPENSSL=1', '-DME_COM_MBEDTLS=0'],
        versionHeader: 'openssl/opensslv.h',
        versionMacro: 'OPENSSL_VERSION_NUMBER',
        headers: ['openssl/ssl.h', 'openssl/evp.h', 'openssl/rand.h', 'openssl/err.h', 'openssl/dh.h',
            'openssl/core_names.h'],
        below: { version: '0x101010ffL', label: '1.1.1' },
        floor: { version: '0x30000000L', label: '3.0.0' },
        error: 'GoAhead requires OpenSSL 3.0 or later',
    },
    {
        name: 'MbedTLS',
        source: 'mbedtls.c',
        defines: ['-DME_COM_OPENSSL=0', '-DME_COM_MBEDTLS=1'],
        versionHeader: 'mbedtls/mbedtls_config.h',
        versionMacro: 'MBEDTLS_VERSION_NUMBER',
        headers: ['mbedtls/ssl.h', 'mbedtls/ssl_cache.h', 'mbedtls/ssl_ticket.h', 'mbedtls/ctr_drbg.h',
            'mbedtls/net_sockets.h', 'psa/crypto.h', 'mbedtls/debug.h', 'mbedtls/error.h', 'mbedtls/check_config.h'],
        below: { version: '0x03060600', label: '3.6.6' },
        floor: { version: '0x03060700', label: '3.6.7' },
        error: 'GoAhead requires MbedTLS 3.6.7 or later',
    },
]

function stub(dir: string, header: string, body: string) {
    let path = join(dir, header)
    mkdirSync(dirname(path), { recursive: true })
    writeFileSync(path, body)
}

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX compiler')
} else {
    const src = join(import.meta.dir, '..', '..', 'src')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-tls-floor-'))
    try {
        for (let backend of BACKENDS) {
            for (let [which, { version, label }] of [['below', backend.below], ['floor', backend.floor]] as const) {
                let inc = join(dir, `${backend.name}-${which}`)
                for (let header of backend.headers) {
                    stub(inc, header, '')
                }
                stub(inc, backend.versionHeader, `#define ${backend.versionMacro} ${version}\n`)
                let result = spawnSync(String(cc), ['-E', '-I' + inc, '-I' + src, '-I' + join(src, 'osdep'),
                    '-DME_COM_SSL=1', ...backend.defines, join(src, backend.source), '-o', '/dev/null'],
                { encoding: 'utf8' })
                if (which == 'below') {
                    teq(result.status != 0, true, `${backend.name} ${label} must fail to build`)
                    tcontains(result.stderr, backend.error, `${backend.name} ${label} must name the minimum version`)
                } else {
                    teq(result.status, 0, `${backend.name} ${label} must build: ${result.stderr}`)
                }
            }
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
