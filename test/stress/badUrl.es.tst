/*
    badUrl.tst - Stress test malformed URLs 
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

let caught
try {
    /* Http.get will throw exception for a bad url */
    http.get(HTTP + "/index\x01.html")
} catch {
    caught = true
}
ttrue(caught)
http.close()

//  Bypass http to send the request to the server

let s = new Socket
s.connect(HTTP)
s.write("GET /index\x01.html HTTP/1.0\r\n\r\n")
let response = new ByteArray
while ((n = s.read(response, -1)) != null) {}
let r = response.toString()
ttrue(r.contains('400 Bad Request'))
s.close()
