/*
    Test various whitespace
 */

import { Socket, ByteArray, Uri } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)
const DELAY  = 500

let s
let count = 0
let n

//  Leading white space
s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
count += await s.write(' GET /index.html HTTP/1.0\r\n\r\n')
ttrue(count > 0)
let response = new ByteArray(8192)
for (count = 0; (n = await s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()

//  white space after method
s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
count += await s.write('GET         /index.html HTTP/1.0\r\n\r\n')
ttrue(count > 0)
response = new ByteArray(8192)
for (count = 0; (n = await s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()

//  white space after URI
s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
count += await s.write('GET /index.html      HTTP/1.0\r\n\r\n')
ttrue(count > 0)
response = new ByteArray(8192)
for (count = 0; (n = await s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()
