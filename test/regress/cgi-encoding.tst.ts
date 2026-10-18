/*
    cgi-encoding.tst - How the URL, the query and an ISINDEX search reach a CGI program
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const CGI = '/cgi-bin/cgitest'

/* Use HTTP/1.0 to keep CGI output unframed; chunk boundaries can split the records checked below. */
async function request(target: string, body: string = ''): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let head = (body ? 'POST ' : 'GET ') + target + ' HTTP/1.0\r\nHost: x\r\nConnection: close\r\n'
    if (body) {
        head += 'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n'
    }
    await s.write(head + '\r\n' + body)
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

/*
    Read cgitest's <P>NAME=VALUE</P> records.
 */
function reported(raw: string, name: string): string | null {
    let escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
    let match = raw.match(new RegExp('<P>' + escaped + '=([^<]*)</P>', 'i'))
    return match ? match[1] : null
}

function expect(raw: string, name: string, value: string | null, why: string) {
    let actual = reported(raw, name)
    ttrue(actual === value, `${why}: ${name} expected ${JSON.stringify(value)}, got ${JSON.stringify(actual)}`)
}

if (thas('ME_GOAHEAD_CGI')) {
    let raw: string

    /*
        Path
     */
    for (let [path, info] of [
        ['/a+b', '/a+b'],
        ['/a%20b', '/a b'],
        ['/a%2Bb', '/a+b'],
        ['/a%2Fb', '/a/b'],
        ['/a%25b', '/a%b'],
        ['/caf%C3%A9', '/café'],
    ]) {
        raw = await request(CGI + path)
        ttrue(status(raw) == '200', `${path} must run the program, got ${status(raw)}`)
        expect(raw, 'PATH_INFO', info, `PATH_INFO for ${path}`)
        expect(raw, 'SCRIPT_NAME', CGI, `SCRIPT_NAME for ${path}`)
        expect(raw, 'REQUEST_URI', CGI + path, `REQUEST_URI for ${path}`)
    }

    raw = await request('/cgi-bin/cgi%74est')
    ttrue(status(raw) == '200', `an encoded program name must find the program, got ${status(raw)}`)
    expect(raw, 'SCRIPT_NAME', CGI, 'SCRIPT_NAME is decoded')
    expect(raw, 'REQUEST_URI', '/cgi-bin/cgi%74est', 'REQUEST_URI is as sent')

    for (let path of ['/a%01b', '/a%0Ab', '/a%00b', '/a%2500b', '/x.cgi%00.txt']) {
        raw = await request(CGI + path)
        ttrue(status(raw) == '400', `${path} must be refused before the program runs, got ${status(raw)}`)
        ttrue(reported(raw, 'PATH_INFO') === null, `${path} must not reach the program`)
    }

    /*
        Query: raw where the CGI specification says raw, decoded into CGI_ variables
     */
    raw = await request(CGI + '?na%6De=a+b%26c&x=%E2%82%AC')
    expect(raw, 'QUERY_STRING', 'na%6De=a+b%26c&x=%E2%82%AC', 'QUERY_STRING is as sent')
    expect(raw, 'REQUEST_URI', CGI + '?na%6De=a+b%26c&x=%E2%82%AC', 'REQUEST_URI keeps its query')
    expect(raw, 'CGI_name', 'a b&c', 'a key and value are decoded, with + as a space')
    expect(raw, 'CGI_x', '€', 'a multi-byte value is decoded')

    raw = await request(CGI + '?x=1&caf%C3%A9&fl%61g')
    expect(raw, 'CGI_café', null, 'a name outside the portable environment grammar is not exported')
    expect(raw, 'CGI_flag', '', 'a valueless key is decoded')
    expect(raw, 'CGI_caf%C3%A9', null, 'a valueless key must not keep its escapes')

    raw = await request(CGI + '?a=1&a=2')
    expect(raw, 'CGI_a', '1 2', 'repeated keys are joined')

    raw = await request(CGI + '?x=a%00b')
    expect(raw, 'CGI_x', 'a%00b', 'an encoded NUL is kept encoded')

    raw = await request(CGI + '?x=a%0Ab')
    expect(raw, 'CGI_x', 'a\nb', 'a newline in a value is kept')

    raw = await request(CGI + '?a%3Db=1&z=2')
    expect(raw, 'CGI_z', '2', 'the ordinary key beside it still arrives')
    ttrue(!raw.includes('<P>CGI_a=b'), 'a key decoding to hold "=" must not become a variable')

    raw = await request(CGI + '?x%0AFOO=bar&z=2')
    expect(raw, 'CGI_z', '2', 'the ordinary key beside it still arrives')
    ttrue(!raw.includes('FOO=bar</P>'), 'a key decoding to hold a newline must not become a variable')

    raw = await request(CGI + '?x=%28%29%20%7B%20echo%20hi%3B%20%7D')
    expect(raw, 'CGI_x', null, 'a "() {" value must not reach the environment')

    raw = await request(CGI + '?PATH=/evil')
    ttrue(!raw.includes('<P>PATH=/evil'), 'a query key must not set PATH')

    raw = await request(CGI + '?q=1', 'pn=a+b%26c&pm=%C3%A9')
    ttrue(raw.includes('PVAR pn=a b&c') && raw.includes('PVAR pm=é'), 'the form body must reach the program')
    expect(raw, 'CGI_pn', null, 'a form field must not be exported to the environment')
    expect(raw, 'CGI_q', '1', 'the query beside it is')

    /*
        ISINDEX
     */
    raw = await request(CGI + '?new%20york+city')
    ttrue(status(raw) == '200', `an ISINDEX search must run the program, got ${status(raw)}`)
    expect(raw, 'QUERY_STRING', 'new%20york+city', 'QUERY_STRING is as sent')
    expect(raw, 'ARG[1]', '--', 'terms follow the end-of-options marker')
    expect(raw, 'ARG[2]', 'new york', 'an encoded space stays inside its term')
    expect(raw, 'ARG[3]', 'city', 'the next term')
    expect(raw, 'ARG[4]', null, 'no further terms')

    raw = await request(CGI + '?a%2Bb+c')
    expect(raw, 'ARG[2]', 'a+b', 'an encoded + stays inside its term')
    expect(raw, 'ARG[3]', 'c', 'a literal + separates terms')

    raw = await request(CGI + '?+a++b+')
    expect(raw, 'ARG[2]', 'a', 'leading, doubled and trailing separators make no empty terms')
    expect(raw, 'ARG[3]', 'b', 'the second term')
    expect(raw, 'ARG[4]', null, 'no empty term at the end')

    raw = await request(CGI + '?caf%C3%A9+cr%C3%A8me')
    expect(raw, 'ARG[2]', 'café', 'a multi-byte term is decoded')
    expect(raw, 'ARG[3]', 'crème', 'a multi-byte term is decoded')

    raw = await request(CGI + '?a%00b')
    expect(raw, 'ARG[2]', 'a%00b', 'an encoded NUL is kept encoded in a term')

    for (let query of [
        'a%01b', 'a%0Ab', 'ok+b%09c',       // a control character anywhere in a term
        '%C3%A9lan',                        // a non-ASCII leading byte
        '%ADd',                             // soft hyphen, best-fit to '-' on Windows
        '%EF%BC%8Dd',                       // fullwidth hyphen, best-fit to '-' on Windows
        'a%3Db',
    ]) {
        raw = await request(CGI + '?' + query)
        ttrue(status(raw) == '400', `ISINDEX "${query}" must be refused, got ${status(raw)}`)
        expect(raw, 'ARG[1]', null, `no argv may be built for "${query}"`)
    }
}
