/*
    mbedtls-ciphers.tst.ts - The MbedTLS backend enforces the cipher floor, bounds its cipher list and rejects an
    unusable one
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
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
    const certs = join(root, 'certs')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-ciphers-'))
    const fixture = join(import.meta.dir, 'mbedtls-ciphers-fixture.c')
    const libs = pkg.stdout.trim().split(/\s+/)


    const probe = join(dir, 'probe.c')
    writeFileSync(probe, 'int main(void) { return 0; }\n')
    const sanitize = spawnSync(String(cc), ['-fsanitize=address', probe, '-o', join(dir, 'probe')], {encoding: 'utf8'})
        .status == 0 ? ['-fsanitize=address', '-fno-omit-frame-pointer'] : []

    function run(name: string, defines: string[]) {
        const exe = join(dir, name)
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-DME_GOAHEAD_SSL_NO_V1_2=0', ...defines, ...sanitize,
            '-Wformat', '-Werror=format', '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'), fixture,
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead', ...libs, '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles for ${name}: ${built.stderr}`)
        const result = spawnSync(exe, [certs], { encoding: 'utf8', timeout: 60000 })
        teq(result.status, 0, `the fixture runs for ${name}: ${result.stderr}`)
        return result.stdout
    }

    try {
        //  The default list: forward secret suites only, so static RSA and SHA-1 CBC are refused
        let out = run('default', [])
        tcontains(out, 'repeats passed', 'a repeated name is used once and does not overflow the list')
        tcontains(out, 'filtering passed', 'unsupported and below-floor names are dropped')
        tcontains(out, 'sslOpen ok', 'the default configuration opens')
        tcontains(out, 'default list passed', 'the default list holds the forward secret suites only')
        tcontains(out, 'static RSA gcm refused', 'a client offering only static RSA is refused')
        tcontains(out, 'static RSA cbc refused', 'a client offering only static RSA with SHA-1 CBC is refused')
        tcontains(out, 'ecdhe sha1 cbc refused', 'a client offering only ECDHE with SHA-1 CBC is refused')
        tcontains(out, 'ecdhe gcm accepted', 'a client offering ECDHE with AES-GCM is accepted')

        //  A configured list never brings in a suite below the floor
        const mixed = 'TLS_RSA_WITH_AES_128_GCM_SHA256:TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256'
        out = run('floor', [`-DME_GOAHEAD_SSL_CIPHERS="${mixed}"`])
        tcontains(out, 'configured 1', 'only the permitted suite is configured')
        tcontains(out, 'static RSA gcm refused', 'the configured static RSA suite is refused')
        tcontains(out, 'ecdhe gcm accepted', 'the permitted suite is accepted')

        //  A list with nothing usable left fails at startup
        out = run('only-floor', ['-DME_GOAHEAD_SSL_CIPHERS="TLS_RSA_WITH_AES_128_CBC_SHA"'])
        tcontains(out, 'sslOpen failed', 'a list of only below-floor ciphers fails sslOpen')
        out = run('unsupported', ['-DME_GOAHEAD_SSL_CIPHERS="NOT_A_CIPHER"'])
        tcontains(out, 'sslOpen failed', 'a list of only unsupported ciphers fails sslOpen')

        //  A very long list is configured without overrun, each suite once
        const long = Array(400).fill('TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256').join(':')
        out = run('long', [`-DME_GOAHEAD_SSL_CIPHERS="${long}"`])
        tcontains(out, 'sslOpen ok', 'an over-long list opens')
        tcontains(out, 'configured 1', 'the repeated suite is configured once')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
