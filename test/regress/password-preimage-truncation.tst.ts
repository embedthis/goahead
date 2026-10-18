/*
    password-preimage-truncation.tst - A password that does not fit the preimage is not verified
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

const LONG_USER = 'u'.repeat(90)

function basic(user: string, password: string): string {
    return Buffer.from(user + ':' + password).toString('base64')
}

async function get(target: string, credentials: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\n' +
                  'Authorization: Basic ' + credentials + '\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    /*
        Reject legacy hashes whose truncated input omitted the password.
     */
    for (let password of ['correcthorse', 'wrong', '', 'anything-at-all']) {
        let raw = await get('/auth/basic/basic.html', basic(LONG_USER, password))
        ttrue(status(raw) == '401', 'an account whose password does not fit the preimage must be refused, ' +
              `password "${password}" got ${status(raw)}`)
    }

    let raw = await get('/auth/basic/basic.html', basic('joshua', 'pass1'))
    ttrue(status(raw) == '200', `a normal account must still authenticate, got ${status(raw)}`)

    raw = await get('/auth/basic/basic.html', basic('joshua', 'wrongpass'))
    ttrue(status(raw) == '401', `a wrong password must still be refused, got ${status(raw)}`)

    raw = await get('/auth/basic/basic.html', basic('nosuchuser', 'pass1'))
    ttrue(status(raw) == '401', 'an unknown user must still be refused')
}
