/*
    Analyze auth.c with PAM forced on and require the scan gate to include this analysis.
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { spawnSync } from 'node:child_process'
import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')

const isWindows = String(Config.OS).match(/^win/) != null
const clang = isWindows ? null : Cmd.locate('clang')

function run(cmd: string, args: string[], input?: string): string {
    let result = spawnSync(cmd, args, { encoding: 'utf8', input })
    return (result.stdout || '') + (result.stderr || '')
}

function pamHeaderPresent(): boolean {
    return spawnSync(String(clang), ['-E', '-x', 'c', '-', '-o', '/dev/null'],
        { encoding: 'utf8', input: '#include <security/pam_appl.h>\n' }).status == 0
}

const GATE = join(ROOT, 'bin', 'scan-check.sh')

if (!existsSync(GATE)) {
    tskip('The scan-build gate is not in this tree')
} else if (!clang) {
    tskip('The analyzer probe needs clang')
} else if (!pamHeaderPresent()) {
    tskip('The PAM header is not installed')
} else {
    const DEFINES = ['-DME_COMPILER_HAS_PAM=1', '-DME_COM_SSL=1', '-DME_COM_OPENSSL=1', '-DME_COM_MBEDTLS=0',
        '-DME_VERSION="test"', '-D_FORTIFY_SOURCE=3']
    const INCLUDES = ['-I' + join(ROOT, 'src'), '-I' + join(ROOT, 'src', 'osdep')]

    tinfo('--- The analyzer reports nothing in auth.c with the PAM store compiled in ---')
    let output = run(String(clang), ['--analyze', '-std=gnu11', '-O2', ...DEFINES, ...INCLUDES,
        '-Xclang', '-analyzer-output=text', '-o', '/dev/null', join(ROOT, 'src', 'auth.c')])
    let reports = output.split('\n').filter(line => /src\/auth\.c:\d+:\d+: warning: /.test(line))
    ttrue(!/error:/.test(output), 'auth.c must compile with PAM forced on:\n' + output)
    ttrue(reports.length == 0, 'the analyzer must report nothing in the PAM store:\n' + reports.join('\n'))

    tinfo('--- The gate analyzes the PAM store where the build leaves it out ---')
    let gate = readFileSync(GATE, 'utf8')
    ttrue(/ME_COMPILER_HAS_PAM=1/.test(gate) && /pam_appl\.h/.test(gate),
        'bin/scan-check.sh must analyze src/auth.c with ME_COMPILER_HAS_PAM=1 when the PAM header is present')
}
