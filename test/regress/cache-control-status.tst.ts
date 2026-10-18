/*
    cache-control-status.tst - Cache-Control must be gated on status and on authorization

    websWriteHeaders() selected "Cache-Control: public, max-age=86400" from the request's file
    extension alone. wp->code was never consulted, so a 401 challenge, a 404 and a 500 for a URL
    ending in a listed extension were all marked publicly cacheable. One anonymous request poisons a
    shared cache with the 401; the cache then serves that 401 to clients that DO hold credentials,
    locking every protected asset out for the lifespan.

    Three defects in the same block, all asserted here:

      1. The status/authorization gate. Only a plain unauthenticated 200 or 304 may be public.
      2. Exactly one Cache-Control. The Set-Cookie loop emitted one no-cache="set-cookie" per
         cookie, and CGI/JST added a second, weaker header of their own after the public one.
      3. The unanchored extension match. "ext," was searched in the raw list, so any extension that
         is a tail of a listed entry matched - ".g" matched the "jpg" entry.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const httpUri = new Uri(HTTP)

async function request(uri: string, auth: string = null, extra: string = ''): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    let headers = 'GET ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n' + extra
    if (auth) {
        headers += 'Authorization: Basic ' + auth + '\r\n'
    }
    await s.write(headers + '\r\n')
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

async function postForm(uri: string, body: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('POST ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n' +
        'Content-Type: application/x-www-form-urlencoded\r\n' +
        'Content-Length: ' + body.length + '\r\n\r\n' + body)
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

function header(response: string, name: string): string {
    let match = response.match(new RegExp('^' + name + ':\\s*(.*)$', 'im'))
    return match ? match[1].trim() : null
}

/*
    Collect every Cache-Control header. The count matters as much as the value: a response carrying
    both "public, max-age=86400" and "no-cache" is decided by whichever the cache prefers.
 */
function cacheControls(response: string): string[] {
    let values: string[] = []
    let lines = response.split('\r\n')
    for (let i = 0; i < lines.length; i++) {
        if (lines[i] == '') {
            /* End of the header block - stop before scanning the body */
            break
        }
        let match = lines[i].match(/^Cache-Control:\s*(.*)$/i)
        if (match) {
            values.push(match[1].trim())
        }
    }
    return values
}

function hasDirective(response: string, directive: string): boolean {
    let values = cacheControls(response)
    for (let i = 0; i < values.length; i++) {
        if (values[i].indexOf(directive) >= 0) {
            return true
        }
    }
    return false
}

function isPublic(response: string): boolean {
    return hasDirective(response, 'public')
}

function isNoStore(response: string): boolean {
    return hasDirective(response, 'no-store')
}

/*
    joshua:pass1 - the only test user holding the "manage" ability required by /auth/basic/admin/.
 */
const JOSHUA = 'am9zaHVhOnBhc3Mx'

let response: string

if (thas('ME_GOAHEAD_AUTH')) {
    /*
        The reported defect. An anonymous request for a protected .js asset is challenged with a 401.
        That challenge is what a shared cache would store under the URL, so it must never be public and
        must carry no-store.
     */
    response = await request('/auth/basic/admin/secret.js')
    ttrue(status(response) == 401)
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))
    ttrue(cacheControls(response).length == 1)
    ttrue(response.contains('Vary:'))

    /*
        The same URL fetched with valid credentials returns 200, but the body is authorization-dependent
        and must not be stored by a shared cache either - the mirror-image poisoning, where a cached
        authenticated body is served to a client holding no credentials.
     */
    response = await request('/auth/basic/admin/secret.js', JOSHUA)
    ttrue(status(response) == 200)
    ttrue(response.contains('CACHING-PROTECTED-ASSET'))
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))
    ttrue(cacheControls(response).length == 1)
}

/*
    A 404 for a listed extension was public too. An error response says nothing durable about the
    URL, so caching it for the lifespan hides the resource once it exists.
 */
response = await request('/caching/no-such-file.js')
ttrue(status(response) == 404)
ttrue(!isPublic(response))
ttrue(isNoStore(response))

/*
    The feature must still work. A plain unauthenticated 200 for a listed extension is exactly the
    case ME_GOAHEAD_CLIENT_CACHE exists to serve, and stays public with a single header.
 */
response = await request('/caching/public.js')
ttrue(status(response) == 200)
ttrue(response.contains('CACHING-PUBLIC-ASSET'))
ttrue(isPublic(response))
ttrue(!isNoStore(response))
ttrue(cacheControls(response).length == 1)

/*
    The unanchored match. ".g" is a tail of the listed "jpg" entry, so the old
    strstr("css,gif,ico,jpg,js,png,", "g,") found it inside "jpg," and marked the response public.
    Comma-delimiting both sides confines the match to whole entries.
 */
response = await request('/caching/tail.g')
ttrue(status(response) == 200)
ttrue(response.contains('CACHING-TAIL-ASSET'))
ttrue(!isPublic(response))

/*
    A conditional request that revalidates must not be told no-store - that would instruct the
    client to drop the entry it just revalidated, defeating client caching entirely. A 304 for a
    listed extension is the same public response as the 200 it refreshes.
 */
response = await request('/caching/public.js', null, 'If-Modified-Since: Mon, 01 Jan 2035 00:00:00 GMT\r\n')
ttrue(status(response) == 304)
ttrue(!isNoStore(response))

if (thas('ME_GOAHEAD_JAVASCRIPT')) {
    /*
        A dynamic response must never be public even when its URL carries a listed extension, and must
        carry exactly one Cache-Control - the CGI and JST handlers used to append a second, weaker
        "no-cache" of their own after websWriteHeaders had already emitted "public, max-age".
     */
    response = await request('/test.jst')
    ttrue(status(response) == 200)
    ttrue(!isPublic(response))
    ttrue(cacheControls(response).length == 1)
}

if (thas('ME_GOAHEAD_CGI')) {
    /*
        A CGI response is dynamic and must carry the single no-store, private header. The CGI handler
        used to append a "Cache-Control: no-cache" of its own after websWriteHeaders had already written
        one, so a CGI URL ending in a listed extension was served both "public, max-age=86400" and
        "no-cache" and the cache picked whichever it preferred.
     */
    response = await request('/cgi-bin/cgitest')
    ttrue(status(response) == 200)
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))
    ttrue(cacheControls(response).length == 1)
}

/*
    Ordinary static content on an extension that is not in ME_GOAHEAD_CLIENT_CACHE must be left
    exactly as it was - no Cache-Control at all. The fix must not turn every unlisted 200 into
    no-store, which would be a silent caching regression for the common case.
 */
response = await request('/index.html')
ttrue(status(response) == 200)
ttrue(cacheControls(response).length == 0)

if (thas('ME_GOAHEAD_AUTH')) {
    /*
        Form login. The response sets a session cookie, so it must not be public and must carry exactly
        one Cache-Control - the Set-Cookie loop used to emit one no-cache="set-cookie" per cookie, and a
        further header if the extension matched. The Set-Cookie itself must survive the rewrite.
     */
    response = await postForm('/action/login', 'username=joshua&password=pass1')
    ttrue(status(response) == 302)
    let setCookie = header(response, 'Set-Cookie')
    ttrue(setCookie != null && setCookie.contains('-goahead-session-='))
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))
    ttrue(cacheControls(response).length == 1)

    /*
        The session-authenticated fetch. This is the case the status gate alone does not catch: form auth
        sets no Authorization header, so wp->authType is empty and only wp->username marks the response
        as authorization-dependent. /auth/form/session.js is a 200 on a listed extension with no
        Set-Cookie in the response, so testing wp->authType alone would publish a session-protected asset
        to every shared cache for the lifespan.
     */
    let cookie = setCookie.split(';')[0]
    response = await request('/auth/form/session.js', null, 'Cookie: ' + cookie + '\r\n')
    ttrue(status(response) == 200)
    ttrue(response.contains('CACHING-SESSION-ASSET'))
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))

    /*
        The same asset without the session cookie is redirected to the login form. A redirect is not a
        durable answer for the URL either, so it must not be public.
     */
    response = await request('/auth/form/session.js')
    ttrue(status(response) == 302)
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))

    /*
        Digest is the other credential-bearing scheme. Its challenge must be gated identically to Basic -
        the gate keys off wp->authType and wp->code, not off the scheme.
     */
    response = await request('/auth/digest/admin/')
    ttrue(status(response) == 401)
    ttrue(!isPublic(response))
    ttrue(isNoStore(response))
}
