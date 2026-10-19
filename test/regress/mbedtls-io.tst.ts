/* MbedTLS writes, the buffered-write flag, writes before the handshake and keys without a public point */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const BIG = 20000
const cc = Cmd.locate('cc')
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const pkg = spawnSync('pkg-config', ['--cflags', '--libs', 'mbedtls', 'mbedx509', 'mbedcrypto'], { encoding: 'utf8' })
const openssl = spawnSync('openssl', ['version'], { encoding: 'utf8' })

if (String(Config.OS).startsWith('win') || !cc || pkg.status !== 0 || openssl.status !== 0) {
    tskip('Requires a POSIX compiler, the openssl command and MbedTLS development packages via pkg-config')
} else {
    const root = join(import.meta.dir, '..', '..')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-io-'))
    try {
        const full = join(dir, 'ec.pem')
        const nopub = join(dir, 'ec-nopublic.pem')
        teq(spawnSync('openssl', ['ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', full]).status, 0,
            'generate an EC key')
        teq(spawnSync('openssl', ['ec', '-in', full, '-no_public', '-out', nopub]).status, 0,
            'write it without its public point')

        const exe = join(dir, 'mbedtls-io')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-I' + join(root, 'src'),
            '-I' + join(root, 'src/osdep'), join(import.meta.dir, 'mbedtls-io-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles: ${built.stderr}`)

        const result = spawnSync(exe, [join(root, 'test', 'pki'), nopub], { encoding: 'utf8', timeout: 60000 })
        let partial = Number((result.stdout.match(/partial (-?\d+)/) || [])[1])
        ttrue(partial > 0 && partial < BIG, `a write blocked after its first record must report what it sent, got ${partial}`)
        tcontains(result.stdout, `delivered ${BIG}`, 'and completing it delivers each byte once')
        tcontains(result.stdout, 'buffered-write 0', 'a flushed write must not leave the socket marked as holding output')
        tcontains(result.stdout, 'handshaking write -1 eof 1 reads 0',
            'a write before the handshake completes fails and reads nothing from the peer')
        tcontains(result.stdout, 'nopublic parsed', 'an EC key without its public point must parse')
        teq(result.status, 0, `the fixture runs to completion: ${result.stderr}`)
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
