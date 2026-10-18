/* A blank first line is an empty CGI header block */
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
    for (let eol of ['\n', '\r\n']) {
        let label = eol == '\n' ? 'LF' : 'CRLF'
        let r = split(await cgiRaw(eol + 'X-From-Body: yes' + eol + 'Status: 302' + eol +
                                   'Location: /elsewhere' + eol + eol + 'rest'))
        ttrue(r.status == 200, `${label}: body lines must not set the status, got ${r.status}`)
        ttrue(!/^x-from-body:/im.test(r.headers), `${label}: a body line must not become a header`)
        ttrue(!/^location:/im.test(r.headers), `${label}: a body line must not become a Location header`)
        ttrue(/^content-type: text\/plain$/im.test(r.headers), `${label}: headerless output is text/plain`)
        tcontains(r.body, 'X-From-Body: yes', `${label}: the lines after the blank line stay in the body`)
    }

    // Control: a normal header block is still parsed
    let r = split(await cgiRaw('Content-Type: text/plain\nX-Probe: kept\n\nbody'))
    ttrue(/^x-probe: kept$/im.test(r.headers), 'a header before the first blank line must be delivered')
    tcontains(r.body, 'body', 'the body must be delivered')
}
