/*
    mbedtls-write-retry.tst.ts - Every byte an MbedTLS write reports as written must reach the peer

    A write MbedTLS could not send is retried from the HTTP output buffer, which may hold more bytes by then. The
    backend must retry the pending record with its own length and then send the rest, rather than report the whole
    buffer written on the strength of the one record.
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const FIRST = 600
const SECOND = 400
const cc = Cmd.locate('cc')
//  The fixture links the built library, so it compiles with the library's feature flags
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const pkg = spawnSync('pkg-config', ['--cflags', '--libs', 'mbedtls', 'mbedx509', 'mbedcrypto'], { encoding: 'utf8' })

if (String(Config.OS).startsWith('win') || !cc || pkg.status !== 0) {
    tskip('Requires a POSIX compiler and MbedTLS development packages via pkg-config')
} else {
    const root = join(import.meta.dir, '..', '..')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-write-retry-'))
    try {
        const exe = join(dir, 'mbedtls-write-retry')
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-I' + join(root, 'src'),
            '-I' + join(root, 'src/osdep'), join(import.meta.dir, 'mbedtls-write-retry-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles: ${built.stderr}`)

        const result = spawnSync(exe, [join(root, 'test', 'pki')], { encoding: 'utf8', timeout: 60000 })
        teq(result.status, 0, `the fixture runs to completion: ${result.stderr}`)
        tcontains(result.stdout, 'first -1', 'a write the transport cannot take reports nothing sent')
        tcontains(result.stdout, `retry ${FIRST + SECOND}`, 'the retry with more output reports it all written')
        tcontains(result.stdout, `delivered ${FIRST + SECOND}`, 'and the peer receives every byte reported written')
        tcontains(result.stdout, 'content intact', 'in order and once')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
