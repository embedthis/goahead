/* A protected directory requested without its trailing slash is authorized, not redirected */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: ' + ADDR + '\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not in this build')
} else {
    /* route uri=/auth/basic/ auth=basic abilities=view guards web/auth/basic */
    let raw = await exchange('/auth/basic')
    ttrue(raw.startsWith('HTTP/1.1 401'), `the directory without a slash must be challenged, got [${raw.split('\r\n')[0]}]`)
    ttrue(!/^Location:/im.test(raw), 'no redirect may be issued before authorization')

    raw = await exchange('/auth/basic/')
    ttrue(raw.startsWith('HTTP/1.1 401'), 'the directory with a slash is challenged')

    raw = await exchange('/auth/nosuchdir')
    ttrue(raw.startsWith('HTTP/1.1 404'), 'an unprotected missing path is still 404')
}
