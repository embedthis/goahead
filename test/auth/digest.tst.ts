/*
    digest.tst - Digest authentication http tests
 */

import { Http } from 'ejscript'
import { tget, thas, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    //  Access to digest/digest.html accepts by any valid user
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 200)
}
