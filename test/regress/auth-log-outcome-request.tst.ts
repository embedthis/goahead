/*
    Failed Basic, Digest and form authentication, refused abilities and form logins and logouts are logged at the
    default level. A per-request Basic success is logged at level 2 only.
 */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawn } from 'node:child_process'
import { createServer, connect } from 'node:net'
import { join } from 'node:path'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

async function request(port: number, raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let data = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

const get = (path: string, extra = '') =>
    `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n${extra}\r\n`
const basic = (user: string, password: string) =>
    `Authorization: Basic ${Buffer.from(`${user}:${password}`).toString('base64')}\r\n`
const post = (path: string, form: string) =>
    `POST ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n` +
    `Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ${form.length}\r\n\r\n${form}`

async function withServer(level: number, body: (port: number, lines: () => string[]) => Promise<void>) {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    const server = spawn(join(dir, '../build/bin/goahead'),
        ['--no-strict', '--log', `stderr:${level}`, '--route', String(tget('TM_ROUTES')), 'web', `127.0.0.1:${port}`],
        {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
    let logs = ''
    server.stdout.on('data', data => logs += data.toString())
    server.stderr.on('data', data => logs += data.toString())
    server.on('error', error => ttrue(false, String(error)))
    try {
        let ready = false
        for (let i = 0; i < 50; i++) {
            if ((await request(port, get('/index.html'))).includes(' 200 ')) {
                ready = true
                break
            }
            await sleep(50)
        }
        ttrue(ready, `private test server at level ${level} started`)
        const mark = logs.length
        await body(port, () => logs.slice(mark).split(/\r?\n/).filter(line => line.includes('Authentication ')))
    } finally {
        const exited = new Promise<void>(resolve => server.once('exit', () => resolve()))
        server.kill()
        await exited
    }
}

//  Run one request and return the authentication lines it added
async function outcome(port: number, lines: () => string[], raw: string): Promise<string[]> {
    const before = lines().length
    await request(port, raw)
    await sleep(100)
    return lines().slice(before)
}

if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_LOGGING')) {
    tskip('Authentication or logging is not enabled')
} else {
    await withServer(0, async (port, lines) => {
        let added = await outcome(port, lines, get('/auth/basic/basic.html', basic('joshua', 'wrong')))
        ttrue(added.length === 1, 'a wrong Basic password writes one line at level 0')
        const expected = new RegExp('^goahead: 0: Authentication failed for user "joshua" from 127\\.0\\.0\\.1 on ' +
            '/auth/basic/ \\(basic\\): password does not match$')
        ttrue(expected.test(added[0] || ''), 'the line names the user, address, route, scheme and reason: ' + added[0])
        ttrue(!(added[0] || '').includes('wrong'), 'the line does not carry the password')

        added = await outcome(port, lines, get('/auth/basic/basic.html', basic('nobody', 'anything')))
        ttrue(added.length === 1 && added[0].includes('unknown user'), 'an unknown Basic user is logged as unknown')

        added = await outcome(port, lines, get('/auth/basic/basic.html'))
        ttrue(added.length === 0, 'the first challenge, with no credentials, is not logged')

        added = await outcome(port, lines, get('/auth/basic/basic.html', basic('joshua', 'pass1')))
        ttrue(added.length === 0, 'a Basic success is not logged at level 0')

        added = await outcome(port, lines, get('/auth/basic/admin/', basic('mary', 'pass2')))
        ttrue(added.length === 1 && added[0].includes('insufficient abilities') && added[0].includes('"mary"'),
            'a user without the ability is logged: ' + added[0])

        added = await outcome(port, lines, get('/auth/digest/digest.html',
            'Authorization: Digest username="joshua", realm="other", nonce="abc", uri="/auth/digest/digest.html", ' +
            'response="0123456789abcdef0123456789abcdef"\r\n'))
        ttrue(added.length === 1 && added[0].includes('(digest)') && added[0].includes('"joshua"'),
            'a refused Digest response is logged: ' + added[0])

        added = await outcome(port, lines, get('/auth/basic/basic.html', 'Authorization: Digest username="joshua"\r\n'))
        ttrue(added.length === 1 && added[0].includes('wrong authentication scheme'), 'the wrong scheme is logged')

        added = await outcome(port, lines, get('/auth/basic/basic.html', basic('a\x01b' + 'u'.repeat(200), 'pw')))
        ttrue(added.length === 1, 'a hostile user name writes one line')
        ttrue(!/[\x00-\x09\x0b-\x1f\x7f]/.test(added[0] || ''), 'control bytes in the user name are escaped')
        ttrue((added[0] || '').includes('a\\x01b'), 'the user name is written escaped')
        ttrue((added[0] || '').length < 250, 'the user name is truncated')

        added = await outcome(port, lines, post('/action/login', 'username=joshua&password=wrong'))
        ttrue(added.length === 1 && added[0].includes('(form)') && added[0].includes('failed') &&
            added[0].includes('"joshua"'), 'a failed form login is logged: ' + added[0])

        added = await outcome(port, lines, post('/action/login', 'username=joshua&password=pass1'))
        ttrue(added.length === 1 && added[0].includes('succeeded') && added[0].includes('(form)'),
            'a form login is logged: ' + added[0])

        added = await outcome(port, lines, get('/action/logout'))
        ttrue(added.length === 1 && added[0].includes('logout'), 'a form logout is logged: ' + added[0])

        added = await outcome(port, lines, `POST /auth/authSessionTest HTTP/1.1\r\nHost: localhost\r\n` +
            `Origin: http://evil.example\r\nConnection: close\r\nContent-Length: 0\r\n\r\n`)
        ttrue(added.length === 1 && added[0].includes('request from another site'),
            'a cross-site refusal is logged: ' + added[0])
    })

    await withServer(2, async (port, lines) => {
        const added = await outcome(port, lines, get('/auth/basic/basic.html', basic('joshua', 'pass1')))
        ttrue(added.length === 1 && added[0].includes('succeeded') && added[0].includes('"joshua"'),
            'a Basic success is logged at level 2: ' + added[0])
    })
}
