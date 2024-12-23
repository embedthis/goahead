/*
    form.tst - Basic session form tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  GET
http.get(HTTP + "/action/sessionTest")
ttrue(http.status == 200)
ttrue(http.response.contains("Number null"))
let cookie = http.header("Set-Cookie")
if (cookie) {
    cookie = cookie.match(/(-goahead-session-=.*);/)[1]
}
ttrue(cookie && cookie.contains("-goahead-session-="))
http.close()

//  POST
http.setCookie(cookie)
http.form(HTTP + "/action/sessionTest", {number: "42"})
ttrue(http.status == 200)
ttrue(http.response.contains("Number 42"))
ttrue(!http.header("Set-Cookie"))
http.close()


//  GET - should now get number from session
http.setCookie(cookie)
http.get(HTTP + "/action/sessionTest")
ttrue(http.status == 200)
ttrue(http.response.contains("Number 42"))
ttrue(!http.header("Set-Cookie"))
http.close()
