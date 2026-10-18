/* Registered C action dispatch and unknown-action handling. */
import { Http } from '@embedthis/ejscript'
import { tget, ttrue, tcontains } from '@embedthis/testme'
const HTTP = tget('TM_HTTP')
let http = new Http
http.form(HTTP + '/action/test', {name: 'Smoke', address: 'Test Lane'})
await http.wait()
ttrue(http.status == 200)
tcontains(http.response, 'name: Smoke')
tcontains(http.response, 'address: Test Lane')
http = new Http
http.get(HTTP + '/action/noSuchSmokeAction')
await http.wait()
ttrue(http.status == 404)
