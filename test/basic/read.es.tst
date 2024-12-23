/*
    read.tst - Various Http read tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

//  Test http.read() into a byte array
http.get(HTTP + "/big.jst")
buf = new ByteArray
count = 0
while (http.read(buf) > 0) {
    count += buf.length
}
if (count != 61491) {
    print("COUNT IS " + count + " code " + http.status)
}
ttrue(count == 61491)
http.close()

http.get(HTTP + "/lines.txt")
lines = http.readLines()
for (l in lines) {
    line = lines[l]
    ttrue(line.contains("LINE"))
    ttrue(line.contains((l+1).toString()))
}
ttrue(http.status == 200)
