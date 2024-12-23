/*
    Uri validation
 */

const HTTP: Uri = tget('TM_HTTP') || "127.0.0.1:8080"

function get(uri): String {
    let s = new Socket
    s.connect(HTTP.address)
    let count = 0
    try {
        count += s.write("GET " + uri + " HTTP/1.0\r\n\r\n")
    } catch {
        tfail("Write failed. Wrote  " + count + " of " + data.length + " bytes.")
    }
    let response = new ByteArray
    while ((n = s.read(response, -1)) != null ) {}
    s.close()
    return response.toString()
}

let response
response = get('index.html')
ttrue(response.toString().contains('Bad Request'))

response = get('/\x01index.html')
ttrue(response.toString().contains('Bad Request'))

response = get('\\index.html')
ttrue(response.toString().contains('Bad Request'))
