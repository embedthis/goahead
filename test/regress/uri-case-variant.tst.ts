/*
    uri-case-variant.tst - A case-variant URI must not defeat route authorization

    A case-variant URI must not select a different route than the file the kernel will open.
    On a case-insensitive file system (macOS, Windows, vfat) /AUTH/BASIC/basic.html and
    /auth/basic/basic.html name one file, so a byte-exact route compare let the case variant fall
    through to the unauthenticated catch-all route while open() still served the protected document.
    That one compare gates auth, abilities, form login and TLS enforcement.

    Asserted on all platforms: where the file system folds case the variant must be authorized like
    the exact-case spelling; where it does not, the variant names no file and must 404. Neither
    platform may return 200.
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
    Baseline: the exact-case spelling is protected, and ordinary routing still works.
 */
let response = await rawGet('/auth/basic/basic.html')
ttrue(status(response) == 401)

response = await rawGet('/index.html')
ttrue(status(response) == 200)

/*
    Case variants of a Basic-auth route. 401 where the file system folds, 404 where it does not.
 */
for (let uri of ['/AUTH/BASIC/basic.html', '/Auth/Basic/basic.html', '/auth/BASIC/basic.html']) {
    response = await rawGet(uri)
    ttrue(status(response) == 401 || status(response) == 404)
    ttrue(!response.contains('Hello basic world'))
}

/*
    Case variant of an abilities-gated route. "view" is not enough for /auth/basic/admin/.
 */
response = await rawGet('/AUTH/BASIC/ADMIN/index.html')
ttrue(status(response) == 401 || status(response) == 404)

/*
    Case variant of the TLS-enforcing route. Must redirect to https, never serve cleartext.
 */
response = await rawGet('/SECURE/index.html')
ttrue(status(response) == 302 || status(response) == 404)
ttrue(!response.contains('Hello secure world'))

/*
    Case variant of the form-login route. Must redirect to the login page, never serve content.
 */
response = await rawGet('/AUTH/FORM/index.html')
ttrue(status(response) != 200)

/*
    Negative case: the fold must not be over-broad. /auth/basicX/ is a different prefix, so it
    falls through to the catch-all and 404s on the missing file rather than demanding credentials.
 */
response = await rawGet('/auth/basicX/basic.html')
ttrue(status(response) == 404)
