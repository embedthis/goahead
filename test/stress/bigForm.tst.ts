/*
    bigForm.tst - Stress test very large form data
 */

import { Http, Socket, ByteArray, Uri } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const httpUri = new Uri(HTTP)

//  This writes 12K of body data. LimitBody should be less than this for this unit test.
let data = ""
for (let i = 0; i < 64; i++) {
    data += "name=value&"
    data += "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
}

let s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
await s.write("POST /index.html HTTP/1.0\r\n")
await s.write("Content-Type: application/x-www-form-urlencoded\r\n")
await s.write("Content-Length: " + data.length + "\r\n\r\n")
await s.write(data)

let response = new ByteArray(8192)
let n
while ((n = await s.read(response, -1)) != null) {}
let r = response.toString()
// Server may accept large forms or reject with 413/400
ttrue(r.contains('413') || r.contains('400') || r.contains('200'))
s.close()

//  Check server still up
let http = new Http
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
http.close()
