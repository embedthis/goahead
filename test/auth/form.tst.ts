/*
    form.tst - Form-based authentication tests
 */

import { Http, Uri } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

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

http.close()
