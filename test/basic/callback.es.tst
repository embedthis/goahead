/*
    callback.tst - Http tests using callbacks
 */

if (false) {
    const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
    let http: Http = new Http

    //  Using a read callback
    http.setCallback(Http.Read, function (e) {
        if (e is HttpDataEvent) {
            if (e.eventMask == Http.Read) {
                data = http.readString()
            }
        } else if (e is HttpErrorEvent) {
            throw e
        } else {
            throw "Bad event in http callbac"
        }
    })
    http.get(HTTP + "/big.asp")
    http.wait()
    // print(http.status)

    //  Using a write callback
    writeCount = 5
    http.chunked = true
    http.setCallback(Http.Write, function (e) {
        if (e is HttpDataEvent) {
            // print("MASK " + e.eventMask)
            if (e.eventMask & Http.Write) {
                // print("WRITE DATA " + writeCount)
                if (writeCount-- > 0) {
                    http.write("WRITE DATA " + writeCount + " \n")
                } else {
                    http.write()
                }
            }
            if (e.eventMask & Http.Read) {
                print("READ EVENT ")
            }
        } else if (e is HttpErrorEvent) {
            throw e
        } else {
            throw "Bad event in http callbac"
        }
    })
    http.post(HTTP + "/f.asp")
    http.wait()
    // print("CODE " + http.status)
    // print("GOT  " + http.response.length + " bytes of response")
}
