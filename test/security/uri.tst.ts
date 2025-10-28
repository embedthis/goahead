/*
    Uri validation
 */

import { Socket, ByteArray, Uri } from 'ejscript'
import { tfail, tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const httpUri = new Uri(HTTP)

async function get(uri): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ":" + httpUri.port)
    let count = 0
    try {
        count += await s.write("GET " + uri + " HTTP/1.0\r\n\r\n")
    } catch {
        tfail("Write failed. Wrote  " + count + " bytes.")
    }
    let response = new ByteArray(8192)
    let n
    while ((n = await s.read(response, -1)) != null ) {}
    s.close()
    return response.toString()
}

let response
response = await get('index.html')
ttrue(response.contains('Bad Request'))

response = await get('/\x01index.html')
ttrue(response.contains('Bad Request'))

response = await get('\\index.html')
ttrue(response.contains('Bad Request'))
