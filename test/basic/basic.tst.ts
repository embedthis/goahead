/*
    basic.tst - Basic authentication tests
 */

import { Http, Config } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    // Any valid user
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/basic/basic.html")
    await http.wait()
    ttrue(http.status == 200)

    // Must be rejected
    http.setCredentials("joshua", "WRONG PASSWORD")
    http.get(HTTP + "/auth/basic/basic.html")
    await http.wait()
    ttrue(http.status == 401)

    if (Config.OS == "windows") {
        // Case won't matter
        http.setCredentials("joshua", "pass1")
        http.get(HTTP + "/baSIC/BASic.hTMl")
        await http.wait()
        ttrue(http.status == 200)
    }
}
