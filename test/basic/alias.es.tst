/*
    alias.tst - Alias http tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

/*
    The old-alias route maps to /alias/atest.html
 */
http.get(HTTP + "/old-alias/")
http.followRedirects = true
ttrue(http.status == 200)
ttrue(http.response.contains("alias/atest.html"))
