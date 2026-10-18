/*
    mbedtls-handshake.tst.ts - MbedTLS runtime certificates and client certificate validation
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
    const certs = join(root, 'test', 'pki')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-handshake-'))

    const SETTINGS = [
        {
            name: 'no client certificates',
            defines: ['-DME_GOAHEAD_SSL_VERIFY_PEER=0'],
            expect: { none: 'accepted', self: 'accepted', test: 'accepted', ec: 'accepted', expired: 'accepted' },
        },
        {
            name: 'client certificates, any issuer',
            defines: ['-DME_GOAHEAD_SSL_VERIFY_PEER=1', '-DME_GOAHEAD_SSL_VERIFY_ISSUER=0'],
            expect: { none: 'refused', self: 'accepted', test: 'accepted', ec: 'accepted', expired: 'refused' },
        },
        {
            name: 'client certificates, trusted issuer',
            defines: ['-DME_GOAHEAD_SSL_VERIFY_PEER=1', '-DME_GOAHEAD_SSL_VERIFY_ISSUER=1',
                `-DME_GOAHEAD_SSL_AUTHORITY="${join(certs, 'ca.crt')}"`],
            expect: { none: 'refused', self: 'refused', test: 'accepted', ec: 'refused', expired: 'refused' },
        },
    ]
    try {
        for (let [i, setting] of SETTINGS.entries()) {
            const exe = join(dir, 'handshake' + i)
            // Apply format checks to the backend not covered by the product build.
            const args = ['-include', FEATURES,
                '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', ...setting.defines, '-Wformat', '-Werror=format',
                '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'),
                join(import.meta.dir, 'mbedtls-handshake-fixture.c'),
                '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
                ...pkg.stdout.trim().split(/\s+/), '-o', exe]
            const built = spawnSync(String(cc), args, { encoding: 'utf8' })
            teq(built.status, 0, `the fixture compiles for ${setting.name}: ${built.stderr}`)

            const result = spawnSync(exe, [certs], { encoding: 'utf8', timeout: 60000 })
            teq(result.status, 0, `the fixture runs for ${setting.name}: ${result.stderr}`)
            tcontains(result.stdout, 'certificate installed', `a runtime certificate is installed (${setting.name})`)
            for (let [client, outcome] of Object.entries(setting.expect)) {
                tcontains(result.stdout, `${client} ${outcome}`,
                    `with ${setting.name}, client "${client}" must be ${outcome}`)
            }
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
