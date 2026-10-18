/* Session-cached authentication for Basic and Digest routes. */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const FAKE = "-goahead-session-=not-a-real-session-id"

function sessionCookie(http: Http): string | null {
    let setCookie = http.header("set-cookie")
    let match = setCookie ? setCookie.match(/(-goahead-session-=[^;]*)/) : null
    return match ? match[1] : null
}

//  An unauthenticated session, as a browser holds after visiting any page that uses one
async function newSession(): Promise<string> {
    let http = new Http
    http.form(HTTP + "/action/sessionTest", {number: "1"})
    await http.wait()
    let cookie = sessionCookie(http)
    http.close()
    ttrue(cookie != null, 'the test action issues a session')
    return cookie!
}

async function cachedRoundTrip(target: string): Promise<string> {
    //  Credentials without a session: authenticated, but no session is created
    let http = new Http
    http.setCredentials('joshua', 'pass1')
    http.get(HTTP + target)
    await http.wait()
    ttrue(http.status == 200, `credentials authenticate ${target}`)
    ttrue(sessionCookie(http) == null, `a request without a session must not create one for ${target}`)
    http.close()

    //  Credentials with a session: the login is cached in it under a new id
    let held = await newSession()
    http = new Http
    http.setCredentials('joshua', 'pass1')
    http.setCookie(held)
    http.get(HTTP + target)
    await http.wait()
    ttrue(http.status == 200, `credentials with a session authenticate ${target}`)
    let cached = sessionCookie(http)
    ttrue(cached != null && cached != held, `the session is rotated when the login is cached for ${target}`)
    http.close()

    //  The cookie alone authenticates the next request - the cached read resolves the session
    http = new Http
    http.setCookie(cached!)
    http.get(HTTP + target)
    await http.wait()
    ttrue(http.status == 200, `the cached session authenticates ${target}`)
    http.close()

    //  Negative control: no cookie and no credentials
    http = new Http
    http.get(HTTP + target)
    await http.wait()
    ttrue(http.status == 401, `no credentials are refused for ${target}`)
    http.close()

    //  Negative control: an unknown session id grants nothing and allocates nothing
    http = new Http
    http.setCookie(FAKE)
    http.get(HTTP + target)
    await http.wait()
    ttrue(http.status == 401, `an unknown session is refused for ${target}`)
    http.close()
    return cached!
}

if (thas('ME_GOAHEAD_AUTH')) {
    let basicCookie = await cachedRoundTrip('/auth/basic/basic.html')
    let digestCookie = await cachedRoundTrip('/auth/digest/digest.html')

    //  Clean up so the session table is not left holding entries for later tests.
    for (let cookie of [basicCookie, digestCookie]) {
        let http = new Http
        http.setCookie(cookie)
        http.get(HTTP + "/action/logout")
        await http.wait()
        http.close()
    }
}
