/*
    digest.tst - Digest authentication tests
 */

import { Http, Config } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    http.setCredentials("anybody", "PASSWORD WONT MATTER")
    http.get(HTTP + "/index.html")
    await http.wait()
    ttrue(http.status == 200)

    // Any valid user
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 200)

    // Must be rejected
    http.setCredentials("joshua", "WRONG PASSWORD")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    // Group access
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 200)

    // Must be rejected - Mary is not an administrator
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/digest/admin/index.html")
    await http.wait()
    ttrue(http.status == 401)

    if (Config.OS == "windows" || Config.OS == "win32") {
        // Case won't matter
        http.setCredentials("joshua", "pass1")
        http.get(HTTP + "/auth/diGEST/diGEST.hTMl")
        await http.wait()
        ttrue(http.status == 200)
    }
}
