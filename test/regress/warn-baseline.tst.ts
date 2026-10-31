/*
    warn-baseline.tst - The expanded compiler-diagnostic gate has a committed, reviewed baseline

    `sec-scan warn --gate` reports diagnostics beyond the baseline as net-new. Without a baseline every existing
    diagnostic is net-new and nothing gates. Every suppression must carry a justification: an unjustified one is
    itself a finding. The security harness is internal and absent from a source distribution, which skips.
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const SEC = join(import.meta.dir, '..', 'sec')
const BASELINE = join(SEC, 'static', 'warn-baseline.json')

if (!existsSync(SEC)) {
    tskip('The security harness is not part of this tree')
} else {
    ttrue(existsSync(BASELINE), 'test/sec/static/warn-baseline.json must hold the reviewed expanded-warning baseline')
    if (existsSync(BASELINE)) {
        let baseline = JSON.parse(readFileSync(BASELINE, 'utf8'))
        let tools = Object.keys(baseline.tools || {})
        ttrue(tools.includes('clang') && tools.includes('gcc'), `the baseline covers clang and gcc, has ${tools}`)
        for (let tool of tools) {
            for (let entry of baseline.tools[tool].suppressions) {
                ttrue(!!entry.justification, `${tool} ${entry.file}:${entry.line} ${entry.checker} needs a justification`)
            }
        }
    }
}
