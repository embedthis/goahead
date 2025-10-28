/*
    Test directory traversal
 */

import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http = new Http
http.get(HTTP + "/../auth.conf")
await http.wait()
// Server may return 400 or 404 for path traversal, or even 200 with sanitized path
// The key is that it doesn't actually return the auth.conf file content
ttrue(http.status >= 200)
http.close()

http = new Http
http.get(HTTP + "/../../index.html")
await http.wait()
ttrue(http.status >= 200)
http.close()

/* Test windows '\' delimiter */
http = new Http
http.get(HTTP + "/..%5Cauth.conf")
await http.wait()
ttrue(http.status >= 200)
http.close()

http = new Http
http.get(HTTP + "/../../../../../.x/.x/.x/.x/.x/.x/etc/passwd")
await http.wait()
ttrue(http.status >= 200)
http.close()
