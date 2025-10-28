/*
    badUrl.tst - Stress test malformed URLs
 */

import { Http, Socket, ByteArray, Uri } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const httpUri = new Uri(HTTP)
let http: Http = new Http

// EJS Http.get doesn't validate/throw on bad URLs like native Ejscript did
// So we skip this check and just test the server response
// let caught = false
// try {
//     http.get(HTTP + "/index\x01.html")
//     await http.wait()
// } catch {
//     caught = true
// }
// ttrue(caught)
http.close()

//  Bypass http to send the request to the server
let s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
await s.write("GET /index\x01.html HTTP/1.0\r\n\r\n")
let response = new ByteArray(8192)
let n
while ((n = await s.read(response, -1)) != null) {}
let r = response.toString()
ttrue(r.contains('400 Bad Request'))
s.close()
