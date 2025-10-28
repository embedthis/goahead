/*
    basic.tst - Basic-style Authentication tests
 */

import { Http } from 'ejscript'
import { tget, thas, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    //  Access to basic/basic.html only accepts an authenticated user. This should fail with 401.
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    http.setCredentials('joshua', 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)
}
