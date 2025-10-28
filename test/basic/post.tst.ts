/*
    post.tst - Post method tests
 */

import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

http.post(HTTP + "/action/test", "Some data")
await http.wait()
ttrue(http.status == 200)
http.close()

http.form(HTTP + "/action/test", {name: "John", address: "700 Park Ave"})
await http.wait()
ttrue(http.response.contains('name: John'))
ttrue(http.response.contains('address: 700 Park Ave'))
http.close()
