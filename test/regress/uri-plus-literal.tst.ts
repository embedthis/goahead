/*
    uri-plus-literal.tst - '+' is an ordinary character in a URI path
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function rawGet(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: x\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

let raw = await rawGet('/plus+file.txt')
ttrue(raw.contains('200 OK'), 'a path containing a literal + must be served')
ttrue(raw.contains('literal-plus'), 'a literal + in a path must name the file that has one')
ttrue(!raw.contains('real-space'), 'a + in a path must not be decoded to a space')

raw = await rawGet('/plus%20file.txt')
ttrue(raw.contains('200 OK'), 'a percent-encoded space must still be decoded')
ttrue(raw.contains('real-space'), '%20 must name the file whose name has a space')

raw = await rawGet('/plus%2Bfile.txt')
ttrue(raw.contains('literal-plus'), '%2B and a literal + must name one file')

raw = await rawGet('/plus+file.txt')
ttrue(!raw.contains('real-space'), 'the two files must not alias')

if (thas('ME_GOAHEAD_CGI')) {
    raw = await rawGet('/cgi-bin/cgitest?plusvar=a+b')
    ttrue(raw.contains('200 OK'), 'the CGI must run')
    ttrue(raw.contains('CGI_plusvar=a b'), 'a + in the query string must still decode to a space')
    ttrue(raw.contains('QUERY_STRING=plusvar=a+b'), 'QUERY_STRING must carry the raw, undecoded value')
}
