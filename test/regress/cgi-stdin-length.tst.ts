/* CONTENT_LENGTH is the size of what the CGI program reads on stdin */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { rmSync, symlinkSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TEST = fileURLToPath(new URL('..', import.meta.url))

async function post(target: string, contentType: string, body: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST ' + target + ' HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\nSwitches: -e -p\r\n' +
                  'Content-Type: ' + contentType + '\r\nContent-Length: ' + body.length + '\r\n\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return dechunk(response.toString())
}

//  Join a chunked body so a chunk boundary cannot split the text the checks below match
function dechunk(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    if (end < 0 || !/^transfer-encoding:\s*chunked/im.test(raw.slice(0, end))) {
        return raw
    }
    let body = '', pos = end + 4
    while (pos < raw.length) {
        let eol = raw.indexOf('\r\n', pos)
        let size = parseInt(raw.slice(pos, eol), 16)
        if (eol < 0 || !(size > 0)) {
            break
        }
        body += raw.slice(eol + 2, eol + 2 + size)
        pos = eol + 2 + size + 2
    }
    return raw.slice(0, end + 4) + body
}

function env(raw: string, name: string): string | null {
    let match = raw.match(new RegExp('<P>' + name + '=([^<\\r\\n]*)</P>'))
    return match ? match[1] : null
}

function postLength(raw: string): number {
    if (raw.contains('No Post Data Found')) {
        return 0
    }
    let match = raw.match(/Post Data (\d+) bytes found/)
    return match ? parseInt(match[1]) : -1
}

if (thas('ME_GOAHEAD_CGI') && thas('ME_GOAHEAD_UPLOAD') && process.platform != 'win32') {
    for (let dir of ['upcgi', 'rrcgi']) {
        rmSync(TEST + dir, { force: true })
        symlinkSync('cgi-bin', TEST + dir)
    }
    try {
        let body = '--XYZ\r\nContent-Disposition: form-data; name="f"; filename="stdin.txt"\r\n\r\nhello\r\n--XYZ--\r\n'
        let raw = await post('/upcgi/cgitest', 'multipart/form-data; boundary=XYZ', body)
        ttrue(raw.contains('200 OK'), `the upload CGI must run, got [${raw.slice(0, 60)}]`)
        ttrue(env(raw, 'CONTENT_LENGTH') == String(postLength(raw)),
              `CONTENT_LENGTH must be the stdin size on an upload route: ${env(raw, 'CONTENT_LENGTH')} against ` +
              postLength(raw))
        ttrue(env(raw, 'CONTENT_LENGTH') == '0', 'the parsed upload body is not on stdin')

        raw = await post('/rrcgi/cgitest', 'application/octet-stream', 'hello world')
        ttrue(raw.contains('200 OK'), `the rerouted CGI must run, got [${raw.slice(0, 60)}]`)
        ttrue(postLength(raw) == 11, `a rerouted request must deliver its body on stdin, got ${postLength(raw)}`)
        ttrue(env(raw, 'CONTENT_LENGTH') == '11', `CONTENT_LENGTH must be 11, got ${env(raw, 'CONTENT_LENGTH')}`)
    } finally {
        for (let dir of ['upcgi', 'rrcgi']) {
            rmSync(TEST + dir, { force: true })
        }
    }
}
