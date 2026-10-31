/*
    gopass-credential-bound.tst.ts - gopass accepts exactly the credentials the server can verify

    The server hashes "username:realm:password" through a WEBS_MAX_PASSWORD_BUFFER buffer and refuses a
    credential that does not fit (makePasswordPreimage in src/auth.c). gopass bounds only the password, so for a
    long username it writes a credential, with either cipher, that can never authenticate: the account is
    created, the operator sees no error, and every login is refused as "credentials too long". gopass must
    refuse what the server refuses and accept what the server accepts.
 */

import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { connect } from 'node:net'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const PORT = 25711
const REALM = 'example.com'
const PASSWORD = 'a'.repeat(32)
/*
    WEBS_MAX_PASSWORD_BUFFER is 99: "username:realm:password" and its terminator must fit, so the three
    fields may total 96 characters. The realm is 11 characters and the password 32.
 */
const LONGEST_USER = 'u'.repeat(96 - REALM.length - PASSWORD.length)
const TOO_LONG_USER = LONGEST_USER + 'u'

const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

function gopass(args: string[]): { code: number | null, stdout: string, stderr: string } {
    const proc = Bun.spawnSync(['gopass', ...args])
    return {
        code: proc.exitCode,
        stdout: new TextDecoder().decode(proc.stdout),
        stderr: new TextDecoder().decode(proc.stderr),
    }
}

async function request(raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port: PORT})
        let data = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

const get = (path: string, user?: string) =>
    `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n` +
    (user ? `Authorization: Basic ${Buffer.from(`${user}:${PASSWORD}`).toString('base64')}\r\n` : '') + '\r\n'

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    const scratch = mkdtempSync(join(tmpdir(), 'gopass-bound-'))
    const authFile = join(scratch, 'auth.txt')
    const routeFile = join(scratch, 'route.txt')
    writeFileSync(authFile, 'role name=user abilities=view\n')
    writeFileSync(routeFile, 'route uri=/auth/basic/ auth=basic abilities=view\nroute uri=/\n')

    //  The longest credential the server verifies is accepted, with either cipher
    let r = gopass(['--file', authFile, '--password', PASSWORD, REALM, LONGEST_USER, 'user'])
    ttrue(r.code == 0, 'gopass accepts the longest credential the server verifies: ' + r.stderr)
    r = gopass(['--cipher', 'md5', '--password', PASSWORD, REALM, LONGEST_USER, 'user'])
    ttrue(r.code == 0, 'gopass accepts the longest credential the server verifies with md5: ' + r.stderr)

    //  The defect: one character more is a credential the server cannot verify, and gopass must refuse it
    r = gopass(['--password', PASSWORD, REALM, TOO_LONG_USER, 'user'])
    ttrue(r.code != 0, 'gopass refuses a credential one character longer than the server verifies')
    ttrue(!r.stdout.startsWith('BF2:'), 'no hash is produced for the credential the server refuses')
    r = gopass(['--cipher', 'md5', '--password', PASSWORD, REALM, TOO_LONG_USER, 'user'])
    ttrue(r.code != 0, 'gopass refuses the same credential with md5')
    ttrue(!/^[0-9a-f]{32}$/m.test(r.stdout), 'no md5 hash is produced for the credential the server refuses')

    //  The accepted credential authenticates: the bound gopass applies is the server's, not a tighter one
    const server = startServer(join(dir, '../build/bin/goahead'),
        ['--no-strict', '--log', 'stderr:0', '--route', routeFile, '--auth', authFile, 'web', `127.0.0.1:${PORT}`],
        {cwd: dir, stdio: ['ignore', 'ignore', 'ignore']})
    try {
        let ready = false
        for (let i = 0; i < 50 && !ready; i++) {
            ready = (await request(get('/index.html'))).includes(' 200 ')
            if (!ready) await sleep(50)
        }
        ttrue(ready, 'private test server started')
        ttrue((await request(get('/auth/basic/basic.html'))).startsWith('HTTP/1.1 401 '),
            'the route requires credentials')
        ttrue((await request(get('/auth/basic/basic.html', LONGEST_USER))).startsWith('HTTP/1.1 200 '),
            'the longest credential gopass accepts authenticates')
    } finally {
        await stopServer(server)
        rmSync(scratch, {recursive: true, force: true})
    }
}
