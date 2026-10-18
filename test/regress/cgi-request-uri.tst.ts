/*
    cgi-request-uri.tst.ts - Raw REQUEST_URI and rejection of double-encoded separators
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function env(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '=([^<\\r\\n]*)'))
    return match ? match[1] : ''
}

if (thas('ME_GOAHEAD_CGI')) {
    let target = '/cgi-bin/cgitest/a%20b?x=1%262'
    let raw = await get(target)
    ttrue(status(raw) == '200', `cgitest must run, got ${status(raw)}`)
    ttrue(env(raw, 'REQUEST_URI') == target,
          `REQUEST_URI must be the target as sent, got [${env(raw, 'REQUEST_URI')}]`)
    ttrue(env(raw, 'PATH_INFO') == '/a b', `PATH_INFO stays decoded, got [${env(raw, 'PATH_INFO')}]`)

    raw = await get('http://127.0.0.1/cgi-bin/cgitest?y=2')
    ttrue(env(raw, 'REQUEST_URI') == '/cgi-bin/cgitest?y=2',
          `an absolute-form REQUEST_URI must be origin form, got [${env(raw, 'REQUEST_URI')}]`)
}

for (let path of ['/cgi-bin/cgitest/%252e%252e%252fetc', '/%252e%252e/index.html', '/a%252Fb', '/a%255cb']) {
    let raw = await get(path)
    ttrue(status(raw) == '400', `a double-encoded dot or separator in ${path} must be refused, got ${status(raw)}`)
}

let raw = await get('/a%2541.html')
ttrue(status(raw) == '404', `an encoded percent before other text is a plain name, got ${status(raw)}`)
