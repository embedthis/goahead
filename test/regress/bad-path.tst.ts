/*
    Test a bad path 'sip:nm'
 */

import { Socket, ByteArray, Uri } from 'ejscript'
import { tget } from 'testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)

let s = new Socket

s.connect(httpUri.host + ":" + httpUri.port)
await s.write('OPTIONS sip:nm SIP/2.0\r\nContent-Length: 0\r\nContact: \r\nAccept: application/*\r\n\r\n')

let response = new ByteArray(8192)
let n
while ((n = await s.read(response, -1)) != null) {}
s.close()
