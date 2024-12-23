/*
    post.tst - Post method tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

http.post(HTTP + "/action/test", "Some data")
ttrue(http.status == 200)
http.close()

http.form(HTTP + "/action/test", {name: "John", address: "700 Park Ave"})
ttrue(http.response.contains('name: John'))
ttrue(http.response.contains('address: 700 Park Ave'))
http.close()
