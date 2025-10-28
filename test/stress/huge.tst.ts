/*
    huge.tst - Huge get file
 */

import { Http, Path, ByteArray, File } from 'ejscript'
import { tdepth, tget, tskip, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const TIMEOUT = 10000
const HUGE= "../web/huge.txt"
var SIZE = 5 * 1024 * 1024 * 1024
let http: Http = new Http

if (tdepth() >= 5) {
    /*
        Create test file
     */
    if (!await Path(HUGE).exists || await Path(HUGE).size < SIZE) {
        App.log.write("\n  [Generate] Huge 5GB test file \"" + HUGE + "\" ...")
        let data = new ByteArray
        for (let i = 0; i < 1024; i++) {
            data.write("%05d 0123456789012345678901234567890123456789012345678", i)
        }
        let f = File(HUGE, "w")
        while (await Path(HUGE).size < SIZE) {
            f.write(data)
        }
        f.close()
        App.log.write("\n      [Test] GET \"" + HUGE + "\" ...")
    }
    SIZE = await Path(HUGE).size

    /*
        Test Http get for a huge file
     */
    let total, count, complete
    http.async = true
    http.setLimits({receive: 10 * 1024 * 1024 * 1024})
    var buf = new ByteArray
    http.on("readable", function (event, http) {
        buf.flush()
        count = http.read(buf, -1)
        total += count
        // printf("Read %d total %5.2f %%\n", count, total / SIZE * 100.0)
    })
    http.on("close", function (event, http) {
        complete = true
    })
    http.get(HTTP + "/huge.txt")
    await http.wait()
    let mark = new Date
    ttrue(complete)
    ttrue(total == SIZE)
    ttrue(http.status == 200)
    http.close()

/*
    //  Get first 5 bytes
    http.setHeader("range", "bytes=0-4")
    await http.get(HTTP + "/big.txt")
    ttrue(http.status == 206)
    ttrue(http.response == "01234")
    http.close()


    //  Get last 5 bytes
    http.setHeader("range", "bytes=-5")
    await http.get(HTTP + "/big.txt")
    ttrue(http.status == 206)
    ttrue(http.response.trim() == "MENT")
    http.close()


    //  Get from specific position till the end
    http.setHeader("range", "bytes=117000-")
    await http.get(HTTP + "/big.txt")
    ttrue(http.status == 206)
    ttrue(http.response.trim() == "END OF DOCUMENT")
    http.close()


    //  Multiple ranges
    http.setHeader("range", "bytes=0-5,25-30,-5")
    await http.get(HTTP + "/big.txt")
    ttrue(http.status == 206)
    ttrue(http.response.contains("content-range: bytes 0-5/117016"))
    ttrue(http.response.contains("content-range: bytes 25-30/117016"))
    ttrue(http.response.contains("content-range: bytes 117011-117015/117016"))
    ttrue(http.response.contains("012345"))
    ttrue(http.response.contains("567890"))
    ttrue(http.response.contains("MENT"))
    http.close()
*/
} else {
  tskip("Runs at depth 6")
}
