/* Early failures have error status; no failure exposes template source. */
import {Http} from '@embedthis/ejscript'
import {tget, thas, tskip, ttrue, tcontains} from '@embedthis/testme'
const HTTP = tget('TM_HTTP')
if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JST disabled')
} else {
    for (const [name, status] of [['errorfirst',500], ['errorlate',200], ['errorunterm',500], ['asp0',400]]) {
        const http = new Http
        http.get(HTTP + '/jst/' + name + '.jst')
        await http.wait()
        ttrue(http.status == status)
        ttrue(!http.response.includes('PRIVATE_TEMPLATE_MARKER'))
        ttrue(!http.response.includes('missingSecret'))
        ttrue(!http.response.includes('<%'))
        if (name == 'errorlate') {
            tcontains(http.response, 'Before')
            tcontains(http.response, 'Javascript Error')
            ttrue(!http.response.includes('After'))
        }
    }
    const http = new Http
    http.get(HTTP + '/jst/emptyresult.jst')
    await http.wait()
    ttrue(http.status == 200)
    ttrue(http.response == '')
}
