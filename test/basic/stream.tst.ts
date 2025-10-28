/*
    stream.tst - Http tests using streams
 */

import { Http, TextStream } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

http.get(HTTP + "/big.jst")
await http.wait()
const ts = new TextStream(http)
const lines = await ts.readLines()
ttrue(lines.length == 801)
ttrue(lines[0].contains("aaaaabbb") && lines[0].contains("0 "))
ttrue(lines[799].contains("aaaaabbb") && lines[799].contains("799"))
