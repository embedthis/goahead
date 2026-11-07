/*
    vxworks-arm-compiles.tst - Every library source compiles in the VxWorks configuration.

    No CI leg builds VxWorks, so a conditional arm can rot unnoticed. Compile each library source with the
    VxWorks platform defines against the host stand-in
    headers used by the CGI backend fixture, with implicit declarations and conflicting types as errors.

    crypt.c is excluded: websGetRandomBytes stops the build with #error until the porter supplies a CSPRNG.
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { teq, tinfo, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { readdirSync } from 'node:fs'
import { join } from 'node:path'

const cc = Cmd.locate('cc')
const ROOT = join(import.meta.dir, '..', '..')
const EXCLUDED = ['crypt.c', 'goahead.c', 'test.c', 'cgitest.c', 'rom.c', 'openssl.c', 'mbedtls.c']

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX host compiler')
} else {
    const sources = readdirSync(join(ROOT, 'src')).filter(f => f.endsWith('.c') && !EXCLUDED.includes(f)).sort()
    const flags = ['-fsyntax-only',
        '-include', join(import.meta.dir, 'vxworks', 'vxworks-host-stubs.h'),
        '-DME_OS_TYPE=16', '-DVXWORKS=1', '-D_WRS_VXWORKS_MAJOR=7', '-D_WRS_VXWORKS_MINOR=0',
        '-DME_COM_SSL=0', '-DME_COM_OPENSSL=0', '-DME_COM_MBEDTLS=0',
        '-Werror=implicit-function-declaration', '-Werror=int-conversion', '-Werror=incompatible-pointer-types',
        '-Werror=int-to-pointer-cast', '-Werror=pointer-to-int-cast',
        '-Wno-macro-redefined',
        '-I' + join(ROOT, 'test', 'regress', 'vxworks'), '-I' + join(ROOT, 'src'), '-I' + join(ROOT, 'src', 'osdep')]
    for (const source of sources) {
        const r = spawnSync(String(cc), [...flags, join(ROOT, 'src', source)], { encoding: 'utf8' })
        const errors = (r.stderr || '').split('\n').filter(line => line.includes('error:')).join('\n')
        teq(r.status, 0, `src/${source} compiles in the VxWorks configuration: ${errors}`)
    }
    tinfo(`${sources.length} sources compiled`)
}
