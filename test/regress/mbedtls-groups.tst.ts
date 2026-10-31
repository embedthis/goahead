/*
    MbedTLS accepts elliptic curve key exchange groups and refuses finite-field groups.
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
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-groups-'))
    try {
        const exe = join(dir, 'mbedtls-groups')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0',
            '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'),
            join(import.meta.dir, 'mbedtls-groups-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles: ${built.stderr}`)

        const result = spawnSync(exe, [join(root, 'test', 'pki')], { encoding: 'utf8', timeout: 120000 })
        teq(result.status, 0, `the fixture runs to completion: ${result.stderr}`)
        tcontains(result.stdout, 'sslOpen ok', 'the default configuration opens')
        tcontains(result.stdout, 'x25519 accepted', 'a client offering X25519 is accepted')
        tcontains(result.stdout, 'secp256r1 accepted', 'a client offering P-256 is accepted')
        tcontains(result.stdout, 'secp384r1 accepted', 'a client offering P-384 is accepted')
        tcontains(result.stdout, 'ffdhe2048 refused', 'a client offering only ffdhe2048 is refused')
        tcontains(result.stdout, 'ffdhe8192 refused', 'a client offering only ffdhe8192 is refused')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
