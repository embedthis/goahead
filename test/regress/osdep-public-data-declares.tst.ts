/*
    osdep-public-data-declares.tst.ts - PUBLIC_DATA declares a variable on every platform; it never defines one

    goahead.h writes "PUBLIC_DATA int logLevel;" and "PUBLIC_DATA WebsRomIndex websRomIndex[];" so that every
    source file sees the one definition in runtime.c and rom.c. On Unix PUBLIC_DATA is "extern". On Windows it must
    be "extern __declspec(dllexport)" (library build) or "extern __declspec(dllimport)" (ME_DLL_IMPORT consumer):
    without extern each unit that includes goahead.h defines the variable, which the MSVC linker reports as
    LNK2005 under clang-cl or any -fno-common toolchain, and which MSVC itself accepts only as a COFF common symbol.

    osdep.h is preprocessed against empty stub system headers, so the test runs on any host with a C compiler and
    needs no Windows SDK.
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')
const OSDEP = join(import.meta.dir, '..', '..', 'src', 'osdep')

//  osdep.h detects the target from compiler macros, so the host's are undefined before Windows is defined
const HOST = ['__APPLE__', '__MACH__', '__linux__', '__FreeBSD__', '__OpenBSD__', '__NetBSD__', '__unix__', '__unix']
const WINDOWS = [...HOST.map(m => '-U' + m), '-D_WIN32=1', '-D_MSC_VER=1940', '-D_M_X64=1']
const TARGETS = [
    { name: 'Unix', defines: [] },
    { name: 'Windows library (dllexport)', defines: WINDOWS },
    { name: 'Windows consumer (ME_DLL_IMPORT)', defines: [...WINDOWS, '-DME_DLL_IMPORT=1'] },
]

function expansion(dir: string, defines: string[]): { status: number | null, value: string, stderr: string } {
    let probe = join(dir, 'probe.c')
    writeFileSync(probe, '#include "osdep.h"\nPROBE_BEGIN PUBLIC_DATA PROBE_END\n')
    let result = spawnSync(String(cc), ['-E', '-P', '-nostdinc', '-I' + join(dir, 'stubs'), '-I' + OSDEP,
        ...defines, probe], { encoding: 'utf8' })
    let match = /PROBE_BEGIN\s*(.*?)\s*PROBE_END/s.exec(result.stdout || '')
    return { status: result.status, value: match ? match[1].replace(/\s+/g, ' ') : '', stderr: result.stderr }
}

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX compiler')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-public-data-'))
    try {
        //  An empty stub for every system header osdep.h can include, so the preprocessor never leaves the tree
        let header = readFileSync(join(OSDEP, 'osdep.h'), 'utf8')
        for (let match of header.matchAll(/^\s*#\s*include\s+[<"]([^>"]+)[>"]/gm)) {
            let path = join(dir, 'stubs', match[1])
            mkdirSync(dirname(path), { recursive: true })
            writeFileSync(path, '')
        }
        //  osdep.h compares the kernel version with the macros this header defines
        writeFileSync(join(dir, 'stubs', 'linux', 'version.h'),
            '#define LINUX_VERSION_CODE 0x60000\n#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))\n')
        for (let target of TARGETS) {
            let { status, value, stderr } = expansion(dir, target.defines)
            teq(status, 0, `${target.name}: osdep.h must preprocess: ${stderr}`)
            tcontains(value, 'extern',
                `${target.name}: PUBLIC_DATA must declare, not define, but expands to "${value}"`)
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
