/*
    uri-winfold.tst - Win32 name folding must not defeat route authorization

    Win32 strips a trailing dot or space from the final path component and accepts an alternate
    data stream suffix and an 8.3 alias, so /big.jst., /big.jst%20, /big.jst+ and /big.jst::$DATA
    all missed the extensions=jst route while the file still opened. The request fell through to
    the unauthenticated catch-all bound to the file handler, which served the template source
    instead of executing it.

    The fix rejects these spellings when the request path is created, on every platform, so one
    canonical form holds everywhere. Asserted on all platforms; the disclosure itself is live only
    on the Windows runner.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)

async function rawGet(uri: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('GET ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
    let response = new ByteArray(131072)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

/*
    Baseline: the canonical spelling is executed by the JST handler, never served as source.
 */
let response = await rawGet('/big.jst')
ttrue(status(response) == 200)
ttrue(!response.contains('<%'))

/*
    Every Win32-foldable spelling of the same file is rejected as non-canonical, and none of them
    may return the template source.

    Trailing dot and trailing space (literal, %20-encoded, and via the "+" that websDecodeUrl maps
    to a space); the NTFS alternate data stream suffixes; and the 8.3 alias form.
 */
for (let uri of ['/big.jst.', '/big.jst%20', '/big.jst+', '/big.jst%2E',
                 '/big.jst::$DATA', '/big.jst:$i30:$INDEX_ALLOCATION',
                 '/BIG~1.JST', '/big~1.jst', '/sub~1/index.html']) {
    response = await rawGet(uri)
    ttrue(status(response) == 400)
    ttrue(!response.contains('<%'))
}

/*
    A folded directory component is rejected too, not just the final one.
 */
for (let uri of ['/sub./index.html', '/sub%20/index.html', '/su:b/index.html']) {
    response = await rawGet(uri)
    ttrue(status(response) == 400)
}

/*
    Negative cases: the rejection must not be over-broad. A leading dot, an interior dot, a space
    that is not trailing, and a "~" not followed by digits are all legitimate.
 */
response = await rawGet('/index.html')
ttrue(status(response) == 200)

response = await rawGet('/My%20Documents/index.html')
ttrue(status(response) != 400)

response = await rawGet('/~user/index.html')
ttrue(status(response) == 404)

response = await rawGet('/.hidden/index.html')
ttrue(status(response) == 404)
