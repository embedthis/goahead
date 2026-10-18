/*
    gopass.tst - Unit tests for the gopass CLI tool (src/utils/gopass.c).
    Verifies the plaintext-length ceiling (WEBS_MAX_PASSWORD = 32) is enforced
    at the hash-creation entry point, matching the runtime check in auth.c.
 */

import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

//  gopass reports why it refused through the logging layer, so a build without logging refuses silently
const LOGGING = thas('ME_GOAHEAD_LOGGING')

function run(args: string[]): { code: number | null, stdout: string, stderr: string } {
    const proc = Bun.spawnSync(['gopass', ...args])
    return {
        code: proc.exitCode,
        stdout: new TextDecoder().decode(proc.stdout),
        stderr: new TextDecoder().decode(proc.stderr),
    }
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled, so gopass is a stub')
} else {
    //  Missing required args -> usage error (exit 2).
    let r = run([])
    ttrue(r.code == 2)

    //  Short password, default (blowfish) cipher -> BF2 hash printed. New hashes must never be
    //  written in the legacy BF1 format, whose key schedule silently drops password material once
    //  the username and realm are long enough.
    r = run(['--password', 'pass1', 'example.com', 'testuser', 'user'])
    ttrue(r.code == 0)
    ttrue(r.stdout.startsWith('BF2:'))

    //  Exact-boundary password (32 chars) with blowfish -> accepted.
    //  Before the fix this failed whenever "user:realm:password" exceeded 32 bytes,
    //  which it does for any realistic username + realm combination.
    r = run(['--password', 'a'.repeat(32), 'example.com', 'testuser', 'user'])
    ttrue(r.code == 0)
    ttrue(r.stdout.startsWith('BF2:'))

    //  A username long enough to have pushed the password out of the BF1 key window must still
    //  produce a usable hash in the new format.
    r = run(['--password', 'pass1', 'example.com', 'u'.repeat(42), 'user'])
    ttrue(r.code == 0)
    ttrue(r.stdout.startsWith('BF2:'))

    //  Empty password -> rejected with exit 3, with either cipher.
    r = run(['--password', '', 'example.com', 'testuser', 'user'])
    ttrue(r.code == 3)
    ttrue(!LOGGING || r.stderr.includes('must not be empty'))
    r = run(['--cipher', 'md5', '--password', '', 'example.com', 'testuser', 'user'])
    ttrue(r.code == 3)

    //  Over-boundary password (33 chars) -> rejected with exit 3.
    r = run(['--password', 'a'.repeat(33), 'example.com', 'testuser', 'user'])
    ttrue(r.code == 3)
    ttrue(!LOGGING || r.stderr.includes('maximum length'))

    //  Exact-boundary password with MD5 cipher -> accepted.
    r = run(['--cipher', 'md5', '--password', 'a'.repeat(32), 'example.com', 'testuser', 'user'])
    ttrue(r.code == 0)
    ttrue(/^[0-9a-f]{32}$/m.test(r.stdout.trim()))

    //  Over-boundary with MD5 cipher -> same plaintext check applies -> exit 3.
    r = run(['--cipher', 'md5', '--password', 'a'.repeat(33), 'example.com', 'testuser', 'user'])
    ttrue(r.code == 3)
    ttrue(!LOGGING || r.stderr.includes('maximum length'))

    //  Pathologically long password -> still rejected cleanly, no crash.
    r = run(['--password', 'a'.repeat(4096), 'example.com', 'testuser', 'user'])
    ttrue(r.code == 3)

    //  Updating an auth file stages the new contents beside it and leaves nothing else behind.
    let dir = mkdtempSync(join(tmpdir(), 'gopass-'))
    let authFile = join(dir, 'auth.txt')
    writeFileSync(authFile, 'role name=user abilities=view,\nuser name=alice password=x roles=user\n')
    r = run(['--file', authFile, '--password', 'pass1', 'example.com', 'bob', 'user'])
    ttrue(r.code == 0)
    let text = readFileSync(authFile, 'utf8')
    ttrue(text.includes('user name=bob '))
    ttrue(text.includes('user name=alice '))
    ttrue(readdirSync(dir).length == 1)
    rmSync(dir, { recursive: true, force: true })
}
