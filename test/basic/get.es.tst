/*
    get.tst - Http GET tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  Basic get. Validate response code and contents
http.get(HTTP + "/index.html")
ttrue(http.status == 200)
// ttrue(http.readString().contains("Hello"))

//  Validate get contents
http.get(HTTP + "/index.html")
ttrue(http.readString(12) == "<html><head>")

//  Validate get contents
http.get(HTTP + "/index.html")
ttrue(http.response.endsWith("</html>\n"))
ttrue(http.response.endsWith("</html>\n"))

//  Test Get with a body. Yes this is valid Http, although unusual.
http.get(HTTP + "/index.html", 'name=John&address=700+Park+Ave')
ttrue(http.status == 200)

if (Config.OS == "windows") {
    http.get(HTTP + "/inDEX.htML")
    ttrue(http.status == 200)
}
