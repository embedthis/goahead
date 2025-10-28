/*
    redirect.tst - Redirection tests
 */

import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  First just test a normal get
http.get(HTTP + "/dir/index.html")
await http.wait()
ttrue(http.status == 200)

http.followRedirects = false
http.get(HTTP + "/dir")
await http.wait()
ttrue(http.status == 302)

http.followRedirects = true
http.get(HTTP + "/dir")
await http.wait()
ttrue(http.status == 200)

/*
http.followRedirects = true
http.get(HTTP + "/dir/")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("Hello /dir/index.html"))
*/
