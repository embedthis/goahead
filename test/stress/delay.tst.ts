/*
    Test various protocol delays
 */

import { Socket, ByteArray, Uri, App } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const httpUri = new Uri(HTTP)
const DELAY  = 500

let s = new Socket
let count = 0
let response
let n

//  Connect and delay
s.connect(httpUri.host + ":" + httpUri.port)
await App.sleep(DELAY)

//  Continue with delay part way through the first line
count += await s.write("GET")
ttrue(count == 3)
await App.sleep(DELAY)
count += await s.write(" /index.html HTTP/1.0\r\n\r\n")
ttrue(count > 10)
response = new ByteArray(8192)
for (count = 0; (n = await s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()

//  Delay before headers
s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
count = await s.write("GET")
ttrue(count == 3)
await App.sleep(DELAY)
count += await s.write(" /index.html HTTP/1.0\r\n\r\n")
ttrue(count > 10)
response = new ByteArray(8192)
for (count = 0; (n = await s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()

//  Delay after one <CR>
response = new ByteArray(8192)
s = new Socket
s.connect(httpUri.host + ":" + httpUri.port)
count = await s.write("GET")
ttrue(count == 3)
count += await s.write(" /index.html HTTP/1.0\r\n")
await App.sleep(DELAY)
count += await s.write("\r\n")
ttrue(count > 10)
response = new ByteArray(8192)
for (count = 0; (n = await s.read(response, -1)) != null; count += n) { }
ttrue(response.toString().contains('200 OK'))
ttrue(response.toString().contains('Hello /index'))
s.close()
