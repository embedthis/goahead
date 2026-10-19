/* Build the backend fixture when MbedTLS development libraries are installed. */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, thas, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const pkg = spawnSync('pkg-config', ['--cflags', '--libs', 'mbedtls', 'mbedx509', 'mbedcrypto'], { encoding: 'utf8' })
const openssl = spawnSync('openssl', ['version'], { encoding: 'utf8' })
if (String(Config.OS).startsWith('win') || !cc || pkg.status !== 0 || openssl.status !== 0) {
    tskip('Requires a POSIX compiler, the openssl command and MbedTLS development packages via pkg-config')
} else {
    const root = join(import.meta.dir, '..', '..')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-setup-'))
    try {
        const encrypted = join(dir, 'encrypted.key')
        teq(spawnSync('openssl', ['pkey', '-in', join(root, 'test/pki/test.key'), '-aes256', '-passout', 'pass:secret',
            '-out', encrypted]).status, 0, 'write an encrypted key')
        const malformed = join(dir, 'malformed.pem')
        writeFileSync(malformed, 'this is not a certificate, a key or a CRL\n')
        const exe = join(dir, 'probe')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-I' + join(root, 'src'),
            '-I' + join(root, 'src/osdep'), join(import.meta.dir, 'mbedtls-setup-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, 'MbedTLS backend fixture compiles: ' + built.stderr)
        const result = spawnSync(exe, [join(root, 'test', 'pki'), encrypted, malformed], { encoding: 'utf8' })
        teq(result.status, 0, 'the backend fixture passes: ' + result.stderr)
        tcontains(result.stdout, 'setup failure and success passed', 'success control also exercised')
        tcontains(result.stdout, 'dh group passed', 'sslOpen configures the 2048-bit RFC 3526 DHE group')
        tcontains(result.stdout, 'versions and depth passed', 'the version and chain depth settings apply to MbedTLS')
        tcontains(result.stdout, 'key mismatch passed', 'sslOpen refuses a key that does not match the certificate')
        tcontains(result.stdout, 'encrypted key passed', 'an encrypted key is reported as encrypted')
        tcontains(result.stdout, 'malformed files passed', 'a malformed certificate, key and CRL free their buffer')
        if (thas('ME_GOAHEAD_LOGGING')) {
            tcontains(result.stdout, 'debug hook passed',
                'the MbedTLS debug hook handles empty and unterminated messages')
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
