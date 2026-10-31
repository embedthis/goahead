/*
    cgi-header-nul-byte.tst - A NUL byte inside a CGI header block must not turn the response into a
    200 whose body is the header text.

    parseCgiHeaders located the end of the header block with string functions, so a NUL byte inside
    a header line hid the blank line from the scan. The block was then treated as "no headers yet",
    and once the program exited the gateway served default 200 text/plain headers with the whole
    output, Status line and all, as the body. A CGI that answered 403 with a cookie was delivered as
    a 200 whose body printed "Status: 403". A response the gateway cannot tokenize is answered on the
    gateway's own behalf with 500, as a bare CR or a bad Status already is.

    The program is a shell script written here because cgitest cannot emit a NUL from a C string.
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'
import { chmodSync, rmSync, writeFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TEST = fileURLToPath(new URL('..', import.meta.url))
const PROGRAM = TEST + 'cgi-bin/nul-header'

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

if (!thas('ME_GOAHEAD_CGI') || process.platform == 'win32') {
    tskip('CGI is not in this build, or no shell CGI on Windows')
} else {
    //  A 403 with a cookie, and a NUL inside a header value before the blank line
    writeFileSync(PROGRAM, "#!/bin/sh\nprintf 'Status: 403\\nX-Probe: a\\0b\\nSet-Cookie: id=1\\n\\nDenied\\n'\n")
    chmodSync(PROGRAM, 0o755)
    try {
        let raw = await get('/cgi-bin/nul-header')
        let status = raw.split('\r\n')[0]
        let body = raw.slice(raw.indexOf('\r\n\r\n') + 4)
        ttrue(!/^HTTP\/1\.1 200 /.test(status), `a header block holding a NUL must not be served as 200, got [${status}]`)
        ttrue(/^HTTP\/1\.1 500 /.test(status), `an untokenizable header block is a malformed response, got [${status}]`)
        ttrue(!body.contains('Status: 403'), 'the CGI header text must not become the response body')
        ttrue(!/^set-cookie:/im.test(raw), 'no header of a malformed response may be forwarded')
    } finally {
        rmSync(PROGRAM, { force: true })
    }
}
