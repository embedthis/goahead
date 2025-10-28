/*
    callback.tst - Http tests using callbacks
 */

import { Http } from 'ejscript'
import { tget } from 'testme'

if (false) {
    const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
    let http: Http = new Http

    //  Using a read callback
    http.setCallback(Http.Read, function (e: any) {
        if (typeof e === 'object' && 'eventMask' in e) {
            if (e.eventMask == Http.Read) {
                const data = http.readString()
            }
        } else if (e instanceof Error) {
            throw e
        } else {
            throw "Bad event in http callbac"
        }
    })
    http.get(HTTP + "/big.asp")
    await http.wait()
    // console.log(http.status)

    //  Using a write callback
    let writeCount = 5
    http.chunked = true
    http.setCallback(Http.Write, function (e: any) {
        if (typeof e === 'object' && 'eventMask' in e) {
            // console.log("MASK " + e.eventMask)
            if (e.eventMask & Http.Write) {
                // console.log("WRITE DATA " + writeCount)
                if (writeCount-- > 0) {
                    http.write("WRITE DATA " + writeCount + " \n")
                } else {
                    http.write()
                }
            }
            if (e.eventMask & Http.Read) {
                console.log("READ EVENT ")
            }
        } else if (e instanceof Error) {
            throw e
        } else {
            throw "Bad event in http callbac"
        }
    })
    http.post(HTTP + "/f.asp")
    await http.wait()
    // console.log("CODE " + http.status)
    // console.log("GOT  " + http.response.length + " bytes of response")
}
