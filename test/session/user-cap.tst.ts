/* One account cannot fill the session table */

import { Http, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const USER_LIMIT = 8                // ME_GOAHEAD_LIMIT_SESSION_USER
const LOGINS = USER_LIMIT + 4

async function sessionCount(http: Http): Promise<number> {
    http.reset()
    http.get(HTTP + "/action/sessions")
    await http.wait()
    let count = parseInt(http.response.match(/sessions=(\d+)/)[1])
    http.close()
    return count
}

async function login(http: Http, username: string, password: string): Promise<string | null> {
    http.reset()
    http.form(HTTP + "/action/login", {username, password})
    await http.wait()
    let ok = http.status == 302 && new Uri(http.header('location')).path == '/auth/form/index.html'
    let setCookie = http.header("set-cookie")
    http.close()
    return ok && setCookie ? setCookie.match(/(-goahead-session-=[^;]*)/)[1] : null
}

//  /stale/ requires a form login with the "view" ability, which mary holds
async function loggedIn(http: Http, cookie: string): Promise<boolean> {
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/stale/index.html")
    await http.wait()
    let refused = http.status == 302 && new Uri(http.header('location') || '').path == '/auth/form/login.html'
    http.close()
    return !refused
}

if (thas('ME_GOAHEAD_AUTH')) {
    let http: Http = new Http
    let base = await sessionCount(http)

    let cookies: string[] = []
    let failed = 0
    for (let i = 0; i < LOGINS; i++) {
        let cookie = await login(http, "mary", "pass2")
        if (cookie) {
            cookies.push(cookie)
        } else {
            failed++
        }
    }
    ttrue(failed == 0, `${failed} of ${LOGINS} logins by one account failed`)
    //  mary may hold sessions from earlier tests: they are older, so they are retired first
    let count = await sessionCount(http)
    ttrue(count - base <= USER_LIMIT, `one account must hold at most ${USER_LIMIT} sessions, it added ${count - base}`)

    let oldLive = 0, newLive = 0
    for (let i = 0; i < cookies.length; i++) {
        let live = await loggedIn(http, cookies[i])
        if (i < LOGINS - USER_LIMIT) {
            oldLive += live ? 1 : 0
        } else {
            newLive += live ? 1 : 0
        }
    }
    ttrue(oldLive == 0, `the ${LOGINS - USER_LIMIT} oldest sessions must be logged out, ${oldLive} still work`)
    ttrue(newLive == USER_LIMIT, `the ${USER_LIMIT} newest sessions must still work, ${newLive} do`)

    let admin = await login(http, "joshua", "pass1")
    ttrue(admin != null, 'another account must still log in')
    if (admin) {
        cookies.push(admin)
    }

    for (let each of cookies) {
        http.reset()
        http.setCookie(each)
        http.get(HTTP + "/action/logout")
        await http.wait()
        http.close()
    }
}
