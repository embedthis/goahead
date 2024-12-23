/*
    bigForm.tst - Stress test very large form data (triggers chunking)
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

let form = {}
for (i in 100) {
    form["field_" + i] = Date.ticks
}

http.form(HTTP + "/action/test", form)
ttrue(http.status == 200)
ttrue(http.response.contains("name: null"))
http.close()
