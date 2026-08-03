/*
    basic.tst - Basic authentication tests
 */

import { Http } from '@embedthis/ejscript'
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

    /*
        A Windows-only block here used to assert that "/baSIC/BASic.hTMl" returned 200 because
        "case won't matter" on a case-insensitive file system. It was doubly wrong and never ran,
        because Config.OS could not equal "windows": the path dropped the "/auth" prefix so it
        resolved to nothing at any casing, and serving a case-folded variant of a protected URL is
        exactly the auth bypass the case-folding hardening now prevents. regress/uri-case-variant
        covers the real contract - 401 or 404, and the content must not appear.
     */
}
