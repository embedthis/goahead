/*
    hardening-doc.tst - The deployment guide lists the hardening flags the project files build with
 */
import { ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const PREMAKE = join(ROOT, 'projects', 'premake5.lua')
const MAKEFILE = join(ROOT, 'Makefile')

//  A distribution may not carry the project sources the flags are read from
if (existsSync(PREMAKE) && existsSync(MAKEFILE)) {
    let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
    let premake = readFileSync(PREMAKE, 'utf8')
    let makefile = readFileSync(MAKEFILE, 'utf8')

    const HARDENING = new RegExp('"(-fstack-protector[\\w-]*|-Wl,-z,[\\w,-]+|-pie|-fPIE|-Wformat=2|' +
        '-Werror=format-security|/guard:cf|/DYNAMICBASE|/HIGHENTROPYVA|/NXCOMPAT|/CETCOMPAT)"', 'g')
    let flags = new Set([...premake.matchAll(HARDENING)].map(m => m[1]))
    let cfi = (makefile.match(/^CFI_FLAGS\s*:=.*for f in ([^;]*);/m) || ['', ''])[1].trim().split(/\s+/)
    for (let flag of cfi.filter(f => f)) {
        flags.add(flag)
    }
    ttrue(flags.size >= 12, `the project files name the hardening flags, found [${[...flags]}]`)
    ttrue(cfi.includes('-fcf-protection=full') && cfi.includes('-mbranch-protection=standard'),
        `the Makefile selects control-flow protection, found [${cfi}]`)
    for (let flag of flags) {
        ttrue(guide.includes(flag), `SECURITY-DEPLOYMENT.md must list the hardening flag ${flag}`)
    }

    let level = (premake.match(/_FORTIFY_SOURCE=(\d)/) || ['', ''])[1]
    ttrue(level != '', 'the project files set _FORTIFY_SOURCE')
    let documented = new Set((guide.match(/_FORTIFY_SOURCE=\d/g) || []).map(f => f.slice(-1)))
    ttrue(documented.size == 1 && documented.has(level),
        `SECURITY-DEPLOYMENT.md must give _FORTIFY_SOURCE=${level}, the level the project files set, ` +
        `got [${[...documented]}]`)
}
