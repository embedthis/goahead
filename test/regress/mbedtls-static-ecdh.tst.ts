/*
    mbedtls-static-ecdh.tst.ts - The MbedTLS cipher floor refuses static ECDH suites, which OpenSSL cannot offer

    The backends must enforce one TLS 1.2 policy. OpenSSL has no static ECDH suites, so a configured one fails its
    startup; MbedTLS must drop it from the list and refuse a client that offers nothing else.
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
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-static-ecdh-'))
    try {
        const exe = join(dir, 'mbedtls-static-ecdh')
        const configured = 'TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256'
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-DME_GOAHEAD_SSL_NO_V1_2=0',
            `-DME_GOAHEAD_SSL_CIPHERS="${configured}"`,
            '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'),
            join(import.meta.dir, 'mbedtls-static-ecdh-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles: ${built.stderr}`)

        const result = spawnSync(exe, [join(root, 'test', 'pki')], { encoding: 'utf8', timeout: 60000 })
        teq(result.status, 0, `the fixture runs to completion: ${result.stderr}`)
        tcontains(result.stdout, 'listed 1 static-ecdh dropped', 'static ECDH names are dropped from a configured list')
        tcontains(result.stdout, 'sslOpen ok', 'the configuration opens with the forward secret suite left')
        tcontains(result.stdout, 'static ecdh refused', 'a client offering only a static ECDH suite is refused')
        tcontains(result.stdout, 'ecdhe accepted', 'a client offering the ECDHE suite is accepted')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
