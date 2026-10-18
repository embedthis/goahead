/* The committed amalgamation in dist/ matches src/ */

import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')
const SCRIPT = join(TOP, 'bin', 'buildLib.sh')

if (!existsSync(SCRIPT)) {
    tskip('bin/buildLib.sh is not part of a source distribution, so there is nothing to compare dist/ with')
} else if (process.platform == 'win32') {
    tskip('bin/buildLib.sh is a POSIX shell script; the Unix runs verify dist/')
} else {
    let scratch = mkdtempSync(join(tmpdir(), 'goahead-dist-'))
    try {
        let result = spawnSync('bash', [SCRIPT, '--dist-only'], {
            env: { ...process.env, DIST: scratch }, encoding: 'utf8', timeout: 60000
        })
        ttrue(result.status == 0, `bin/buildLib.sh --dist-only must succeed: ${result.stderr}`)
        let generated = readdirSync(scratch).sort()
        ttrue(generated.length > 0, 'bin/buildLib.sh must generate the amalgamation')
        let stale = generated.filter(name => !existsSync(join(TOP, 'dist', name)) ||
            !readFileSync(join(TOP, 'dist', name)).equals(readFileSync(join(scratch, name))))
        if (stale.length) {
            tinfo('Stale: ' + stale.join(', ') + '. Run "make package" and commit dist/ with the source change.')
        }
        ttrue(stale.length == 0, `dist/ must match src/, stale: ${stale.join(', ')}`)
    } finally {
        rmSync(scratch, { recursive: true, force: true })
    }
}
