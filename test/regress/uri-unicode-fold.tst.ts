/*
    uri-unicode-fold.tst - An NFD spelling must not defeat route authorization

    APFS and HFS+ resolve the NFC and NFD spellings of a name to the same file, so a byte-exact
    route compare let /cafe%CC%81/x.txt fall through to the unauthenticated catch-all route while
    open() served the document behind the protected /café/ route. This axis is independent of case
    and works on a case-sensitive volume too.

    The fix rejects a decomposed path when the request path is created, so the NFD spelling never
    reaches routing or open() on any platform. The fixture web/café/x.txt is created with explicit
    NFC bytes by prep.sh.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)

async function rawGet(uri: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('GET ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

/*
    Baseline: the NFC spelling matches the protected route and demands credentials.
 */
let response = await rawGet('/caf%C3%A9/x.txt')
ttrue(status(response) == 401)
ttrue(!response.contains('cafe-secret'))

/*
    The NFD spelling of the same name is rejected as non-canonical. It must never reach the
    catch-all route, and must never disclose the document.
 */
response = await rawGet('/cafe%CC%81/x.txt')
ttrue(status(response) == 400)
ttrue(!response.contains('cafe-secret'))

/*
    A combining mark anywhere in the path is rejected, in any of the composable ranges: Latin
    (U+0301), Hebrew (U+05B0), Arabic (U+064E), kana voicing (U+3099) and Hangul jamo (U+1100).
 */
for (let uri of ['/cafe%CC%81.html', '/a%D6%B0/index.html', '/a%D9%8E/index.html',
                 '/a%E3%82%99.html', '/%E1%84%80/index.html']) {
    response = await rawGet(uri)
    ttrue(status(response) == 400)
}

/*
    Malformed UTF-8 has no single interpretation, so it has no canonical form either.
 */
for (let uri of ['/%C3/index.html', '/%80%80/index.html', '/caf%C3%A9%CC/x.txt']) {
    response = await rawGet(uri)
    ttrue(status(response) == 400)
}

/*
    Negative case: a precomposed non-ASCII path is still valid. It must route normally rather than
    be rejected, so the rejection is not simply "no non-ASCII allowed".
 */
response = await rawGet('/caf%C3%A9/missing.txt')
ttrue(status(response) == 401)

response = await rawGet('/index.html')
ttrue(status(response) == 200)
