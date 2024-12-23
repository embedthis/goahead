/*
    Test various whitespace
 */

const HTTP: Uri = tget('TM_HTTP') || '127.0.0.1:8080'
const DELAY  = 500

let s
let count = 0

//  Leading white space
s = new Socket
s.connect(HTTP.address)
count += s.write(' GET /index.html HTTP/1.0\r\n\r\n')
ttrue(count > 0)
for (count = 0; (n = s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()

//  white space after method
s = new Socket
s.connect(HTTP.address)
count += s.write('GET         /index.html HTTP/1.0\r\n\r\n')
ttrue(count > 0)
for (count = 0; (n = s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()

//  white space after URI
s = new Socket
s.connect(HTTP.address)
count += s.write('GET /index.html      HTTP/1.0\r\n\r\n')
ttrue(count > 0)
for (count = 0; (n = s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()
