/*
    getv6.tst - IPv6 GET tests
 */

import { Http } from 'ejscript'
import { tget, ttrue, tcontains } from 'testme'

const HTTPV6 = tget('TM_HTTPV6') || "[::1]:8090"
let http: Http = new Http

//  Basic get. Validate response code and contents
http.get(HTTPV6 + "/index.html")
await http.wait()
ttrue(http.status == 200)
// ttrue(http.readString().contains("Hello"))

//  Validate get contents
http.get(HTTPV6 + "/index.html")
await http.wait()
ttrue(http.readString(12) == "<html><head>")

//  Validate get contents
http.get(HTTPV6 + "/index.html")
await http.wait()
tcontains(http.response, "</html>")
tcontains(http.response, "</html>")

//  Test Get with a body. Yes this is valid Http, although unusual.
http.get(HTTPV6 + "/index.html", 'name=John&address=700+Park+Ave')
await http.wait()
ttrue(http.status == 200)
