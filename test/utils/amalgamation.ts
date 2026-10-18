/*
    amalgamation.ts - Build a program against dist/goaheadLib.c at chosen ME_GOAHEAD_* settings
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { join } from 'node:path'

export const ROOT = join(import.meta.dir, '..', '..')

export const isWindows = String(Config.OS).match(/^win/) != null

/*
    Rebuild only on POSIX; MSVC requires platform import libraries. Return NULL without a compiler.
 */
export const cc: string | null = isWindows ? null : (Cmd.locate('cc') || Cmd.locate('gcc') || Cmd.locate('clang'))

/*
    Disable SSL and PAM to isolate configuration tests from host dependencies. Supply ME_VERSION explicitly.
 */
const COMMON = '-I' + join(ROOT, 'src') + ' -I' + join(ROOT, 'src', 'osdep') +
    ' -DME_COM_SSL=0 -DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=0 -DME_COMPILER_HAS_PAM=0' +
    ' -DME_VERSION=\\"test\\"'

const LIB = join(ROOT, 'dist', 'goaheadLib.c')

/*
    Compile main against the amalgamation into exe with the given -D defines
 */
export function compile(exe: string, defines: string, main: string): Cmd {
    return new Cmd(cc + ' -o ' + exe + ' ' + defines + ' ' + COMMON + ' ' + main + ' ' + LIB)
}
