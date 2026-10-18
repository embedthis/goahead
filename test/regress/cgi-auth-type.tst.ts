/* AUTH_TYPE is set only for a request the server authenticated */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { rmSync, symlinkSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TEST = fileURLToPath(new URL('..', import.meta.url))

async function get(target: string, credentials: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\nSwitches: -e\r\n' +
                  'Authorization: Basic ' + credentials + '\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function env(raw: string, name: string): string | null {
    let match = raw.match(new RegExp('<P>' + name + '=([^<\\r\\n]*)</P>'))
    return match ? match[1] : null
}

if (thas('ME_GOAHEAD_CGI')) {
    let raw = await get('/cgi-bin/cgitest', 'Zm9vOmJhcg==')
    ttrue(raw.contains('200 OK'), 'cgitest must run')
    ttrue(!env(raw, 'AUTH_TYPE'), `an unauthenticated route must not export the client's scheme, got ` +
          env(raw, 'AUTH_TYPE'))
    ttrue(!env(raw, 'REMOTE_USER'), 'an unauthenticated route has no REMOTE_USER')

    if (thas('ME_GOAHEAD_AUTH') && process.platform != 'win32') {
        rmSync(TEST + 'authcgi', { force: true })
        symlinkSync('cgi-bin', TEST + 'authcgi')
        try {
            //  joshua:pass1
            raw = await get('/authcgi/cgitest', 'am9zaHVhOnBhc3Mx')
            ttrue(raw.contains('200 OK'), `an authenticated CGI request must run, got [${raw.slice(0, 60)}]`)
            ttrue(env(raw, 'AUTH_TYPE') == 'basic', `AUTH_TYPE must be the route's scheme, got ${env(raw, 'AUTH_TYPE')}`)
            ttrue(env(raw, 'REMOTE_USER') == 'joshua', 'REMOTE_USER must be the authenticated user')
        } finally {
            rmSync(TEST + 'authcgi', { force: true })
        }
    }
}
