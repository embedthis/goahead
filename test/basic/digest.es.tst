/*
    digest.tst - Digest authentication tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    http.setCredentials("anybody", "PASSWORD WONT MATTER")
    http.get(HTTP + "/index.html")
    ttrue(http.status == 200)

    // Any valid user 
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/digest/digest.html")
    ttrue(http.status == 200)

    // Must be rejected 
    http.setCredentials("joshua", "WRONG PASSWORD")
    http.get(HTTP + "/auth/digest/digest.html")
    ttrue(http.status == 401)

    // Group access 
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/digest/digest.html")
    ttrue(http.status == 200)

    // Must be rejected - Mary is not an administrator
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/digest/admin/index.html")
    ttrue(http.status == 401)

    if (Config.OS == "windows") {
        // Case won't matter 
        http.setCredentials("joshua", "pass1")
        http.get(HTTP + "/auth/diGEST/diGEST.hTMl")
        ttrue(http.status == 200)
    }
}
