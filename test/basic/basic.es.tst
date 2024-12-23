/*
    basic.tst - Basic authentication tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
/*
    http.setCredentials("anybody", "PASSWORD WONT MATTER")
    http.get(HTTP + "/index.html")
    ttrue(http.status == 200)
*/

    // Any valid user
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/basic/basic.html")
    ttrue(http.status == 200)
App.exit(0);

    // Must be rejected
    http.setCredentials("joshua", "WRONG PASSWORD")
    http.get(HTTP + "/auth/basic/basic.html")
    ttrue(http.status == 401)

/* FUTURE
    // Group access
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/basic/group/group.html")
    ttrue(http.status == 200)

    // Must be rejected - Joshua is not in group
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/basic/group/group.html")
    ttrue(http.status == 401)

    // User access - Joshua is the required member
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/basic/user/user.html")
    ttrue(http.status == 200)

    // Must be rejected - Mary is not in group
    http.setCredentials("mary", "pass1")
    http.get(HTTP + "/auth/basic/user/user.html")
    ttrue(http.status == 401)
*/

    if (Config.OS == "windows") {
        // Case won't matter
        http.setCredentials("joshua", "pass1")
        http.get(HTTP + "/baSIC/BASic.hTMl")
        ttrue(http.status == 200)
    }
}
