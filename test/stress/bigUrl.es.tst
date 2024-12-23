/*
    bigUrl.tst - Stress test very long URLs 
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let depth = tdepth() || 4

let http: Http = new Http

// Create a very long query
let queryPart = ""
for (i in 100) {
    queryPart += + "key" + i + "=" + 1234567890 + "&"
}

//  Vary up the query length based on the depth

for (iter in depth) {
    let query = ""
    for (i in 5 * (iter + 3)) {
        query += queryPart + "&"
    }
    query = query.trim("&")

    // Test /index.html
    http.get(HTTP + "/index.html?" + query)

    /*
        On windows, may get a connection reset as the server may respond before reading all the URL data
     */
    let status
    try {
        status = http.status
        if (query.length < 2000) {
            ttrue(http.status == 200)
            ttrue(http.response.contains("Hello /index.html"))
        } else {
            if (http.status != 413) {
                print('STATUS', http.status)
                dump('HEADERS', http.headers)
                print('response', http.response)
            }
            ttrue(http.status == 413)
        }
    } catch (e) {
        ttrue(e.message.contains('Connection reset'))
    }
    http.close()
}
