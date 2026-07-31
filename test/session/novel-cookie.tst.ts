/*
    novel-cookie.tst - An unknown session id must not be adopted, and must not cost the server a
    session slot.

    A read never allocates, so an arbitrary client cannot mint server state by presenting a novel
    cookie. A write does allocate, but always mints a fresh server-generated id rather than adopting
    the client's value (the session fixation invariant, see fixation.tst.ts).
 */

import { Http } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const FAKE = "-goahead-session-=not-a-real-session-id"

let http: Http = new Http

//  Read with an unknown id. Neither adopted nor replaced with a new session.
http.setCookie(FAKE)
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number null"))
ttrue(http.header("set-cookie") == null)
http.close()

//  Fifty distinct unknown ids must allocate nothing. Before the fix each of these consumed one of
//  the ME_GOAHEAD_LIMIT_SESSION_COUNT slots.
for (let i = 0; i < 50; i++) {
    http.reset()
    http.setCookie("-goahead-session-=novel-id-" + i)
    http.get(HTTP + "/action/sessionTest")
    await http.wait()
    ttrue(http.status == 200)
    ttrue(http.header("set-cookie") == null)
    http.close()
}

//  A write with an unknown id creates a session, but with a fresh server-generated id.
http.reset()
http.setCookie(FAKE)
http.form(HTTP + "/action/sessionTest", {number: "7"})
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 7"))
let setCookie = http.header("set-cookie")
ttrue(setCookie != null)
let issued = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
ttrue(issued != FAKE)
ttrue(issued.contains("::webs.session::"))
http.close()

//  The planted id was never bound to the stored value.
http.reset()
http.setCookie(FAKE)
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number null"))
ttrue(http.header("set-cookie") == null)
http.close()

//  The issued id was.
http.reset()
http.setCookie(issued)
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 7"))
http.close()
