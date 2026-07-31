/*
    exhaust.tst - Session table exhaustion and the login lockout.

    A failed login must not create a session, so anonymous requests cannot fill the session table and
    lock every legitimate user out of form authentication. And when the table genuinely is full, a
    login must fail as a failure - not report success and send the client to the protected page with
    no session cookie, where it is bounced straight back to the login form.

    The authenticated sessions this test creates are logged out again at the end. Tests run serially
    against one shared server (testme.json5, execution.parallel: false) and a session lives
    ME_GOAHEAD_LIMIT_SESSION_LIFE seconds, so leaving them behind would break every later test.
 */

import { Http, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const SESSION_COUNT = 512           // ME_GOAHEAD_LIMIT_SESSION_COUNT
const ATTEMPTS = SESSION_COUNT + 8

if (thas('ME_GOAHEAD_AUTH')) {
    let http: Http = new Http

    //  A single failed login allocates nothing and hands back no session id.
    http.form(HTTP + "/action/login", {username: "nobody", password: "bad"})
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
    ttrue(http.header("set-cookie") == null)
    http.close()

    //  More failed logins than the table can hold. None of them may consume a slot.
    for (let i = 0; i < ATTEMPTS; i++) {
        http.reset()
        http.form(HTTP + "/action/login", {username: "nobody", password: "bad"})
        await http.wait()
        ttrue(http.status == 302)
        ttrue(http.header("set-cookie") == null)
        http.close()
    }

    //  Form login still works for a legitimate user - the lockout is gone.
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/index.html')
    let setCookie = http.header("set-cookie")
    ttrue(setCookie != null)
    let cookie = setCookie.match(/(-goahead-session-=[^;]*)/)[1]
    http.close()

    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    /*
        Now fill the table with genuine sessions. Loop until a login comes back without a cookie
        rather than counting to exactly SESSION_COUNT: the shared server may already hold sessions
        from earlier tests.
     */
    let cookies = [cookie]
    let exhausted = null
    for (let i = 0; i < ATTEMPTS; i++) {
        http.reset()
        http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
        await http.wait()
        ttrue(http.status == 302)
        let issued = http.header("set-cookie")
        let location = new Uri(http.header('location')).path
        http.close()
        if (issued == null) {
            exhausted = location
            break
        }
        cookies.push(issued.match(/(-goahead-session-=[^;]*)/)[1])
    }

    //  The table filled, and the login that could not get a session failed loudly: it was sent back
    //  to the login page, not redirected to the protected page with no cookie.
    ttrue(exhausted != null)
    ttrue(exhausted == '/auth/form/login.html')

    //  Drain the table again.
    for (let each of cookies) {
        http.reset()
        http.setCookie(each)
        http.get(HTTP + "/action/logout")
        await http.wait()
        http.close()
    }

    //  And logins work again.
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/index.html')
    setCookie = http.header("set-cookie")
    ttrue(setCookie != null)
    http.close()

    http.reset()
    http.setCookie(setCookie.match(/(-goahead-session-=[^;]*)/)[1])
    http.get(HTTP + "/action/logout")
    await http.wait()
    http.close()
}
