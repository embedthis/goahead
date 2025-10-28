/*
    bigUrl.tst - Stress test very long URLs
 */

import { Http, Socket, ByteArray, Uri } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const httpUri = new Uri(HTTP)

//  This writes a ~13K URI. LimitUri should be less than this for this unit test.
let data = "/"
for (let i = 0; i < 64; i++) {
    data += "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
}

let s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
await s.write("GET " + data + " HTTP/1.0\r\n\r\n")
let response = new ByteArray(8192)
let n
while ((n = await s.read(response, -1)) != null) {}
let r = response.toString()
ttrue(r.contains('414') || r.contains('413') || r.contains('400'))
s.close()

//  Check server still up
let http = new Http
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
http.close()
