/*
    connection-token-list.tst - Connection is a #token list, not a single value
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function responseCount(raw: string): number {
    return raw.split('HTTP/1.1 ').length - 1 + raw.split('HTTP/1.0 ').length - 1
}

/*
    Pipeline a second request to verify connection persistence.
 */
function pipelined(version: string, connection: string): string {
    return 'GET /index.html ' + version + '\r\nHost: x\r\nConnection: ' + connection + '\r\n\r\n' +
        'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
}

let raw = await exchange(pipelined('HTTP/1.1', 'close, TE'))
ttrue(responseCount(raw) == 1, `"close, TE" must close the connection, got ${responseCount(raw)} responses`)

raw = await exchange(pipelined('HTTP/1.1', 'TE, close'))
ttrue(responseCount(raw) == 1, `"TE, close" must close the connection, got ${responseCount(raw)} responses`)

raw = await exchange(pipelined('HTTP/1.1', 'keep-alive, close'))
ttrue(responseCount(raw) == 1, `close must win over keep-alive in one list, got ${responseCount(raw)} responses`)

raw = await exchange(pipelined('HTTP/1.0', 'keep-alive, Upgrade'))
ttrue(responseCount(raw) == 2, `"keep-alive, Upgrade" must keep the connection, got ${responseCount(raw)} responses`)

raw = await exchange(pipelined('HTTP/1.1', 'close'))
ttrue(responseCount(raw) == 1, 'a bare close must still close the connection')

raw = await exchange(pipelined('HTTP/1.0', 'keep-alive'))
ttrue(responseCount(raw) == 2, 'a bare keep-alive must still keep the connection')


raw = await exchange(pipelined('HTTP/1.1', 'close\r\nConnection: keep-alive'))
ttrue(responseCount(raw) == 1, `close then a keep-alive line must close the connection, got ${responseCount(raw)}`)

raw = await exchange(pipelined('HTTP/1.1', 'keep-alive\r\nConnection: close'))
ttrue(responseCount(raw) == 1, `keep-alive then a close line must close the connection, got ${responseCount(raw)}`)

raw = await exchange(pipelined('HTTP/1.0', 'close\r\nConnection: keep-alive'))
ttrue(responseCount(raw) == 1, `HTTP/1.0 close then a keep-alive line must close, got ${responseCount(raw)}`)

/*
    Retain required fields while removing named hop-by-hop fields.
 */
if (thas('ME_GOAHEAD_CGI')) {
    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nX-Hop: secret\r\n' +
                         'Connection: close, X-Hop\r\n\r\n')
    ttrue(raw.contains('200 OK'), 'the CGI must run')
    ttrue(!raw.contains('HTTP_X_HOP='), 'a header named in Connection must not reach the CGI environment')

    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: keepme\r\n' +
                         'Connection: close, Host\r\n\r\n')
    ttrue(raw.contains('HTTP_HOST='), 'Connection must not be able to strip a header the message needs')

    //  No spelling of a protected field can strip it, and Origin is protected
    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nOrigin: http://evil.example\r\n' +
                         'X-Hop: secret\r\nConnection: close, content_length, transfer_encoding, origin, X-Hop\r\n' +
                         'Content-Length: 0\r\n\r\n')
    ttrue(raw.contains('200 OK'), 'the CGI must run')
    ttrue(raw.contains('HTTP_CONTENT_LENGTH='), 'Connection must not be able to strip Content-Length')
    ttrue(raw.contains('HTTP_ORIGIN='), 'Connection must not be able to strip Origin')
    ttrue(!raw.contains('HTTP_X_HOP='), 'a header named in Connection must still be stripped')
}

/*
    A cross-site request cannot pass the Origin gate by naming Origin in Connection.
 */
if (thas('ME_GOAHEAD_AUTH')) {
    const BASIC = 'Basic ' + Buffer.from('joshua:pass1').toString('base64')
    raw = await exchange('POST /basicaction/showTest HTTP/1.1\r\nHost: ' + ADDR + '\r\n' +
                         'Origin: http://evil.example\r\nAuthorization: ' + BASIC + '\r\n' +
                         'Connection: close, origin\r\nContent-Length: 0\r\n\r\n')
    ttrue(raw.startsWith('HTTP/1.1 403'), 'a cross-site POST naming origin in Connection must be refused:\n' +
          raw.slice(0, 200))
}
