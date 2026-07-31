/*
    rotate.tst - Session id rotation at the Basic/Digest privilege transition.

    websAuthenticate stores the authenticated username in session storage. It must rotate the
    session id first: a client that arrives holding a live, server-issued id would otherwise have the
    victim's identity written into the session it chose (session fixation). fixation.tst.ts covers
    the form-login path and plants a fabricated id, which was never adopted; this test presents a
    genuine, live id obtained pre-auth from the anonymous session action.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

/*
    Obtain a real, live, server-issued session id without authenticating. A write to the session
    action establishes the session and returns its cookie.
 */
async function plantSession(value: string): String {
    let http: Http = new Http
    http.form(HTTP + "/action/sessionTest", {number: value})
    await http.wait()
    ttrue(http.status == 200)
    let cookie = http.header("set-cookie").match(/(-goahead-session-=[^;]*)/)[1]
    ttrue(cookie.contains("::webs.session::"))
    http.close()
    return cookie
}

if (thas('ME_GOAHEAD_AUTH')) {
    for (let [scheme, page, value] of [["basic", "/auth/basic/basic.html", "71"],
                                       ["digest", "/auth/digest/digest.html", "72"]]) {
        let planted = await plantSession(value)

        //  The planted session is live: it holds the value written into it.
        let http: Http = new Http
        http.setCookie(planted)
        http.get(HTTP + "/action/sessionTest")
        await http.wait()
        ttrue(http.status == 200)
        ttrue(http.response.contains("Number " + value))
        http.close()

        //  Authenticate while presenting the planted id. The id must be rotated, not adopted.
        http = new Http
        http.setCookie(planted)
        http.setCredentials('joshua', 'pass1')
        http.get(HTTP + page)
        await http.wait()
        ttrue(http.status == 200)
        let setCookie = http.header("set-cookie")
        ttrue(setCookie != null)
        let issued = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
        ttrue(issued != planted)
        ttrue(issued.contains("::webs.session::"))
        http.close()

        //  The planted id did not inherit the identity and no longer names a session at all.
        http = new Http
        http.setCookie(planted)
        http.get(HTTP + page)
        await http.wait()
        ttrue(http.status == 401)
        http.close()

        http = new Http
        http.setCookie(planted)
        http.get(HTTP + "/action/sessionTest")
        await http.wait()
        ttrue(http.status == 200)
        ttrue(http.response.contains("Number null"))
        http.close()

        //  The issued id is the authenticated one and reaches the page without credentials.
        http = new Http
        http.setCookie(issued)
        http.get(HTTP + page)
        await http.wait()
        ttrue(http.status == 200)
        http.close()

        //  No spurious rotation: the cached path resolves the session, it does not mint a new id.
        http = new Http
        http.setCookie(issued)
        http.get(HTTP + page)
        await http.wait()
        ttrue(http.status == 200)
        ttrue(http.header("set-cookie") == null)
        http.close()

        //  Do not leave entries in the session table for later tests.
        http = new Http
        http.setCookie(issued)
        http.get(HTTP + "/action/logout")
        await http.wait()
        http.close()
    }
}
