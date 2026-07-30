/*
    gopass.tst - Unit tests for the gopass CLI tool (src/utils/gopass.c).
    Verifies the plaintext-length ceiling (WEBS_MAX_PASSWORD = 32) is enforced
    at the hash-creation entry point, matching the runtime check in auth.c.
 */

import { ttrue } from '@embedthis/testme'

function run(args: string[]): { code: number | null, stdout: string, stderr: string } {
    const proc = Bun.spawnSync(['gopass', ...args])
    return {
        code: proc.exitCode,
        stdout: new TextDecoder().decode(proc.stdout),
        stderr: new TextDecoder().decode(proc.stderr),
    }
}

//  Missing required args -> usage error (exit 2).
let r = run([])
ttrue(r.code == 2)

//  Short password, default (blowfish) cipher -> BF1 hash printed.
r = run(['--password', 'pass1', 'example.com', 'testuser', 'user'])
ttrue(r.code == 0)
ttrue(r.stdout.startsWith('BF1:'))

//  Exact-boundary password (32 chars) with blowfish -> accepted.
//  Before the fix this failed whenever "user:realm:password" exceeded 32 bytes,
//  which it does for any realistic username + realm combination.
r = run(['--password', 'a'.repeat(32), 'example.com', 'testuser', 'user'])
ttrue(r.code == 0)
ttrue(r.stdout.startsWith('BF1:'))

//  Over-boundary password (33 chars) -> rejected with exit 3.
r = run(['--password', 'a'.repeat(33), 'example.com', 'testuser', 'user'])
ttrue(r.code == 3)
ttrue(r.stderr.includes('maximum length'))

//  Exact-boundary password with MD5 cipher -> accepted.
r = run(['--cipher', 'md5', '--password', 'a'.repeat(32), 'example.com', 'testuser', 'user'])
ttrue(r.code == 0)
ttrue(/^[0-9a-f]{32}$/m.test(r.stdout.trim()))

//  Over-boundary with MD5 cipher -> same plaintext check applies -> exit 3.
r = run(['--cipher', 'md5', '--password', 'a'.repeat(33), 'example.com', 'testuser', 'user'])
ttrue(r.code == 3)
ttrue(r.stderr.includes('maximum length'))

//  Pathologically long password -> still rejected cleanly, no crash.
r = run(['--password', 'a'.repeat(4096), 'example.com', 'testuser', 'user'])
ttrue(r.code == 3)
