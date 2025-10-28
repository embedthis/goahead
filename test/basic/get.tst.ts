/*
    get.tst - Http GET tests
 */

import { Http, Config } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  Basic get. Validate response code and contents
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
// ttrue(http.readString().contains("Hello"))

//  Validate get contents
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.readString(12) == "<html><head>")

//  Validate get contents
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.response.endsWith("</html>\n"))
ttrue(http.response.endsWith("</html>\n"))

//  Test Get with a body. Yes this is valid Http, although unusual.
http.get(HTTP + "/index.html", 'name=John&address=700+Park+Ave')
await http.wait()
ttrue(http.status == 200)

if (Config.OS == "windows") {
    http.get(HTTP + "/inDEX.htML")
    await http.wait()
    ttrue(http.status == 200)
}
