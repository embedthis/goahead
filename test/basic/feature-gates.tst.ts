/*
    feature-gates.tst - The features on by default are on, so the tests gated on them run
 */

import { tget, thas, tinfo, ttrue } from '@embedthis/testme'
import { readdirSync, readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const DEFAULT_ON = ['ME_GOAHEAD_AUTH', 'ME_GOAHEAD_CGI', 'ME_GOAHEAD_JAVASCRIPT', 'ME_GOAHEAD_UPLOAD', 'ME_SSL']
const ROOT = fileURLToPath(new URL('..', import.meta.url))
const SELF = 'basic/feature-gates.tst.ts'
const strict = !!tget('TM_STRICT_SKIPS')

function gatedOn(flag: string): string[] {
    let pattern = new RegExp(`thas\\(\\s*['"]${flag}['"]\\s*\\)`)
    return (readdirSync(ROOT, { recursive: true }) as string[])
        .filter(f => f.endsWith('.tst.ts') && !f.includes('node_modules') && f != SELF)
        .filter(f => pattern.test(readFileSync(ROOT + f, 'utf8')))
        .sort()
}

for (let flag of DEFAULT_ON) {
    let gated = gatedOn(flag)
    ttrue(gated.length > 0, `${flag} gates at least one test`)
    if (!thas(flag)) {
        let message = `${flag} is off in this build, so these tests skip and are reported as passing: ` +
            gated.join(', ')
        if (strict) {
            ttrue(false, message)
        } else {
            tinfo(message)
        }
    }
}
