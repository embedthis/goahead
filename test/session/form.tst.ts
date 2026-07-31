/*
    form.tst - Session variable lifecycle via the sessionTest action.

    Order matters here: the read comes first and must NOT establish a session. Reading or removing a
    session variable does not create one - a write is what establishes the session. This
    test previously asserted that the opening cookie-less GET was answered with a Set-Cookie, which
    was the defect itself: any unauthenticated request could mint server state. Do not restore that
    assertion.
 */

import { Http } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  GET with no cookie. The read returns the default and must not allocate a session.
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number null"))
ttrue(http.header("set-cookie") == null)
http.close()

//  POST with no cookie. The write establishes the session, so a cookie is issued here.
http.reset()
http.form(HTTP + "/action/sessionTest", {number: "42"})
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 42"))
let setCookie = http.header("set-cookie")
ttrue(setCookie != null)
let cookie = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
ttrue(cookie.contains("::webs.session::"))
http.close()

//  GET with the issued cookie - the value persists across requests.
http.reset()
http.setCookie(cookie)
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 42"))
//  An existing session is resolved, not re-created, so no new cookie is issued. A cookie reappearing
//  here would mean the session was silently lost and a fresh one minted.
ttrue(http.header("set-cookie") == null)
http.close()

//  POST with the issued cookie - a write into an existing session does not rotate the id.
http.reset()
http.setCookie(cookie)
http.form(HTTP + "/action/sessionTest", {number: "43"})
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 43"))
ttrue(http.header("set-cookie") == null)
http.close()

//  Read back the updated value.
http.reset()
http.setCookie(cookie)
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 43"))
http.close()
