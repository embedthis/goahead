/* Framing and hop-by-hop headers from a CGI program are not passed to the client */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

//  getArgv descapes the switch string once and -r descapes again, so encode every byte twice
function doubleEncode(raw: string): string {
    return raw.split('').map(c => '%25' + c.charCodeAt(0).toString(16).padStart(2, '0')).join('')
}

async function cgiRaw(raw: string): Promise<string> {
    let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + program + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nSWITCHES: -r%20' +
        doubleEncode(raw) + '\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function headerLines(response: string, name: string): string[] {
    let head = response.slice(0, response.indexOf('\r\n\r\n'))
    return head.split('\r\n').filter(line => line.toLowerCase().startsWith(name.toLowerCase() + ':'))
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    let response = await cgiRaw('Content-Type: text/plain\nContent-Length: 5\nTransfer-Encoding: chunked\n' +
        'Connection: cgi-token\nKeep-Alive: timeout=999\nUpgrade: cgi-proto\nTE: trailers\nTrailer: X-Cgi\n' +
        'Proxy-Connection: cgi\nX-Kept: yes\n\nhello')
    ttrue(/^HTTP\/1\.1 200/.test(response), 'the CGI response is delivered')
    ttrue(headerLines(response, 'Transfer-Encoding').length == 0, 'a CGI Transfer-Encoding must not be passed through')
    ttrue(headerLines(response, 'Content-Length').length == 1, 'the response carries one Content-Length')
    let connection = headerLines(response, 'Connection')
    ttrue(connection.length == 1 && !connection[0].includes('cgi-token'), 'only the server writes Connection')
    for (let name of ['Keep-Alive', 'Upgrade', 'TE', 'Trailer', 'Proxy-Connection']) {
        ttrue(headerLines(response, name).length == 0, `a CGI ${name} header must not be passed through`)
    }
    ttrue(headerLines(response, 'X-Kept').length == 1, 'an end-to-end header is still passed through')
    ttrue(response.endsWith('\r\n\r\nhello'), 'the body is framed by the server')
}
