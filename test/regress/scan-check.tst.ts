/* bin/scan-check.sh fails on any static-analysis finding missing from the baseline */

import { tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')
const SCRIPT = join(TOP, 'bin', 'scan-check.sh')

const A = 'deadcode.DeadStores | src/http.c | getToken | Value stored to \'endToken\' is never read'
const B = 'core.NullDereference | src/cgitest.c | main | Dereference of null pointer (loaded from variable \'cp\')'
const C = 'unix.Malloc | src/runtime.c | sclone | Potential leak of memory'

if (!existsSync(join(TOP, 'test', 'sec'))) {
    tskip('The static-analysis gate is internal and is not part of a source distribution')
} else if (process.platform == 'win32') {
    tskip('bin/scan-check.sh is a POSIX shell script; the Unix runs verify it')
} else if (spawnSync('python3', ['--version']).status !== 0) {
    tskip('bin/scan-check.sh needs python3, which this host lacks; CI installs it for the gate')
} else {
    ttrue(existsSync(SCRIPT), 'bin/scan-check.sh must exist')
    let scratch = mkdtempSync(join(tmpdir(), 'goahead-scan-'))

    function compare(keys: string[], baseline: string[]) {
        writeFileSync(join(scratch, 'keys.txt'), keys.map(k => k + '\n').join(''))
        writeFileSync(join(scratch, 'baseline.txt'), baseline.map(k => k + '\n').join(''))
        let result = spawnSync('bash', [SCRIPT, '--compare', join(scratch, 'keys.txt'), join(scratch, 'baseline.txt')],
            { encoding: 'utf8', timeout: 30000 })
        return { status: result.status, out: result.stdout + result.stderr }
    }
    try {
        let baseline = ['# comment', '', A + '  # Not a security issue: dead store', C + '  # False positive']

        let r = compare([C, A], baseline)
        ttrue(r.status == 0, `identical findings must pass: ${r.out}`)

        r = compare([A, B, C], baseline)
        ttrue(r.status == 1, `a new finding must fail: ${r.out}`)
        ttrue(r.out.includes(B), `the new finding must be named: ${r.out}`)
        ttrue(!r.out.includes('finding: ' + A), `a baselined finding must not be reported as new: ${r.out}`)

        r = compare([A], baseline)
        ttrue(r.status == 0, `a finding no longer reported must pass: ${r.out}`)
        ttrue(r.out.includes('notice') && r.out.includes(C), `a stale baseline key must be noticed: ${r.out}`)

        r = compare([A, A, C], baseline)
        ttrue(r.status == 1, `a second report with a baselined key must fail: ${r.out}`)

        r = compare([], [])
        ttrue(r.status == 0, `no findings and an empty baseline must pass: ${r.out}`)

        rmSync(join(scratch, 'baseline.txt'))
        let missing = spawnSync('bash', [SCRIPT, '--compare', join(scratch, 'keys.txt'), join(scratch, 'baseline.txt')],
            { encoding: 'utf8', timeout: 30000 })
        ttrue(missing.status == 2, `a missing baseline must fail: ${missing.stderr}`)

        let committed = join(TOP, 'test', 'sec', 'scan-build-baseline.txt')
        if (existsSync(committed)) {
            ttrue(!readFileSync(committed, 'utf8').includes('  # TRIAGE'), 'every baselined finding must be triaged')
        }
    } finally {
        rmSync(scratch, { recursive: true, force: true })
    }
}
