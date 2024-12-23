/*
    reuse.tst - Test Http reuse for multiple requests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

http.get(HTTP + "/index.html")
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
ttrue(http.status == 200)
