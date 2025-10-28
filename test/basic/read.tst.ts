/*
    read.tst - Various Http read tests
 */

import { Http, ByteArray } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

//  Test http.read() into a byte array
http.get(HTTP + "/big.jst")
await http.wait()
const buf = new ByteArray
let count = 0
while ((await http.read(buf)) > 0) {
    count += buf.length
}
if (count != 61491) {
    console.log("COUNT IS " + count + " code " + http.status)
}
ttrue(count == 61491)
http.close()

http.get(HTTP + "/lines.txt")
await http.wait()
const lines = await http.readLines()
for (const l in lines) {
    const line = lines[l]
    ttrue(line.contains("LINE"))
    ttrue(line.contains((parseInt(l)+1).toString()))
}
ttrue(http.status == 200)
