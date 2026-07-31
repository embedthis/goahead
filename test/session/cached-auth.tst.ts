/*
    cached-auth.tst - Session-cached authentication for Basic and Digest routes.

    websAuthenticate stores the authenticated username in session storage and reads it back with
    websGetSessionVar on later requests, so a client holding the session cookie is not re-challenged.
    That read is on the path changed by the session-creation fix (a read no longer creates a
    session), so it needs direct coverage: neither auth/basic.tst.ts nor auth/digest.tst.ts sends a
    cookie.

    Scope: same-route reuse only. Whether a session minted on the Basic route should satisfy the
    Digest route is a separate open question - do not assert cross-protocol reuse here.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const FAKE = "-goahead-session-=not-a-real-session-id"

if (thas('ME_GOAHEAD_AUTH')) {
    //  Basic: authenticate with credentials. A session is created for the authenticated user.
    let http: Http = new Http
    http.setCredentials('joshua', 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)
    let setCookie = http.header("set-cookie")
    ttrue(setCookie != null)
    let basicCookie = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
    http.close()

    //  The cookie alone authenticates the next request - the cached read resolves the session.
    http = new Http
    http.setCookie(basicCookie)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    //  Negative control: no cookie and no credentials.
    http = new Http
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)
    http.close()

    //  Negative control: an unknown session id grants nothing and allocates nothing.
    http = new Http
    http.setCookie(FAKE)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)
    http.close()

    //  Digest: same round trip.
    http = new Http
    http.setCredentials('joshua', 'pass1')
    http.get(HTTP + '/auth/digest/digest.html')
    await http.wait()
    ttrue(http.status == 200)
    setCookie = http.header("set-cookie")
    ttrue(setCookie != null)
    let digestCookie = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
    http.close()

    http = new Http
    http.setCookie(digestCookie)
    http.get(HTTP + '/auth/digest/digest.html')
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    http = new Http
    http.setCookie(FAKE)
    http.get(HTTP + '/auth/digest/digest.html')
    await http.wait()
    ttrue(http.status == 401)
    http.close()

    //  Clean up so the session table is not left holding entries for later tests.
    for (let cookie of [basicCookie, digestCookie]) {
        http = new Http
        http.setCookie(cookie)
        http.get(HTTP + "/action/logout")
        await http.wait()
        http.close()
    }
}
