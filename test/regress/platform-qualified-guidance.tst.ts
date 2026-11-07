/*
    platform-qualified-guidance.tst - Protections the code applies on Unix-like systems only are stated with their
    platform in the deployment guide and the man page

    The secret-file mode warning is compiled under ME_UNIX_LIKE, and the log file's O_NOFOLLOW and 0640 mode
    exist only where the platform provides them. The documents must not promise them unconditionally.
 */
import { ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')

function bullet(text: string, title: string): string {
    let line = text.split('\n').find(l => l.includes(title)) || ''
    ttrue(line != '', `the guide has the "${title}" item`)
    return line
}

function section(text: string, from: string, to: string): string {
    let start = text.indexOf(from)
    let end = text.indexOf(to, start + 1)
    ttrue(start >= 0 && end > start, `the man page has a ${from.trim()} section`)
    return text.slice(start, end)
}

//  The code: the checks are platform-conditional, so the documents must say so
let fs = readFileSync(join(ROOT, 'src', 'fs.c'), 'utf8')
ttrue(/PUBLIC void websCheckSecretFile\([^)]*\)\s*\{\s*#if ME_UNIX_LIKE/.test(fs),
    'the secret-file mode check is compiled for Unix-like systems only')
let header = readFileSync(join(ROOT, 'src', 'goahead.h'), 'utf8')
ttrue(/#ifndef O_NOFOLLOW\s*\n\s*#define O_NOFOLLOW 0/.test(header),
    'O_NOFOLLOW is defined away where the platform lacks it, so the log is then opened through a link')

let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
let password = bullet(guide, 'Protect the password file')
ttrue(/(Unix-like|POSIX)[^\n]*warns at startup when a file holding passwords/.test(password),
    'the guide must say the password-file mode warning is given on Unix-like systems')
ttrue(/Windows/.test(password), 'the guide must say Windows makes no password-file mode check')
let logs = bullet(guide, 'Size and protect the log partition')
ttrue(/(Unix-like|POSIX)[^\n]*0640[^\n]*symbolic link/.test(logs),
    'the guide must say the log mode and link refusal apply on Unix-like systems')
ttrue(/Windows/.test(logs), 'the guide must say Windows lacks the log mode and link protections')

const MAN = join(ROOT, 'doc', 'man')
if (existsSync(MAN)) {
    let txt = section(readFileSync(join(MAN, 'goahead.txt'), 'utf8'), '       --log logSpec', '       --home')
    ttrue(/Unix/.test(txt) && /Windows/.test(txt), 'goahead.txt must qualify the --log mode and link protections by platform')
    let roff = section(readFileSync(join(MAN, 'goahead.1'), 'utf8'), '--log logSpec', '--home directory')
    ttrue(/Unix/.test(roff) && /Windows/.test(roff), 'goahead.1 must qualify the --log mode and link protections by platform')
}
