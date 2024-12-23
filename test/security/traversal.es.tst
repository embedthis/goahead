/*
    Test directory traversal
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let status

http = new Http
http.get(HTTP + "/../auth.conf")
assert(http.status == 400)
http.close()

http.get(HTTP + "/../../index.html")
assert(http.status == 400)
http.close()

/* Test windows '\' delimiter */
http.get(HTTP + "/..%5Cauth.conf")
assert(http.status == 400)
http.close()

http.get(HTTP + "/../../../../../.x/.x/.x/.x/.x/.x/etc/passwd")
assert(http.status == 400)
http.close()

