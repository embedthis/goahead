/*
    Pending MbedTLS output must wait for socket writability without spinning the event loop.
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { teq, tget, tskip, ttrue } from '@embedthis/testme'
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
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-stalled-'))
    try {
        const exe = join(dir, 'mbedtls-stalled-reader')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-I' + join(root, 'src'),
            '-I' + join(root, 'src/osdep'), join(import.meta.dir, 'mbedtls-stalled-reader-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles: ${built.stderr}`)

        const result = spawnSync(exe, [join(root, 'test', 'pki')], { encoding: 'utf8', timeout: 60000 })
        teq(result.status, 0, `the fixture runs to completion: ${result.stderr}`)
        const turns = Number((result.stdout.match(/turns (\d+)/) || [])[1])
        const queued = Number((result.stdout.match(/queued (\d+)/) || [])[1])
        ttrue(queued > 0, `the response is still queued while the peer does not read (queued ${queued})`)
        ttrue(turns > 0 && turns <= 20,
            `the event loop must wait for the socket to become writable, not re-service the connection (took ${turns} turns in 500 ms)`)
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
