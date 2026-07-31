/*
    logout.tst - Logout must not allocate a session.

    websLogoutUser removes the username session variable before destroying the session. Removing a
    variable used to create a session, so an unauthenticated logout created a session purely to
    delete it - emitting a Set-Cookie on the way out and giving an anonymous client a live,
    server-issued session id for the cost of one request.
 */

import { Http, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

if (thas('ME_GOAHEAD_AUTH')) {
    let http: Http = new Http

    //  Unauthenticated logout. Redirects to the login page and allocates nothing.
    http.get(HTTP + "/action/logout")
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
    ttrue(http.header("set-cookie") == null)
    http.close()

    //  Repeating it costs the server nothing.
    for (let i = 0; i < 50; i++) {
        http.reset()
        http.get(HTTP + "/action/logout")
        await http.wait()
        ttrue(http.status == 302)
        ttrue(http.header("set-cookie") == null)
        http.close()
    }

    //  Logout with an unknown session id allocates nothing either.
    http.reset()
    http.setCookie("-goahead-session-=not-a-real-session-id")
    http.get(HTTP + "/action/logout")
    await http.wait()
    ttrue(http.status == 302)
    ttrue(http.header("set-cookie") == null)
    http.close()

    //  A real logout still revokes access, and issues no new cookie doing it.
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/index.html')
    let cookie = http.header("set-cookie").match(/(-goahead-session-=[^;]*)/)[1]
    http.close()

    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    http.reset()
    http.setCookie(cookie)
    http.post(HTTP + "/action/logout")
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
    ttrue(http.header("set-cookie") == null)
    http.close()

    //  The session is gone server-side.
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
    http.close()
}
