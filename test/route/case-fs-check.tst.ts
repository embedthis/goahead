/*
    case-fs-check.tst - Startup reports a served directory whose case rule differs from the build's
 */

import { Cmd } from '@embedthis/ejscript'
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from 'node:fs'
import { createServer } from 'node:net'
import { join } from 'node:path'
import { tmpdir } from 'node:os'
import { cc, compile, isWindows, ROOT } from '../utils/amalgamation'

/*
    check reports audit counts; listen and nostrict test startup outcomes under their respective policies.
 */
const DRIVER = `
#include "goahead.h"

int main(int argc, char **argv)
{
    int rc;

    if (websOpen("web", "route.txt") < 0) {
        return 2;
    }
    if (smatch(argv[1], "check")) {
        printf("problems=%d\\n", websCheckRoutes());
        rc = 0;
    } else {
        websSetStrict(!smatch(argv[1], "nostrict"));
        rc = websListen(argv[2]) < 0 ? 1 : 0;
    }
    websClose();
    return rc;
}
`


let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
ttrue(guide.split('\n').some(line => line.includes('ME_GOAHEAD_CASE_INSENSITIVE_FS') && !line.includes('make CFLAGS') &&
    /FAT/.test(line) && /SMB/.test(line)), 'SECURITY-DEPLOYMENT.md must say to set ME_GOAHEAD_CASE_INSENSITIVE_FS ' +
    'for a document directory on a case-insensitive file system')

if (isWindows) {
    tskip('Windows builds always fold case, and rebuilding the amalgamation is a POSIX-only test')
} else if (!cc) {
    tskip('No C compiler on PATH to rebuild the amalgamation')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-casefs-'))
    const web = join(dir, 'web')
    mkdirSync(web)
    //  "other" is left empty, so its case rule is found from its own name rather than an entry
    mkdirSync(join(dir, 'other'))
    writeFileSync(join(web, 'Probe.txt'), 'probe\n')
    writeFileSync(join(dir, 'route.txt'), 'route uri=/files/ dir=other\nroute uri=/\n')
    writeFileSync(join(dir, 'driver.c'), DRIVER)

    const variant = join(web, 'pROBE.TXT')
    const insensitive = existsSync(variant) && statSync(variant).ino == statSync(join(web, 'Probe.txt')).ino

    function run(exe: string, ...args: string[]): {status: number, output: string} {
        let cmd = new Cmd([exe, ...args].join(' '), {dir})
        return {status: cmd.status, output: String(cmd.error) + String(cmd.response)}
    }

    async function freePort(): Promise<number> {
        const probe = createServer()
        await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
        const port = (probe.address() as any).port
        await new Promise<void>(resolve => probe.close(() => resolve()))
        return port
    }

    for (const setting of [0, 1]) {
        const exe = join(dir, `driver-${setting}`)
        const build = compile(exe, `-DME_GOAHEAD_CASE_INSENSITIVE_FS=${setting}`, join(dir, 'driver.c'))
        ttrue(build.status == 0, `Cannot build the driver at ME_GOAHEAD_CASE_INSENSITIVE_FS=${setting}:\n` +
            String(build.error))
        if (build.status != 0) {
            continue
        }
        const expected = (setting == 0 && insensitive) ? 2 : 0
        const check = run(exe, 'check')
        ttrue(check.output.includes(`problems=${expected}`), `ME_GOAHEAD_CASE_INSENSITIVE_FS=${setting} on a ` +
            `${insensitive ? 'case-insensitive' : 'case-sensitive'} directory must report ${expected} problems:\n` +
            check.output)
        const warned = /ignores letter case[^\n]*ME_GOAHEAD_CASE_INSENSITIVE_FS=1/.test(check.output)
        ttrue(warned == (expected > 0), `the warning must ${expected ? '' : 'not '}be given:\n` + check.output)
        if (expected) {
            ttrue(check.output.includes('"web"') && check.output.includes('"other"'),
                'the warning must name the documents directory and the route dir=:\n' + check.output)

            const strict = run(exe, 'listen', `http://127.0.0.1:${await freePort()}`)
            ttrue(strict.status != 0 && /Strict mode: refusing/.test(strict.output),
                'strict mode must refuse to start:\n' + strict.output)
            const loose = run(exe, 'nostrict', `http://127.0.0.1:${await freePort()}`)
            ttrue(loose.status == 0 && /ignores letter case/.test(loose.output),
                'without strict mode the server must start with the warning:\n' + loose.output)
        }
    }
    rmSync(dir, {recursive: true, force: true})
}
