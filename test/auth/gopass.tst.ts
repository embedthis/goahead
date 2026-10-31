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

    //  Default Blowfish output uses BF2 to retain all password material.
    r = run(['--password', 'pass1', 'example.com', 'testuser', 'user'])
    ttrue(r.code == 0)
    ttrue(r.stdout.startsWith('BF2:'))

    r = run(['--password', 'a'.repeat(32), 'example.com', 'testuser', 'user'])
    ttrue(r.code == 0)
    ttrue(r.stdout.startsWith('BF2:'))

    //  Long usernames must not crowd the password out of the key.
    r = run(['--password', 'pass1', 'example.com', 'u'.repeat(42), 'user'])
    ttrue(r.code == 0)
    ttrue(r.stdout.startsWith('BF2:'))

    //  --rounds sets the key schedules stored in the hash; out-of-range or non-numeric counts are usage errors
    r = run(['--rounds', '256', '--password', 'pass1', 'example.com', 'testuser', 'user'])
    ttrue(r.code == 0 && r.stdout.startsWith('BF2:00256:'), `--rounds 256 must be stored, got [${r.stdout.trim()}]`)
    for (let bad of ['0', '65537', '12x', '-5']) {
        r = run(['--rounds', bad, '--password', 'pass1', 'example.com', 'testuser', 'user'])
        ttrue(r.code == 2, `--rounds ${bad} must be refused, got exit ${r.code}`)
    }

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

    //  An unknown cipher is an error, and nothing is printed or written
    let before = readFileSync(authFile, 'utf8')
    r = run(['--cipher', 'nosuch', '--file', authFile, '--password', 'pass1', 'example.com', 'carol', 'user'])
    ttrue(r.code == 2, `an unknown cipher must exit 2, got ${r.code}`)
    ttrue(r.stdout == '', 'an unknown cipher must print no hash')
    ttrue(readFileSync(authFile, 'utf8') == before, 'an unknown cipher must leave the auth file unchanged')

    //  A user or role name the auth file cannot hold as one token is refused, and the file is unchanged
    for (let [user, role] of [['bad user', 'user'], ['a=b', 'user'], ['a:b', 'user'], ['#c', 'user'], ['', 'user'],
                              ['dave', 'bad role'], ['dave', 'r=x'], ['dave', 'a,b']]) {
        r = run(['--file', authFile, '--password', 'pass1', 'example.com', user, role])
        ttrue(r.code == 2, `user [${user}] role [${role}] must be refused with exit 2, got ${r.code}`)
        ttrue(readFileSync(authFile, 'utf8') == before, `user [${user}] role [${role}] must leave the file unchanged`)
    }
    r = run(['--file', authFile, '--password', 'pass1', 'example.com', 'dave', 'user', 'admin'])
    ttrue(r.code == 0 && readFileSync(authFile, 'utf8').includes('user name=dave '), 'plain names are accepted')
    rmSync(dir, { recursive: true, force: true })
}
