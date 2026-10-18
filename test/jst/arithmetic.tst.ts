/* Request-supplied integer overflow must fail and leave later requests usable. */
import {Http} from '@embedthis/ejscript'
import {tget, thas, tskip, ttrue} from '@embedthis/testme'
if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JST disabled')
} else {
    for (const n of ['2147483647', '999999999999999999999999', '2147483646']) {
        const http = new Http
        http.get(tget('TM_HTTP') + '/jst/arithmetic.jst?n=' + n)
        await http.wait()
        ttrue(http.status == (n == '2147483646' ? 200 : 500))
        if (http.status == 200) ttrue(http.response == '2147483647')
        else ttrue(!http.response.includes('var value'))
    }
    for (const op of ['divide', 'modulo']) {
        const http = new Http
        http.get(tget('TM_HTTP') + `/jst/${op}.jst?a=-2147483648&b=-1`)
        await http.wait()
        if (op == 'divide') ttrue(http.status == 500)
        else ttrue(http.status == 200 && http.response == '0')
        http.close()
        const after = new Http
        after.get(tget('TM_HTTP') + '/index.html')
        await after.wait()
        ttrue(after.status == 200)
        after.close()
    }
}
