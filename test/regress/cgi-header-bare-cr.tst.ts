/* Only LF ends a CGI response header line */
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

    let r = split(await cgiRaw('X-Echo: a\rX-Second: carried\nContent-Type: text/html\n\nbody'))
    ttrue(!/^x-second:/im.test(r.headers), 'text after a bare CR must not become a header')
    ttrue(!/\r(?!\n)/.test(r.headers), 'no bare CR may reach the response headers')
    ttrue(r.status == 500 || /^content-type: text\/html$/im.test(r.headers),
          `a header after the bad line must not be dropped, got ${r.status}`)


    r = split(await cgiRaw('X-Echo: a\rX-Second: b\nStatus: 403\n\nwithheld'))
    ttrue(r.status != 200, `the CGI's Status must not be lost, got ${r.status}`)
    ttrue(r.body.indexOf('withheld') < 0, 'a body the CGI meant for a 403 must not be served as a 200')

    // A bare CR in an nph status line is not a line break either
    r = split(await cgiRaw('HTTP/1.1 200 OK\rX-Second: carried\nContent-Type: text/plain\n\nbody'))
    ttrue(!/^x-second:/im.test(r.headers), 'text after a bare CR in the status line must not become a header')


    r = split(await cgiRaw('Content-Type: text/plain\r\nX-Probe: kept\r\nStatus: 202\r\n\r\nbody'))
    ttrue(r.status == 202, `a CRLF Status must apply, got ${r.status}`)
    ttrue(/^x-probe: kept$/im.test(r.headers), 'a CRLF header must be delivered')
    tcontains(r.body, 'body', 'the body must be delivered')

    r = split(await cgiRaw('Content-Type: text/plain\r\r\nX-Probe: kept\r\r\n\nbody'))
    ttrue(r.status == 200, `CRs ahead of the LF are part of the line ending, got ${r.status}`)
    ttrue(/^x-probe: kept$/im.test(r.headers), 'a header ending in CR CR LF must be delivered')
}
