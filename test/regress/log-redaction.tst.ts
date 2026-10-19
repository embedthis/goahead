/*
    Release builds redact credentials, session identifiers, query strings and form values from trace output at every
    level. Debug builds trace full values. The build under test reports its type in the level 2 startup banner.
 */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { createServer, connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

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

if (!thas('ME_GOAHEAD_LOGGING') || !thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Logging, authentication or upload is not enabled')
} else {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))

    const server = startServer(join(dir, '../build/bin/goahead'),
        ['--no-strict', '--log', 'stderr:5', '--route', String(tget('TM_ROUTES')), 'web', `127.0.0.1:${port}`],
        {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
    let logs = ''
    server.stdout.on('data', data => logs += data.toString())
    server.stderr.on('data', data => logs += data.toString())
    server.on('error', error => ttrue(false, String(error)))

    try {
        let ready = false
        for (let i = 0; i < 50; i++) {
            if ((await request(port, 'GET /index.html HTTP/1.0\r\nHost: localhost\r\n\r\n')).includes(' 200 ')) {
                ready = true
                break
            }
            await sleep(50)
        }
        ttrue(ready, 'private test server started')
        const release = /BuildType:\s+Release/.test(logs)
        const debug = /BuildType:\s+Debug/.test(logs)
        ttrue(release || debug, 'the server reports its build type')

        const basic = Buffer.from('joshua:CANARY-PW-5f3').toString('base64')
        const get = (path: string, extra = '') =>
            `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n${extra}\r\n`

        //  Basic credentials and a session cookie
        await request(port, get('/auth/basic/basic.html',
            `Authorization: Basic ${basic}\r\nCookie: -goahead-session-=CANARY-SID-7a1\r\n`))

        //  A query string with a password
        await request(port, get('/index.html?user=bob&password=CANARY-QUERY-9c2'))

        //  A form login issues a session cookie in Set-Cookie
        const form = 'username=joshua&password=pass1'
        const response = await request(port, `POST /action/login HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n` +
            `Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ${form.length}\r\n\r\n${form}`)
        const sid = /-goahead-session-=([^;\r\n]+)/.exec(response)
        ttrue(sid != null, 'the login response sets a session cookie')

        //  A multipart text field
        const body = '--XX\r\nContent-Disposition: form-data; name="password"\r\n\r\nCANARY-FIELD-4d8\r\n--XX--\r\n'
        await request(port, `POST /action/uploadTest HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n` +
            `Content-Type: multipart/form-data; boundary=XX\r\nContent-Length: ${body.length}\r\n\r\n${body}`)
        await sleep(100)

        const canaries = ['CANARY-PW-5f3', basic, 'CANARY-SID-7a1', 'CANARY-QUERY-9c2', 'CANARY-FIELD-4d8']
        if (sid) canaries.push(sid[1].split('::').pop() || sid[1])
        if (release) {
            for (const canary of canaries) {
                ttrue(!logs.includes(canary), `a release build logs no ${canary.slice(0, 12)}`)
            }
            ttrue(/authorization: <redacted>/i.test(logs), 'the Authorization header name is traced without its value')
            ttrue(/\bcookie: <redacted>/i.test(logs), 'the Cookie header name is traced without its value')
            ttrue(/set-cookie: <redacted>/i.test(logs), 'the Set-Cookie header name is traced without its value')
            ttrue(logs.includes('/index.html?...'), 'the request line shows the path without the query string')
            ttrue(logs.includes('uploadFilter: form[password]'), 'the multipart field name is traced')
        } else if (debug) {
            ttrue(logs.includes(basic), 'a debug build traces the Authorization value')
            ttrue(logs.includes('CANARY-QUERY-9c2'), 'a debug build traces the query string')
            ttrue(logs.includes('CANARY-FIELD-4d8'), 'a debug build traces multipart field values')
            ttrue(/authorization: Basic/i.test(logs), 'header names are traced')
        }
        ttrue(logs.split('\n').every(line => !/^(GET|POST) /.test(line)), 'the raw request header block is not dumped')
    } finally {
        await stopServer(server)
    }
}
