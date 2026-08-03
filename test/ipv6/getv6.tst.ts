/*
    getv6.tst - IPv6 GET tests
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'

const HTTPV6 = tget('TM_HTTPV6') || "http://[::1]:18090"

if (thas('ME_IPV6')) {
    let http: Http = new Http

    //  Basic get. Validate response code
    http.get(HTTPV6 + "/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    //  Validate get contents
    http.get(HTTPV6 + "/index.html")
    await http.wait()
    ttrue(http.readString(12) == "<html><head>")
    http.close()

    //  Validate get contents
    http.get(HTTPV6 + "/index.html")
    await http.wait()
    tcontains(http.response, "</html>")
    http.close()

    //  Test Get with a body. Yes this is valid Http, although unusual.
    http.get(HTTPV6 + "/index.html", 'name=John&address=700+Park+Ave')
    await http.wait()
    ttrue(http.status == 200)
    http.close()
} else {
    tskip("IPv6 not enabled")
}
