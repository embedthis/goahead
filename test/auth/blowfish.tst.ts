/*
    blowfish.tst - Blowfish password hash key-window tests

    The Blowfish key schedule consumes a fixed 72 key bytes. Password hashes are keyed with
    "salt:username:realm:password", so a long enough username and realm push the password out of
    the window entirely: the stored hash no longer depends on the password and every password
    authenticates. Both accounts below sit at exactly that condition. The legacy BF1 account must
    now be refused; the BF2 account, keyed with a fixed-width digest, must be unaffected.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

//  42 characters each. With the 11-character realm they reach the key window.
const BF1_CLIFF_USER = 'bf1cliffuser000000000000000000000000000000'
const BF2_LONG_USER = 'bf2longuser0000000000000000000000000000000'

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    //  The legacy BF1 account is bypassable, so it must be refused - including with the password
    //  it was created with. Before the fix every one of these returned 200.
    http.setCredentials(BF1_CLIFF_USER, 'wrongpass')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    http.setCredentials(BF1_CLIFF_USER, '')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    http.setCredentials(BF1_CLIFF_USER, 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  The BF2 account under the same username and realm lengths authenticates normally.
    http.setCredentials(BF2_LONG_USER, 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)

    //  ... and only with the correct password.
    http.setCredentials(BF2_LONG_USER, 'wrongpass')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    http.setCredentials(BF2_LONG_USER, '')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 401)

    //  A BF1 account below the window is untouched by the fix and still authenticates.
    http.setCredentials('testbf1', 'pass1')
    http.get(HTTP + '/auth/basic/basic.html')
    await http.wait()
    ttrue(http.status == 200)
}
