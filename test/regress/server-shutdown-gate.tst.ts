/*
    server-shutdown-gate.tst - A test server crash or sanitizer report fails the run, even at shutdown
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const CHECK = join(ROOT, 'test', 'utils', 'check-server.sh')
const FIXTURES = join(import.meta.dir, 'server-shutdown-gate')

function check(log: string, status: string) {
    return spawnSync('bash', [CHECK, join(FIXTURES, log), join(FIXTURES, status)], { encoding: 'utf8' })
}

if (process.platform == 'win32') {
    tskip('test/utils/check-server.sh is a POSIX shell script; the Unix runs verify it')
} else {
    ttrue(existsSync(CHECK), 'test/utils/check-server.sh must exist')

    let result = check('clean-log.txt', 'exit-0.txt')
    ttrue(result.status == 0, 'a clean log with exit status 0 must pass: ' + result.stderr)

    result = check('asan-log.txt', 'exit-0.txt')
    ttrue(result.status != 0, 'an AddressSanitizer report must fail')
    tcontains(result.stderr, 'in websClose', 'and print the report')

    result = check('ubsan-log.txt', 'exit-0.txt')
    ttrue(result.status != 0, 'an UndefinedBehaviorSanitizer runtime error must fail')
    tcontains(result.stderr, 'runtime error: index 64', 'and print the report')

    result = check('clean-log.txt', 'exit-1.txt')
    ttrue(result.status != 0, 'a nonzero server exit status must fail')
    tcontains(result.stderr, 'exited with status 1', 'and report the status')

    let setup = readFileSync(join(ROOT, 'test', 'setup.sh'), 'utf8')
    ttrue(/wait \$PID\s+STATUS=\$\?/.test(setup), 'test/setup.sh must wait for the server and keep its exit status')
    tcontains(setup, 'STATUSFILE="../server-exit.txt"', 'test/setup.sh must record the status beside server-log.txt')

    let make = readFileSync(join(ROOT, 'Makefile'), 'utf8')
    let gated = make.match(/check-server\.sh server-log\.txt server-exit\.txt server-orphans\.txt \|\| rc=1/g) || []
    ttrue(gated.length == 2, 'the Makefile test and test-asan targets must both run check-server.sh')
}
