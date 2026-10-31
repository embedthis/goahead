/*
    windows-release-warning-gate.tst.ts - The CI Windows release build fails on any compiler or linker warning
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const CI = join(import.meta.dir, '..', '..', '.github', 'workflows', 'ci.yml')

if (!existsSync(CI)) {
    tskip('The CI workflow is not part of a source distribution')
} else {
    let text = readFileSync(CI, 'utf8')
    let lines = text.split('\n').filter(line => /msbuild .*Configuration=release/.test(line))
    ttrue(lines.length == 1, 'ci.yml must build the Windows release configuration once')
    ttrue(/\/p:TreatWarningAsError=true|\/warnaserror/.test(lines[0] || ''),
          'the Windows release build must treat warnings as errors, as the Unix release step does')
}
