/*
    Very large URI test
 */

import { Http, Socket, ByteArray, Uri } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const httpUri = new Uri(HTTP)

//  This writes a ~100K URI. LimitUri should be less than 100K for this unit test.

let data = "/"
for (let i = 0; i < 1000; i++) {
    data += "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678\n"
}

/*
    Test LimitUri
 */
let s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
let count = 0
try {
    count += await s.write("GET ")
    count += await s.write(data)
    count += await s.write(" HTTP/1.1\r\n\r\n")
} catch {
    // App.log.error("Write failed. Wrote  " + count + " of " + data.length + " bytes.")
}

/* Server should just close the connection without a response */
let response = new ByteArray(8192)
let n
while ((n = await s.read(response, -1)) != null) { }
if (response.length > 0) {
    /* May not get a response if the write above fails. Then we get a conn reset */
    ttrue(response.toString().contains('413 Request too large'))
}
s.close()

//  Check server still up
let http = new Http
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
http.close()
