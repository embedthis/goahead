/*
    cgi-working-dir.tst - A CGI program starts in its route's directory, and the server's own directory is unchanged
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { mkdirSync, realpathSync, rmSync, symlinkSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

const TEST = fileURLToPath(new URL('..', import.meta.url))

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\nSwitches: -w\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}


function body(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    if (end < 0 || !/^transfer-encoding: *chunked/im.test(raw.slice(0, end))) {
        return raw
    }
    let data = ''
    let pos = end + 4
    while (pos < raw.length) {
        let eol = raw.indexOf('\r\n', pos)
        if (eol < 0) {
            break
        }
        let size = parseInt(raw.slice(pos, eol), 16)
        if (!(size > 0)) {
            break
        }
        data += raw.slice(eol + 2, eol + 2 + size)
        pos = eol + 2 + size + 2
    }
    return data
}


function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function cwd(raw: string): string {
    let match = body(raw).match(/<p>CWD=([^<\r\n]*)<\/p>/)
    return match ? match[1] : ''
}

if (thas('ME_GOAHEAD_CGI') && process.platform != 'win32') {
    rmSync(TEST + 'reldir', { recursive: true, force: true })
    mkdirSync(TEST + 'reldir')
    symlinkSync('../cgi-bin', TEST + 'reldir/relative')
    try {
        let home = realpathSync(TEST)
        let raw = await get('/relative/cgitest')
        ttrue(status(raw) == '200', `the CGI must run, got ${status(raw)}`)
        ttrue(cwd(raw) == realpathSync(TEST + 'reldir'),
              `the CGI must start in its route directory, got [${cwd(raw)}]`)


        raw = await get('/cgi-bin/cgitest')
        ttrue(status(raw) == '200', `the second CGI must run, got ${status(raw)}`)
        ttrue(cwd(raw) == home, `a route without dir= must start in the server directory, got [${cwd(raw)}]`)

        raw = await get('/index.html')
        ttrue(status(raw) == '200', `a static file must still resolve from the server directory, got ${status(raw)}`)
    } finally {
        rmSync(TEST + 'reldir', { recursive: true, force: true })
    }
}
