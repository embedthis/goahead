/* Build the MbedTLS transport fixture when MbedTLS development libraries are installed. */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const pkg = spawnSync('pkg-config', ['--cflags', '--libs', 'mbedtls', 'mbedx509', 'mbedcrypto'], { encoding: 'utf8' })

if (String(Config.OS).startsWith('win') || !cc || pkg.status !== 0) {
    tskip('Requires a POSIX compiler and MbedTLS development packages via pkg-config')
} else {
    const root = join(import.meta.dir, '..', '..')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-transport-'))
    try {
        const exe = join(dir, 'probe')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-I' + join(root, 'src'),
            '-I' + join(root, 'src/osdep'), join(import.meta.dir, 'mbedtls-transport-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, 'MbedTLS transport fixture compiles: ' + built.stderr)
        const result = spawnSync(exe, [], { encoding: 'utf8' })
        teq(result.status, 0, 'the transport fixture passes: ' + result.stderr)
        tcontains(result.stdout, 'transport passed', 'every errno is mapped as the flush expects')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
