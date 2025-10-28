/*
    methods.tst - Test misc Http methods
 */

import { Http, Path } from 'ejscript'
import { tget, ttrue } from 'testme'

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

//  Options
http.connect("OPTIONS", HTTP + "/index.html")
await http.wait()
ttrue(http.status == 406)
// ttrue(http.header("allow") == "DELETE,GET,HEAD,OPTIONS,POST,PUT")

//  Trace - should be disabled
http.connect("TRACE", HTTP + "/index.html")
await http.wait()
ttrue(http.status == 406)

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
