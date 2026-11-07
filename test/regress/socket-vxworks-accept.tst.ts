/*
    socket-vxworks-accept.tst - The VxWorks build of the socket layer does not spin on a listener it cannot accept from.

    VxWorks has no process descriptor limit to read and no /dev/null, so the POSIX-only spare descriptor that refuses
    a connection when none is free is compiled out, while the connection cap keeps its compile-time default of
    FD_SETSIZE minus 32, far above the kernel's descriptor table. Compile the socket layer with the VxWorks platform
    defines against the host stand-in headers and drive it with a queued connection and no descriptor free: the
    event loop must not wake on every turn, and the queued connection must be dropped.
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, thas, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { copyFileSync, mkdtempSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const cc = Cmd.locate('cc')
//  The fixture links the built library, so it compiles with the library's feature flags
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const ROOT = join(import.meta.dir, '..', '..')

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX host compiler')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-vxsock-'))
    try {
        const fixture = join(import.meta.dir, 'socket-vxworks-accept-fixture.c')
        const exe = join(dir, 'vxsock')
        const args = ['-include', FEATURES, '-include', join(import.meta.dir, 'vxworks', 'vxworks-host-stubs.h'),
            '-DME_OS_TYPE=16', '-DVXWORKS=1', '-D_WRS_VXWORKS_MAJOR=7', '-D_WRS_VXWORKS_MINOR=0',
            '-Werror=int-conversion', '-Werror=incompatible-pointer-types', '-Werror=implicit-function-declaration',
            '-Werror=int-to-pointer-cast', '-Werror=pointer-to-int-cast', '-Wno-macro-redefined',
            '-I' + join(ROOT, 'test', 'regress', 'vxworks'), '-I' + join(ROOT, 'src'), '-I' + join(ROOT, 'src', 'osdep'),
            fixture, '-L' + join(ROOT, 'build', 'bin'), '-Wl,-rpath,' + join(ROOT, 'build', 'bin'), '-lgoahead', '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, 'the VxWorks socket layer compiles: ' + built.stderr)
        if (built.status === 0) {
            const result = spawnSync(exe, [], { encoding: 'utf8' })
            tcontains(result.stdout, 'accept exhaustion passed',
                'with no descriptor free the loop does not spin on the listener and the queued connection is dropped: ' +
                result.stdout + result.stderr)
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
