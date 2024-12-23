/*
    secure.tst - SSL http tests
 */

if (!Config.SSL) {
    tskip("SSL not enabled in ejscript")

} else if (thas('ME_SSL')) {
    const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
    const HTTPS = tget('TM_HTTPS') || "127.0.0.1:4443"
    let http: Http = new Http

    http.verify = false
    http.get(HTTP + "/index.html")
    ttrue(!http.isSecure)
    http.close()

    http.verify = false
    http.get(HTTPS + "/index.html")
    http.wait()
    ttrue(http.isSecure)
    http.close()

    http.verify = false
    http.get(HTTPS + "/index.html")
    ttrue(http.readString(12) == "<html><head>")
    http.close()

    //  Validate get contents
    http.verify = false
    http.get(HTTPS + "/index.html?a=b")
    ttrue(http.response.endsWith("</html>\n"))
    ttrue(http.response.endsWith("</html>\n"))
    http.close()

    http.verify = false
    http.post(HTTPS + "/index.html", "Some data")
    ttrue(http.status == 200)
    http.close()

} else {
    tskip("SSL not enabled")
}
