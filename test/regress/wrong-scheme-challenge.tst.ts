/*
    wrong-scheme-challenge.tst.ts - A 401 for credentials of another scheme carries the route's challenge

    RFC 9110 15.5.2: a 401 response must carry a WWW-Authenticate challenge. When a request presents an
    Authorization header whose scheme is not the route's, websAuthenticate answers 401 through websError, which
    issues no challenge and ignores the route's 401 redirect. A client holding stale or foreign credentials for
    the realm (a browser whose cached Digest credentials outlive a route change to Basic, an API client that sends
    a Bearer token on every request) then gets an error page it cannot recover from, while the same request with
    no credentials at all gets the challenge or the login redirect. The denial must look like the no-credential
    denial: challenge for Basic and Digest routes, the route's 401 redirect where one is configured.
 */

import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const PORT = 25710
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

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

const get = (path: string, extra = '') =>
    `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n${extra}\r\n`
const basic = (user: string, password: string) =>
    `Authorization: Basic ${Buffer.from(`${user}:${password}`).toString('base64')}\r\n`

const status = (response: string) => Number((response.match(/^HTTP\/1\.1 (\d+)/) || [])[1])
const header = (response: string, name: string) =>
    (response.split('\r\n\r\n')[0].match(new RegExp(`^${name}: (.*)$`, 'mi')) || [])[1]

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    const server = startServer(join(dir, '../build/bin/goahead'),
        ['--no-strict', '--log', 'stderr:0', '--route', String(tget('TM_ROUTES')), '--auth', join(dir, 'auth.txt'),
            'web', `127.0.0.1:${PORT}`], {cwd: dir, stdio: ['ignore', 'ignore', 'ignore']})
    try {
        let ready = false
        for (let i = 0; i < 50 && !ready; i++) {
            ready = (await request(get('/index.html'))).includes(' 200 ')
            if (!ready) await sleep(50)
        }
        ttrue(ready, 'private test server started')

        //  Controls: the no-credential denial challenges, and the right scheme authenticates
        let r = await request(get('/auth/basic/basic.html'))
        ttrue(status(r) == 401 && /^Basic realm=/.test(header(r, 'WWW-Authenticate') || ''),
            'a Basic route challenges a request without credentials')
        r = await request(get('/auth/basic/basic.html', basic('joshua', 'pass1')))
        ttrue(status(r) == 200, 'Basic credentials authenticate on the Basic route')

        //  The defect: credentials of another scheme are refused without a challenge
        for (const foreign of ['Authorization: Digest username="joshua", realm="example.com"\r\n',
            'Authorization: Bearer 0123456789\r\n']) {
            r = await request(get('/auth/basic/basic.html', foreign))
            ttrue(status(r) == 401, 'credentials of another scheme are refused: ' + foreign.trim())
            ttrue(/^Basic realm=/.test(header(r, 'WWW-Authenticate') || ''),
                'the 401 carries the Basic challenge: ' + foreign.trim())
        }
        if (thas('ME_GOAHEAD_DIGEST')) {
            r = await request(get('/auth/digest/digest.html', basic('joshua', 'pass1')))
            ttrue(status(r) == 401 && /^Digest realm=/.test(header(r, 'WWW-Authenticate') || ''),
                'a Digest route answers Basic credentials with its Digest challenge')
        }

        //  A form route sends its configured 401 redirect, as it does for a request without credentials
        r = await request(get('/auth/form/index.html'))
        const expected = header(r, 'Location')
        ttrue(status(r) == 302 && /\/auth\/form\/login\.html$/.test(expected || ''),
            'a form route redirects a request without credentials to its login page')
        r = await request(get('/auth/form/index.html', basic('joshua', 'pass1')))
        ttrue(status(r) == 302 && header(r, 'Location') == expected,
            'a form route redirects a request with Basic credentials to the same login page')

        //  A route with a wildcard redirect applies it to the wrong-scheme denial as it does to any other
        r = await request(get('/wildauth/index.html', 'Authorization: Bearer 0123456789\r\n'))
        ttrue(status(r) == 302, 'the route redirect applies to a denial for credentials of another scheme')
    } finally {
        await stopServer(server)
    }
}
