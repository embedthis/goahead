/*
    invariants-current.tst - The security invariants document names only open defects as open, cites tests that
    exist, and records the invariants behind the fixed authentication, privilege, rewrite, CGI and upload defects
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const DOC = join(ROOT, 'doc', 'architecture', 'security-invariants.md')
const TICKETS = join(ROOT, 'doc', 'issues', 'tickets')

if (!existsSync(DOC)) {
    tskip('the architecture documents are not in this tree')
} else {
    let text = readFileSync(DOC, 'utf8')

    //  A ticket cited as open must be open
    for (let m of text.matchAll(/^- Open: (\d{5})/gm)) {
        let ticket = join(TICKETS, `${m[1]}.md`)
        if (existsSync(ticket)) {
            let status = (readFileSync(ticket, 'utf8').match(/^status:\s*(\S+)/m) || ['', 'unknown'])[1]
            ttrue(!['closed', 'abandoned', 'archived'].includes(status),
                `the invariants cite ${m[1]} as open but its status is ${status}`)
        }
    }

    //  Every test the document cites exists
    for (let m of text.matchAll(/`(regress|route|auth|upload|session|leak|basic)\/([\w-]+)`/g)) {
        let base = join(ROOT, 'test', m[1], m[2])
        ttrue(existsSync(`${base}.tst.ts`) || existsSync(`${base}.tst.c`), `the cited test ${m[1]}/${m[2]} must exist`)
    }

    //  The code that enforces each fixed critical or high defect is named by an invariant
    const ENFORCERS: [string, string][] = [
        ['websLoginSession', 'a session is bound to the authentication protocol that created it'],
        ['WEBS_SESSION_AUTHTYPE', 'the session records its protocol'],
        ['websDropPrivileges', 'the privilege drop is complete and irreversible'],
        ['websRewriteRequest', 'a rewritten request is validated and routed again from the first route'],
        ['cgiExtensionAllowed', 'a CGI program must satisfy the route extensions'],
        ['validIsindexArg', 'ISINDEX arguments are plain words passed after an end-of-options marker'],
        ['appendWinArg', 'Windows CGI arguments are quoted by the CommandLineToArgvW rules'],
        ['validClientFilename', 'an upload client filename is one plain name and never a storage path'],
    ]
    for (let [symbol, why] of ENFORCERS) {
        tcontains(text, symbol, `the invariants must name ${symbol}: ${why}`)
    }
}
