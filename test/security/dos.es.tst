/*
    Denial of service testing
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

//  Check server available
http = new Http
http.get(HTTP + "/index.html")
ttrue(http.status == 200)
http.close()

if (tdepth() >= 3) {
    if (Config.OS != 'windows') {
        //  Try to crash with DOS attack
        for (i in 200) {
            let s = new Socket
            try {
                s.connect(HTTP.address)
            } catch (e) {
                print("ERROR", i)
                print(e)
                throw e
            }
            let written = s.write("Any Text")
            ttrue(written == 8)
            s.close()
        }
    }

    //  Check server still there
    http = new Http
    http.get(HTTP + "/index.html")
    ttrue(http.status == 200)
    http.close()
} else {
    tskip("Runs at depth 3");
}
