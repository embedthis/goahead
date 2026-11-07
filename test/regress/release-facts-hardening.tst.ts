/*
    release-facts-hardening.tst.ts - The release facts record the hardened build the project files enforce

    doc/compliance/release-facts.md is the source the release artifacts take their values from. Its hardened build
    profile row must describe the flags the project files set, at the fortification level they set, and must not
    describe them as advisory. The compliance tree is retained by the manufacturer and absent from a distribution,
    which skips.
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const FACTS = join(ROOT, 'doc', 'compliance', 'release-facts.md')
const PREMAKE = join(ROOT, 'projects', 'premake5.lua')

if (!existsSync(FACTS) || !existsSync(PREMAKE)) {
    tskip('The compliance record or the project sources are not part of this tree')
} else {
    let facts = readFileSync(FACTS, 'utf8')
    let premake = readFileSync(PREMAKE, 'utf8')
    let row = (facts.match(/^\| Recommended hardened build profile \|(.*)\|\s*$/m) || ['', ''])[1]
    ttrue(row != '', 'release-facts.md has a "Recommended hardened build profile" row')

    ttrue(!/not enforced|do not set|advisory/i.test(row), 'the row must not describe the hardened build as advisory or unenforced')

    let level = (premake.match(/_FORTIFY_SOURCE=(\d)/) || ['', ''])[1]
    ttrue(level != '', 'the project files set _FORTIFY_SOURCE')
    let documented = new Set((row.match(/_FORTIFY_SOURCE=\d/g) || []).map(f => f.slice(-1)))
    ttrue(documented.size == 1 && documented.has(level),
          `the row must give _FORTIFY_SOURCE=${level}, the level the project files set, got [${[...documented]}]`)

    for (let flag of ['-fstack-protector-strong', '-fPIE', '-pie', '-Wl,-z,relro,-z,now', '-Wl,-z,noexecstack',
                      '/guard:cf', '/DYNAMICBASE', '/NXCOMPAT', '/CETCOMPAT']) {
        ttrue(premake.includes(`"${flag}"`), `the project files set ${flag}`)
        ttrue(row.includes(flag), `the row must name ${flag}`)
    }
}
