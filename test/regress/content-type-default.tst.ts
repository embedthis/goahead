/*
    content-type-default.tst - Every response with a body declares its type, and none may be sniffed
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function rawGet(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: x\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function headers(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return (end < 0 ? raw : raw.slice(0, end)).toLowerCase()
}

function types(raw: string): string[] {
    let found = []
    for (let line of headers(raw).split('\r\n')) {
        if (line.startsWith('content-type:')) {
            found.push(line.slice('content-type:'.length).trim())
        }
    }
    return found
}

async function checkType(target: string, expected: string, why: string) {
    let raw = await rawGet(target)
    let found = types(raw)
    ttrue(found.length == 1, `${why} must carry exactly one Content-Type, got ${found.length}`)
    ttrue(found[0] != null && found[0].startsWith(expected),
          `${why} must be ${expected}, got [${found[0]}]`)
    ttrue(headers(raw).contains('x-content-type-options: nosniff'), `${why} must forbid sniffing`)
}

await checkType('/index.html', 'text/html', 'a known extension')

await checkType('/noextension', 'application/octet-stream', 'a path with no extension')

await checkType('/unknown.zzz', 'application/octet-stream', 'an unknown extension')

await checkType('/nosuchfile.jpg', 'text/html', 'an error body')

await checkType('/dir', 'text/html', 'a redirect body')

if (thas('ME_GOAHEAD_JAVASCRIPT')) {
    await checkType('/test.jst', 'text/html', 'a JST response')
}

/*
    CGI sets Content-Type after websWriteHeaders; verify only one is sent.
 */
if (thas('ME_GOAHEAD_CGI')) {
    await checkType('/cgi-bin/cgitest', 'text/html', 'a CGI response')
}

let raw = await rawGet('/empty.html')
ttrue(raw.contains('200 OK'), 'an empty file must still be served')
ttrue(types(raw).length <= 1, 'an empty file must not carry two Content-Type headers')
