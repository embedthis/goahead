/*
    Honor an nph status line even without header fields. A shell fixture emits the bare status line.
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'
import { chmodSync, rmSync, writeFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TEST = fileURLToPath(new URL('..', import.meta.url))
const STATUS_ONLY = TEST + 'cgi-bin/nph-status-only'
const STATUS_HEADER = TEST + 'cgi-bin/nph-status-header'
const HEADERLESS = TEST + 'cgi-bin/nph-headerless'

async function get(target: string, method = 'GET'): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(method + ' ' + target + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function statusLine(raw: string): string {
    return raw.split('\r\n')[0]
}

function body(raw: string): string {
    return raw.slice(raw.indexOf('\r\n\r\n') + 4)
}

if (!thas('ME_GOAHEAD_CGI') || process.platform == 'win32') {
    tskip('CGI is not in this build, or no shell CGI on Windows')
} else {
    writeFileSync(STATUS_ONLY, "#!/bin/sh\nprintf 'HTTP/1.1 403 Forbidden\\r\\n\\r\\nDenied\\n'\n")
    writeFileSync(STATUS_HEADER, "#!/bin/sh\nprintf 'HTTP/1.1 403 Forbidden\\r\\nContent-Type: text/plain\\r\\n\\r\\nDenied\\n'\n")
    writeFileSync(HEADERLESS, "#!/bin/sh\nprintf 'plain text\\r\\n\\r\\nmore\\n'\n")
    for (let program of [STATUS_ONLY, STATUS_HEADER, HEADERLESS]) {
        chmodSync(program, 0o755)
    }
    try {
        //  Control: the same status line followed by a header field is honoured
        let raw = await get('/cgi-bin/nph-status-header')
        ttrue(/^HTTP\/1\.1 403 /.test(statusLine(raw)), `an nph status with a header must be applied, got [${statusLine(raw)}]`)

        //  A bare nph status line is a header block on its own
        raw = await get('/cgi-bin/nph-status-only')
        ttrue(/^HTTP\/1\.1 403 /.test(statusLine(raw)), `a bare nph status must be applied, got [${statusLine(raw)}]`)
        ttrue(!body(raw).contains('HTTP/1.1 403'), 'the status line must not become the response body')
        ttrue(body(raw).contains('Denied'), 'the program body must still be delivered')

        raw = await get('/cgi-bin/nph-status-only', 'HEAD')
        ttrue(/^HTTP\/1\.1 403 /.test(statusLine(raw)), `HEAD must see the nph status too, got [${statusLine(raw)}]`)
        ttrue(body(raw) == '', 'a HEAD response carries no body')

        //  Unchanged: output with neither a status line nor a header field is served as headerless text
        raw = await get('/cgi-bin/nph-headerless')
        ttrue(/^HTTP\/1\.1 200 /.test(statusLine(raw)), `headerless output stays 200, got [${statusLine(raw)}]`)
        ttrue(/^content-type: text\/plain/im.test(raw), 'headerless output stays text/plain')
        ttrue(body(raw).contains('plain text'), 'headerless output is delivered as the body')
    } finally {
        for (let program of [STATUS_ONLY, STATUS_HEADER, HEADERLESS]) {
            rmSync(program, { force: true })
        }
    }
}
