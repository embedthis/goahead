/*
    basic.tst - Basic-style Authentication tests
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    //  Access to basic/basic.html only accepts an authenticated user. This should fail with 401.
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  Legacy (MD5-encoded) user authenticates with correct password.
    http.setCredentials('joshua', 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)

    //  Legacy (MD5-encoded) user rejected with wrong password.
    http.setCredentials('joshua', 'wrongpass')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  Legacy (MD5-encoded) user rejected with empty password.
    http.setCredentials('joshua', '')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  Unknown user rejected (exercises the early websLookupUser == 0 return).
    http.setCredentials('nobody', 'anything')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  BF1 (bcrypt) user authenticates with correct password.
    http.setCredentials('testbf1', 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)

    //  BF1 user rejected with wrong password (regression for BF1 bypass).
    http.setCredentials('testbf1', 'wrongpass')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  BF1 user rejected with empty password.
    http.setCredentials('testbf1', '')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    /*
        Plaintext-length ceiling test. WEBS_MAX_PASSWORD (32) bounds plaintext
        regardless of username or realm length. bigbf1 has a BF1 hash derived
        from a 32-char password, proving the ceiling is reachable for both
        branches. Before the fix, a 32-char password was unreachable for any
        username because the limit was checked against "user:realm:plaintext".
     */
    let pw32 = 'a'.repeat(32)
    http.setCredentials('bigbf1', pw32)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)

    //  Same ceiling applies via the MD5 branch -> consistent handling.
    http.setCredentials('bigmd5', pw32)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)

    //  33-char plaintext exceeds WEBS_MAX_PASSWORD -> reject uniformly via BF1 branch.
    let pw33 = 'a'.repeat(33)
    http.setCredentials('bigbf1', pw33)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  33-char plaintext rejected via MD5 branch too -> the same WEBS_MAX_PASSWORD
    //  check now applies uniformly regardless of stored hash format.
    http.setCredentials('bigmd5', pw33)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  Very long password via BF1 branch must reject cleanly, not crash or bypass.
    let longPass = 'a'.repeat(1024)
    http.setCredentials('testbf1', longPass)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  Very long password via MD5 branch -> plaintext length check rejects without
    //  reaching the truncating fmt() into passbuf.
    http.setCredentials('joshua', longPass)
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  Malformed stored BF1 hash (missing salt/hash tokens) must reject.
    http.setCredentials('badbf1', 'anything')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    /*
        Liveness check: after the over-length and malformed-hash cases, an
        unauthenticated request to a public URL must still succeed. Converts
        any crash triggered above into a test failure rather than a confusing
        connection-refused in a later test.
     */
    http.setCredentials('', '')
    http.get(HTTP + '/index.html')
    await http.wait()
    ttrue(http.status == 200)
}
