/*
    Cross-site HTTP credentials must authenticate without rotating or rebinding an existing form session.
    Same-origin requests may rotate the session.
 */

import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const url = new URL(tget('TM_HTTP') || 'http://127.0.0.1:18080')
const HOST = url.hostname
const PORT = parseInt(url.port || '80')
const MARY = 'Basic ' + Buffer.from('mary:pass2').toString('base64')

async function request(raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: HOST, port: PORT})
        let data = ''
        socket.setTimeout(5000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

const get = (path: string, extra = '') =>
    `GET ${path} HTTP/1.1\r\nHost: ${HOST}:${PORT}\r\nConnection: close\r\n${extra}\r\n`
const post = (path: string, form: string, extra = '') =>
    `POST ${path} HTTP/1.1\r\nHost: ${HOST}:${PORT}\r\nConnection: close\r\n${extra}` +
    `Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ${form.length}\r\n\r\n${form}`

const status = (response: string) => parseInt((response.match(/^HTTP\/1\.1 (\d+)/) || ['', '0'])[1])
const sessionCookie = (response: string) => {
    const match = response.match(/^[Ss]et-[Cc]ookie: (-goahead-session-=[^;\r\n]*)/m)
    return match ? match[1] : null
}

async function formLogin(): Promise<string> {
    const response = await request(post('/action/login', 'username=joshua&password=pass1'))
    ttrue(status(response) == 302, 'the form login succeeds')
    const cookie = sessionCookie(response)
    ttrue(cookie != null, 'the form login issues a session cookie')
    return cookie!
}

async function formLoggedIn(cookie: string): Promise<boolean> {
    return status(await request(get('/auth/form/index.html', `Cookie: ${cookie}\r\n`))) == 200
}

async function logout(cookie: string) {
    await request(post('/action/logout', '', `Cookie: ${cookie}\r\n`))
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    //  A cross-site navigation with the form cookie and another account's Basic credentials
    let cookie = await formLogin()
    ttrue(await formLoggedIn(cookie), 'the form session reaches the protected page')
    let response = await request(get('/auth/basic/basic.html',
        `Cookie: ${cookie}\r\nAuthorization: ${MARY}\r\nSec-Fetch-Site: cross-site\r\nSec-Fetch-Mode: navigate\r\n`))
    ttrue(status(response) == 200, 'verified credentials are still served on a cross-site navigation')
    ttrue(sessionCookie(response) == null, 'a cross-site navigation must not rotate the session: no Set-Cookie')
    ttrue(await formLoggedIn(cookie), 'the form session survives a cross-site navigation that carried credentials')
    await logout(cookie)

    //  Same-origin credentials rotate and cache the session.
    cookie = await formLogin()
    response = await request(get('/auth/basic/basic.html',
        `Cookie: ${cookie}\r\nAuthorization: ${MARY}\r\nSec-Fetch-Site: same-origin\r\n`))
    ttrue(status(response) == 200, 'a same-origin request with credentials is served')
    const rotated = sessionCookie(response)
    ttrue(rotated != null && rotated != cookie, 'a same-origin request with credentials rotates the session')
    ttrue(!(await formLoggedIn(cookie)), 'the rotated form session is gone')
    ttrue(status(await request(get('/auth/basic/basic.html', `Cookie: ${rotated}\r\n`))) == 200,
        'the rotated session is cached for the Basic route')
    await logout(rotated!)

    //  Requests without fetch metadata also rotate the session.
    cookie = await formLogin()
    response = await request(get('/auth/basic/basic.html', `Cookie: ${cookie}\r\nAuthorization: ${MARY}\r\n`))
    ttrue(status(response) == 200)
    const plain = sessionCookie(response)
    ttrue(plain != null && plain != cookie, 'a request without fetch metadata rotates the session')
    await logout(plain!)
}
