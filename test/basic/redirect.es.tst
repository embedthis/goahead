/*
    redirect.tst - Redirection tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  First just test a normal get
http.get(HTTP + "/dir/index.html")
ttrue(http.status == 200)

http.followRedirects = false
http.get(HTTP + "/dir")
ttrue(http.status == 302)

http.followRedirects = true
http.get(HTTP + "/dir")
ttrue(http.status == 200)

/*
http.followRedirects = true
http.get(HTTP + "/dir/")
ttrue(http.status == 200)
ttrue(http.response.contains("Hello /dir/index.html"))
*/
