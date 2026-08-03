/*
    fixation.tst - Session fixation regression (GOAHEAD-SA-2026-0011)

    A session id planted in the client's cookie before login must NOT survive into the
    authenticated session. On a successful login GoAhead must issue a fresh, server-generated
    session id (rotation), and the planted id must not reach the protected form-auth resource.
 */

import { Http, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "http://127.0.0.1:8080"
const PLANTED = "-goahead-session-=attacker-fixed-session-20260617"

let http: Http = new Http

//  Baseline: the planted (unauthenticated) cookie cannot reach the protected page.
http.setCookie(PLANTED)
http.get(HTTP + "/auth/form/index.html")
await http.wait()
ttrue(http.status == 302)
ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
http.close()

//  Victim logs in while carrying the planted cookie. Login succeeds AND rotates the id.
http.reset()
http.setCookie(PLANTED)
http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
await http.wait()
ttrue(http.status == 302)
ttrue(new Uri(http.header('location')).path == '/auth/form/index.html')
let setCookie = http.header("set-cookie")
ttrue(setCookie != null)
let issued = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
//  The server-issued id must differ from the planted id (rotation on login) and be server-generated.
ttrue(issued != PLANTED)
ttrue(issued.contains("::webs.session::"))
http.close()

//  The planted id must NOT access the protected page after the victim's login.
http.reset()
http.setCookie(PLANTED)
http.get(HTTP + "/auth/form/index.html")
await http.wait()
ttrue(http.status == 302)
ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
http.close()

//  The freshly-issued id IS authenticated and reaches the protected page.
http.reset()
http.setCookie(issued)
http.get(HTTP + "/auth/form/index.html")
await http.wait()
ttrue(http.status == 200)
http.close()

//  Wrong-password control: a failed login must not authenticate a planted id.
const PLANTED2 = "-goahead-session-=attacker-fixed-session-wrong"
http.reset()
http.setCookie(PLANTED2)
http.form(HTTP + "/action/login", {username: "joshua", password: "wrong-password"})
await http.wait()
ttrue(http.status == 302)
ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
http.close()

http.reset()
http.setCookie(PLANTED2)
http.get(HTTP + "/auth/form/index.html")
await http.wait()
ttrue(http.status == 302)
ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
http.close()
