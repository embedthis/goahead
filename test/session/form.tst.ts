/*
    form.tst - Basic session form tests
 */

import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  GET
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number null"))
let cookie = http.header("set-cookie")
if (cookie) {
    cookie = cookie.match(/(-goahead-session-=.*);/)[1]
}
ttrue(cookie && cookie.contains("-goahead-session-="))
http.close()

//  POST
http.setCookie(cookie)
http.form(HTTP + "/action/sessionTest", {number: "42"})
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 42"))
// Server may send set-cookie again, that's OK
// ttrue(!http.header("set-cookie"))
http.close()


//  GET - should now get number from session
http.setCookie(cookie)
http.get(HTTP + "/action/sessionTest")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Number 42"))
// Server may send set-cookie again, that's OK
// ttrue(!http.header("set-cookie"))
http.close()
