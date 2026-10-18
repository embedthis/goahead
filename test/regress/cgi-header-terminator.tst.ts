/*
    cgi-header-terminator.tst - A CGI header block ends at its first blank line, whichever line ending it uses
 */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

// Encode twice for getArgv and cgitest's raw-output decoding.
function doubleEncode(raw: string): string {
    return raw.split('').map(c => '%25' + c.charCodeAt(0).toString(16).padStart(2, '0')).join('')
}

async function cgiRaw(raw: string): Promise<string> {
    let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + program + ' HTTP/1.0\r\nHost: localhost\r\nSWITCHES: -r%20' + doubleEncode(raw) + '\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function split(response: string): { status: number, headers: string, body: string } {
    let at = response.indexOf('\r\n\r\n')
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return { status: match ? Number(match[1]) : 0, headers: response.slice(0, at), body: response.slice(at + 4) }
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    let r = split(await cgiRaw('Content-Type: text/plain\n\nx\r\nSet-Cookie: sid=evil\r\nStatus: 302\r\n' +
                               'Location: http://evil.example/\r\n\r\n'))
    ttrue(r.status == 200, `echoed data must not set the status, got ${r.status}`)
    ttrue(!/set-cookie: sid=evil/i.test(r.headers), 'echoed data must not become a Set-Cookie header')
    ttrue(!/location:/i.test(r.headers), 'echoed data must not become a Location header')
    tcontains(r.body, 'Set-Cookie: sid=evil', 'the echoed lines must stay in the body')

    r = split(await cgiRaw('Content-Type: text/plain\r\nX-Probe: kept\r\n\r\nfirst\n\nX-Late: body\n'))
    ttrue(/x-probe: kept/i.test(r.headers), 'a CRLF header must be delivered')
    ttrue(!/x-late:/i.test(r.headers), 'a line after the CRLF blank line must stay in the body')
    tcontains(r.body, 'first\n\nX-Late: body', 'the body must be delivered whole')
}
