/* gopass refuses to replace a password file it cannot read, rather than writing one holding only the new user */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { chmodSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('gopass is built only with authentication')
} else if (process.platform == 'win32' || (process.getuid && process.getuid() === 0)) {
    tskip('Needs POSIX file permissions and a non-root account')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'gopass-unreadable-'))
    const auth = join(dir, 'auth.txt')
    try {
        writeFileSync(auth, 'role name=keeper abilities=keep\nuser name=keep password=2fd6e47ff9bb70c0465fd2f5c8e5305e roles=keeper\n')
        chmodSync(auth, 0o000)
        const result = spawnSync('gopass', ['--file', auth, '--password', 'secret', 'example.com', 'newuser', 'keeper'],
            { encoding: 'utf8' })
        chmodSync(auth, 0o600)
        ttrue(result.status != 0, `gopass must refuse an unreadable password file, got status ${result.status}`)
        const kept = readFileSync(auth, 'utf8')
        ttrue(kept.includes('name=keep '), 'the existing users survive')
        ttrue(!kept.includes('newuser'), 'the new user is not written over them')

        //  The same command against a readable file adds the user and keeps the others
        const added = spawnSync('gopass', ['--file', auth, '--password', 'secret', 'example.com', 'newuser', 'keeper'],
            { encoding: 'utf8' })
        ttrue(added.status == 0, `gopass updates a readable file, got status ${added.status} ${added.stderr}`)
        const updated = readFileSync(auth, 'utf8')
        ttrue(updated.includes('name=keep ') && updated.includes('name=newuser '), 'both users are present')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
