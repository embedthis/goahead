/* A CGI response carries one Content-Type, the program's own */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

//  getArgv descapes the switch string once and -r descapes again, so encode every byte twice
function doubleEncode(raw: string): string {
    return raw.split('').map(c => '%25' + c.charCodeAt(0).toString(16).padStart(2, '0')).join('')
}

async function request(target: string, switches: string | null = null): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: localhost\r\n' +
        (switches != null ? 'SWITCHES: -r%20' + doubleEncode(switches) + '\r\n' : '') + '\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function types(response: string): string[] {
    let head = response.slice(0, response.indexOf('\r\n\r\n'))
    return head.split('\r\n').filter(line => /^content-type:/i.test(line)).map(line => line.slice(13).trim())
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'

    let found = types(await request(program + '/x.html', 'Content-Type: application/json\n\n{"a":1}'))
    ttrue(found.length == 1 && found[0] == 'application/json',
        `a CGI response must carry only the program's type, got ${JSON.stringify(found)}`)

    found = types(await request(program + '/x.html', 'X-Probe: 1\n\nplain'))
    ttrue(found.length == 1 && found[0] == 'text/plain',
        `a CGI response without a type must not take one from the URL, got ${JSON.stringify(found)}`)

    found = types(await request('/index.html'))
    ttrue(found.length == 1 && found[0] == 'text/html', `a document keeps its extension's type, got ${JSON.stringify(found)}`)

    found = types(await request('/no-such-document.html'))
    ttrue(found.length == 1 && found[0] == 'text/html', `an error page is HTML, got ${JSON.stringify(found)}`)
}
