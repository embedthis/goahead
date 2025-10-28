/*
    secure.tst - Test SECURE ability
 */

import { Http } from 'ejscript'
import { tget, thas, tskip, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const HTTPS = tget('TM_HTTPS') || "127.0.0.1:4443"

let http: Http = new Http

if (thas("ME_SSL")) {
    //  Will be denied and redirect to https
    http.get(HTTP + "/secure/index.html")
    await http.wait()
    ttrue(http.status == 302)
    http.close()

    //  Will be admitted
    http.verify = false
    http.get(HTTPS + "/secure/index.html")
    await http.wait()
    ttrue(http.status == 200)
    ttrue(http.response.contains("Hello secure world"))
    http.close()
} else {
    tskip("SSL not enabled")
}
