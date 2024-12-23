/*
    stream.tst - Http tests using streams
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

http.get(HTTP + "/big.jst")
ts = new TextStream(http)
lines = ts.readLines()
ttrue(lines.length == 801)
ttrue(lines[0].contains("aaaaabbb") && lines[0].contains("0 "))
ttrue(lines[799].contains("aaaaabbb") && lines[799].contains("799"))
