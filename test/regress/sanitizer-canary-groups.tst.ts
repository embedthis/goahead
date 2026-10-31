/*
    sanitizer-canary-groups.tst - The sanitizer canary proves every group the Makefile probes for

    The Makefile widens the sanitized build with the groups the compiler accepts, and a probe that selects nothing
    returns the build to the default groups without a word. The canary must therefore run a kind for each probed
    group, chosen by the compiler that built the binary, so a build that lost a group fails before the suite runs.
 */
import { ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const makefile = readFileSync(join(ROOT, 'Makefile'), 'utf8')
const script = readFileSync(join(ROOT, 'test', 'utils', 'sanitizer-canary.sh'), 'utf8')
const source = readFileSync(join(ROOT, 'src', 'test.c'), 'utf8')

//  Group probed by the Makefile, and the canary kind that proves it
const KINDS: Record<string, string> = {
    'integer': 'unsigned',
    'implicit-conversion': 'conversion',
    'bounds-strict': 'bounds',
    'float-cast-overflow': 'float',
}

let probe = makefile.match(/^SANITIZE_FLAGS\s*:=([\s\S]*?)done\)/m)
ttrue(probe != null, 'the Makefile probes SANITIZE_FLAGS')
let groups = [...(probe ? probe[1] : '').matchAll(/-fsanitize=([a-z-]+)/g)].map(m => m[1])
ttrue(groups.length > 0, 'the probe names sanitizer groups')

for (let group of groups) {
    let kind = KINDS[group]
    ttrue(kind != null, `the probed group ${group} has a canary kind`)
    if (kind) {
        ttrue(new RegExp(`^\\s*canary ${kind} `, 'm').test(script), `sanitizer-canary.sh runs the ${kind} kind`)
        ttrue(source.includes(`smatch(kind, "${kind}")`), `goahead-test implements the ${kind} canary`)
    }
}
ttrue(/--version.*clang/.test(script), 'the canary chooses its kinds by the compiler, not by SANITIZE_FLAGS')
ttrue(/CC="\$\(CC\)"[^\n]*sanitizer-canary\.sh/.test(makefile), 'test-asan passes the build compiler to the canary')
