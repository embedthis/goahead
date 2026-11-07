/*
    credential-format-doc.tst - The shipped credential file and the gopass man page document the stored credential
    formats, which hash Digest needs, and how the file must be protected
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')

let auth = readFileSync(join(ROOT, 'src', 'auth.txt'), 'utf8')
tcontains(auth, 'BF2:', 'auth.txt must show the current Blowfish record format')
ttrue(/MD5[^\n]*Digest|Digest[^\n]*MD5/.test(auth), 'auth.txt must say Digest authentication needs an MD5 record')
tcontains(auth, 'password equivalent', 'auth.txt must say an MD5 record is a password equivalent')
tcontains(auth, '0600', 'auth.txt must say the file is to be readable by the server account only')
ttrue(!/^#\s+user name=[^\n]*password=BF1:/m.test(auth) || /BF2:/.test(auth.split('user name=')[1] || ''),
    'the first user example in auth.txt must not be a legacy BF1 record')

const MAN = join(ROOT, 'doc', 'man')
if (existsSync(MAN)) {
    let txt = readFileSync(join(MAN, 'gopass.txt'), 'utf8')
    tcontains(txt, 'BF2', 'gopass.txt must name the hash format it writes')
    tcontains(txt, 'password equivalent', 'gopass.txt must say an MD5 hash is a password equivalent')
    tcontains(txt, 'ME_GOAHEAD_REALM', 'gopass.txt must say the realm must match the server build')
    tcontains(readFileSync(join(MAN, 'gopass.1'), 'utf8'), 'BF2', 'gopass.1 must name the hash format it writes')
}
