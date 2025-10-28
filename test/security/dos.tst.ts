/*
    dos.tst - Test DOS attack vectors
 */

import { Http, Socket, Config } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

//  Check server available
let http = new Http
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
http.close()

if (Config.OS != "windows") {
    //  Try to create a bunch of dangling sockets. Should not be able to DOS the server
    // Note: Connecting to wrong port intentionally - these should fail quickly
    let i
    for (let i = 0; i < 200; i++) {
        try {
            let s = new Socket
            s.setTimeout(10) // 10ms timeout
            s.connect("127.0.0.1:18080")
        } catch {}
    }
    // Give server time to process the connection attempts
    await new Promise(resolve => setTimeout(resolve, 100))

    //  Check server still operating
    http = new Http
    http.get(HTTP + "/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()
}
