/*
    form.tst - Form-based authentication tests
 */

import { Http, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    let http: Http = new Http

    //  Will be denied
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 302)
    let location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    //  Will return login form
    http.get(location)
    await http.wait()
    ttrue(http.status == 200)
    ttrue(http.response.contains("<form"))
    ttrue(http.response.contains('action="/action/login"'))

    //  Login. Should succeed with the response being a redirect to /auth/form/index.html
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/index.html')
    let cookie = http.header("set-cookie")
    ttrue(cookie.match(/(-goahead-session-=.*);/)[1])

    //  Now logged in
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 200)

    //  Now log out. Will be redirected to the login page.
    http.reset()
    http.setCookie(cookie)
    http.post(HTTP + "/action/logout")
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    //  Now should fail to access index.html and get the login page again.
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    /*
        BF1 user login via form auth. Covers Branch 1 of websVerifyPasswordFromFile
        through the loginServiceProc -> websLoginUser -> verify path. formbf1 has
        the 'administrator' role which grants the 'manage' ability required by the
        /auth/form/ route.
     */
    http.reset()
    http.form(HTTP + "/action/login", {username: "formbf1", password: "pass1"})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/index.html')
    cookie = http.header("set-cookie")
    ttrue(cookie.match(/(-goahead-session-=.*);/)[1])

    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 200)

    //  BF1 user with wrong password -> redirect to login page.
    http.reset()
    http.form(HTTP + "/action/login", {username: "formbf1", password: "wrongpass"})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    //  MD5 user with wrong password via form path (Branch 3 negative).
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: "wrongpass"})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    //  Empty password via form path.
    http.reset()
    http.form(HTTP + "/action/login", {username: "joshua", password: ""})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    //  Unknown user via form path (exercises early websLookupUser == 0 return).
    http.reset()
    http.form(HTTP + "/action/login", {username: "nobody", password: "anything"})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    /*
        Plaintext-ceiling coverage via the form entry point (loginServiceProc ->
        websLoginUser -> websVerifyPasswordFromFile). bigbf1 was generated with a
        32-char password; a 33-char submission must be rejected by the same
        WEBS_MAX_PASSWORD check that covers Basic auth.
     */
    let pw32 = 'a'.repeat(32)
    http.reset()
    http.form(HTTP + "/action/login", {username: "bigbf1", password: pw32})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/index.html')

    http.reset()
    http.form(HTTP + "/action/login", {username: "bigbf1", password: 'a'.repeat(33)})
    await http.wait()
    ttrue(http.status == 302)
    location = http.header('location')
    ttrue(new Uri(location).path == '/auth/form/login.html')

    http.close()
}
