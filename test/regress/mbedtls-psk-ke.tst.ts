/*
    Refuse psk_ke-only resumption; accept psk_dhe_ke to preserve forward secrecy.
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')
//  The fixture links the built library, so it compiles with the library's feature flags
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const pkg = spawnSync('pkg-config', ['--cflags', '--libs', 'mbedtls', 'mbedx509', 'mbedcrypto'], { encoding: 'utf8' })

if (String(Config.OS).startsWith('win') || !cc || pkg.status !== 0) {
    tskip('Requires a POSIX compiler and MbedTLS development packages via pkg-config')
} else {
    const root = join(import.meta.dir, '..', '..')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-psk-ke-'))
    try {
        const exe = join(dir, 'mbedtls-psk-ke')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-DME_GOAHEAD_SSL_NO_V1_3=0', '-DME_GOAHEAD_SSL_TICKET=1',
            '-DME_GOAHEAD_SSL_VERIFY_PEER=0',
            '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'),
            join(import.meta.dir, 'mbedtls-psk-ke-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles: ${built.stderr}`)

        const result = spawnSync(exe, [join(root, 'test', 'pki')], { encoding: 'utf8', timeout: 60000 })
        teq(result.status, 0, `the fixture runs to completion: ${result.stderr}`)
        if (result.stdout.includes('unsupported')) {
            tskip('Requires MbedTLS 3.6 with TLS 1.3 session tickets')
        } else {
            tcontains(result.stdout, 'sslOpen ok', 'the configuration opens')
            tcontains(result.stdout, 'full accepted', 'a full TLS 1.3 handshake is accepted and issues a ticket')
            tcontains(result.stdout, 'psk_dhe_ke accepted', 'a resumption with an ephemeral key exchange is accepted')
            tcontains(result.stdout, 'psk_ke refused', 'a resumption without an ephemeral key exchange is refused')
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
