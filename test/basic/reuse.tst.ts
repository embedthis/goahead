/*
    reuse.tst - Test Http reuse for multiple requests
 */

import { Http } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
