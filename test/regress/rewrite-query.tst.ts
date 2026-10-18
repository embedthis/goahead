/* A rewrite to a URL with a query replaces the request query and its variables */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\nSwitches: -e\r\nConnection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function env(raw: string, name: string): string | null {
    let match = raw.match(new RegExp('<P>' + name + '=([^<\\r\\n]*)</P>'))
    return match ? match[1] : null
}

if (thas('ME_GOAHEAD_CGI')) {
    for (let target of ['/test/x?rewrite=/cgi-bin/cgitest?a=1', '/svcrewrite/?rewrite=%2Fcgi-bin%2Fcgitest%3Fa%3D1']) {
        let raw = await get(target)
        ttrue(env(raw, 'QUERY_STRING') == 'a=1', `${target}: QUERY_STRING must be the target's query, got ` +
              env(raw, 'QUERY_STRING'))
        ttrue(env(raw, 'REQUEST_URI') == '/cgi-bin/cgitest?a=1', `${target}: REQUEST_URI must be the target, got ` +
              env(raw, 'REQUEST_URI'))
        ttrue(env(raw, 'CGI_a') == '1', `${target}: the target's query must be parsed into variables`)
        ttrue(env(raw, 'CGI_rewrite') == null, `${target}: no variable of the original query may survive`)
    }

    let raw = await get('/svcrewrite/?rewrite=/cgi-bin/cgitest&b=2')
    ttrue(env(raw, 'QUERY_STRING') == 'rewrite=/cgi-bin/cgitest&b=2',
          `a target without a query must keep the original, got ${env(raw, 'QUERY_STRING')}`)
    ttrue(env(raw, 'REQUEST_URI') == '/cgi-bin/cgitest?rewrite=/cgi-bin/cgitest&b=2',
          `REQUEST_URI must carry the kept query, got ${env(raw, 'REQUEST_URI')}`)
    ttrue(env(raw, 'CGI_b') == '2', 'the kept query must still be parsed into variables')
}
