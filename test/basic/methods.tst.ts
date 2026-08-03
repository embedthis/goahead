/*
    methods.tst - Test misc Http methods
 */

import { Http, Path } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

//  Test methods are caseless
http.connect("GeT", HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)

//  Put a file
const data = await new Path("test.dat").readString()
http.put(HTTP + "/tmp/test.dat", data)
await http.wait()
ttrue(http.status == 201 || http.status == 204)

//  Delete
http.connect("DELETE", HTTP + "/tmp/test.dat")
await http.wait()
if (http.status != 204) {
    console.log("STATUS IS " + http.status)
}
ttrue(http.status == 204)

/*
    Options and Trace.

    optionsHandler answers both only when ME_GOAHEAD_STEALTH is off; with stealth on (the shipped
    default) it refuses everything with 406. The Allow assertion below used to be commented out
    because it can never hold in a stealth build - assert whichever contract the build actually
    has, so the non-stealth path is covered rather than silently untested. TRACE echoing the
    request is a cross-site-tracing surface and is worth asserting precisely when it is enabled.
 */
const stealth = thas('ME_GOAHEAD_STEALTH')

//  Options
http.connect("OPTIONS", HTTP + "/index.html")
await http.wait()
if (stealth) {
    ttrue(http.status == 406)
} else {
    ttrue(http.status == 200)
    const allow = http.header("allow")
    ttrue(allow != null && allow.contains("GET") && allow.contains("OPTIONS"),
        "OPTIONS did not advertise an Allow header: " + allow)
}

//  Trace
http.connect("TRACE", HTTP + "/index.html")
await http.wait()
if (stealth) {
    ttrue(http.status == 406)
} else {
    ttrue(http.status == 200)
    ttrue(http.response.contains("TRACE"), "TRACE did not echo the request line")
}

//  Post
http.post(HTTP + "/index.html", "Some data")
await http.wait()
ttrue(http.status == 200)

http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)

//  Head
http.connect("HEAD", HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
ttrue(http.header("content-length") > 0)
ttrue(http.response == "")
