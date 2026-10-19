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

    /*
        More failed logins than the table can hold. None of them may consume a slot.

        Counted and asserted once afterwards rather than twice per iteration. The count is fixed, so
        this half was already reproducible - but 1040 assertions for two properties made this one
        test a quarter of the whole suite's tally, which buries every other test in the summary and
        tells a reader nothing the two assertions below do not.
     */
    let badStatus = 0
    let badCookie = 0
    for (let i = 0; i < ATTEMPTS; i++) {
        http.reset()
        http.form(HTTP + "/action/login", {username: "nobody", password: "bad"})
        await http.wait()
        if (http.status != 302) {
            badStatus++
        }
        if (http.header("set-cookie") != null) {
            badCookie++
        }
        http.close()
    }
    ttrue(badStatus == 0, `${badStatus} of ${ATTEMPTS} failed logins did not redirect`)
    ttrue(badCookie == 0, `${badCookie} of ${ATTEMPTS} failed logins consumed a session slot`)

    /*
        Record results and drain sessions before asserting; failed assertions exit immediately.
     */
    let observed: [boolean, string][] = []
    let expect = (ok: boolean, message: string) => { observed.push([ok, message]) }
    let issued = (header: string | null): string | null => {
        let match = header ? header.match(/(-goahead-session-=[^;]*)/) : null
        return match ? match[1] : null
    }
    let cookies: string[] = []

    //  Form login still works for a legitimate user - the lockout is gone.
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
    await http.wait()
    expect(http.status == 302, "a legitimate login redirects")
    expect(new Uri(http.header('location')).path == '/auth/form/index.html', "a legitimate login reaches the index")
    let cookie = issued(http.header("set-cookie"))
    expect(cookie != null, "a legitimate login issues a session")
    if (cookie) {
        cookies.push(cookie)
    }
    http.close()

    if (cookie) {
        http.reset()
        http.setCookie(cookie)
        http.get(HTTP + "/auth/form/index.html")
        await http.wait()
        expect(http.status == 200, "the session reaches the protected page")
        http.close()
    }

    /*
        Fill remaining capacity with anonymous sessions; per-account limits prevent one user filling the table. Stop
        when no cookie is issued because earlier tests may hold sessions.
     */
    let notServed = 0
    let filled = 0
    for (let i = 0; i < ATTEMPTS; i++) {
        http.reset()
        http.form(HTTP + "/action/sessionTest", {number: "1"})
        await http.wait()
        /*
            Counted here and asserted once after the loop, not asserted here. This loop runs a number
            of times that depends on how many sessions the shared server already held, so an
            assertion inside it makes the suite's assertion tally a function of which tests ran
            before this one - the run total then differs between an invocation of the whole suite and
            an invocation of this directory, and cannot be reproduced or compared. The coverage is
            identical: every iteration is still checked, and the count below names how many.
         */
        if (http.status != 200) {
            notServed++
        }
        let fresh = issued(http.header("set-cookie"))
        http.close()
        filled++
        if (fresh == null) {
            break
        }
        cookies.push(fresh)
    }
    expect(notServed == 0, `${notServed} of ${filled} session requests were not served`)

    /*
        Use an account with no existing sessions so login cannot free a slot through per-account eviction.
     */
    http.reset()
    http.form(HTTP + "/action/login", {username: "julie", password: "pass4"})
    await http.wait()
    expect(http.status == 302, "a login on a full table redirects")
    let fullCookie = issued(http.header("set-cookie"))
    let exhausted = fullCookie == null ? new Uri(http.header('location')).path : null
    if (fullCookie) {
        cookies.push(fullCookie)
    }
    http.close()

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
    http.form(HTTP + "/action/login", {username: "julie", password: "pass4"})
    await http.wait()
    expect(http.status == 302, "a login on the drained table redirects")
    expect(new Uri(http.header('location')).path == '/auth/form/index.html', "a login on the drained table succeeds")
    let again = issued(http.header("set-cookie"))
    expect(again != null, "a login on the drained table issues a session")
    http.close()

    if (again) {
        http.reset()
        http.setCookie(again)
        http.get(HTTP + "/action/logout")
        await http.wait()
        http.close()
    }

    //  The table filled, and the login that could not get a session failed loudly: it was sent back
    //  to the login page, not redirected to the protected page with no cookie.
    expect(exhausted != null, "the login on the full table was not given a session")
    expect(exhausted == '/auth/form/login.html', `the login on the full table was sent to ${exhausted}`)

    for (let [ok, message] of observed) {
        ttrue(ok, message)
    }
}
