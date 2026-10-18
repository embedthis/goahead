/* JST smoke coverage using the supported write() and named request variables. */
import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP')
async function fetch(path: string): Promise<Http> {
    const http = new Http
    http.setLimits({requestTimeout: 5})
    http.get(HTTP + path)
    await http.wait()
    return http
}
if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    for (const n of [0, 3]) {
        const http = await fetch(`/jst/smoke.jst?n=${n}`)
        ttrue(http.status == 200)
        ttrue(http.response.trim() == (n ? 'yes:012' : 'no:'))
    }
    let http = await fetch('/jst/query.jst?q=hello')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'q=[hello]')
    http = await fetch('/jst/nofunc.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Javascript Error')
    http = await fetch('/index.html')
    ttrue(http.status == 200)
}
