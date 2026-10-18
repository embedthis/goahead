/* The showTest action and cgitest escape request data written into HTML */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const MARKUP = '%3Cb%3Ehi%3C%2Fb%3E'

// Use HTTP/1.0 to read the complete response through connection close.
async function send(method: string, target: string, headers: string = '', body: string = ''): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let request = method + ' ' + target + ' HTTP/1.0\r\nHost: localhost\r\n' + headers
    if (method == 'POST') {
        request += 'Content-Length: ' + body.length + '\r\n'
    }
    await s.write(request + '\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function body(response: string): string {
    return response.slice(response.indexOf('\r\n\r\n') + 4)
}

const FORM = 'Content-Type: application/x-www-form-urlencoded\r\n'

let response = await send('POST', '/action/showTest', FORM, 'v=' + MARKUP)
tcontains(response, 'v=&lt;b&gt;hi&lt;/b&gt;', 'showTest must escape a variable value')
ttrue(!body(response).includes('<b>'), 'showTest must not reflect markup')

response = await send('POST', '/action/showTest', FORM, MARKUP + '=1')
ttrue(!body(response).includes('<b>'), 'showTest must escape a variable name')

if (thas('ME_GOAHEAD_CGI')) {
    let program = '/cgi-bin/cgitest' + (process.platform == 'win32' ? '.exe' : '')

    response = await send('GET', program + '?q=' + MARKUP, 'X-Probe: <i>x</i>\r\n')
    tcontains(response, 'QVAR q=&lt;b&gt;hi&lt;/b&gt;', 'cgitest must escape a query variable')
    tcontains(response, '&lt;i&gt;x&lt;/i&gt;', 'cgitest must escape a header-derived environment variable')
    ttrue(!body(response).includes('<b>') && !body(response).includes('<i>'), 'cgitest must not reflect markup')

    response = await send('POST', program, FORM, 'p=' + MARKUP)
    tcontains(response, 'PVAR p=&lt;b&gt;hi&lt;/b&gt;', 'cgitest must escape a form variable')

    response = await send('POST', program, 'Content-Type: text/plain\r\n', '<b>raw</b>')
    tcontains(response, '&lt;b&gt;raw&lt;/b&gt;', 'cgitest must escape echoed post data')
    ttrue(!body(response).includes('<b>'), 'cgitest must not reflect posted markup')

    response = await send('GET', program + '?a=1%26b')
    tcontains(response, 'QUERY_STRING=a=1%26b', 'values without markup are written verbatim')
}
