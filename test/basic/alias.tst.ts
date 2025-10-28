/*
    alias.tst - Alias http tests
 */

import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

/*
    The old-alias route maps to /alias/atest.html
 */
http.followRedirects = true
http.get(HTTP + "/old-alias/")
await http.wait()
ttrue(http.status == 200)
ttrue(http.response.contains("alias/atest.html"))
