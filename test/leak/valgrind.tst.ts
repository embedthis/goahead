/*
    valgrind.tst - Valgrind tests on Unix-like systems
 */

import { Http, Cmd, App, Config } from 'ejscript'
import { tdepth, thas, tskip, ttrue } from 'testme'

let PORT = 4150
let valgrind = Cmd.locate('valgrind')

if (Config.OS == 'linux' && tdepth() >= 4 && valgrind) {
    let host = '127.0.0.1:' + PORT

    let httpCmd = testme.bin.join('http').portable + ' -q --zero '
    let goahead = testme.bin.join('goahead-test').portable + ' --name api.valgrind'
    valgrind += ' -q --tool=memcheck --leak-check=yes --suppressions=../../../build/bin/mpr.supp ' + goahead + ' -v'

    //  Run http
    function run(args): String {
        try {
            // console.log(httpCmd, args)
            let cmd = Cmd(httpCmd + args)
            if (cmd.status != 0) {
                console.log('STATUS ' + cmd.status)
                console.log(cmd.error)
                console.log(cmd.response)
            }
            ttrue(cmd.status == 0)
            return cmd.response
        } catch (e) {
            ttrue(false, e)
        }
        return null
    }
    /*
        Start valgrind and wait till ready
     */
    // console.log('VALGRIND CMD: ' + valgrind)
    let cmd = Cmd(valgrind, {detach: true})
    let http
    for (let i = 0; i < 10; i++) {
        http = new Http
        try {
            http.get(host + '/index.html')
            await http.wait()
            if (http.status == 200) break
        } catch (e) {}
        App.sleep(1000)
        http.close()
    }
    if (http.status != 200) {
        throw 'Cannot start goahead-test for valgrind'
    }
    run('-i 100 ' + PORT + '/index.html')
    if (thas('ME_GOAHEAD_CGI')) {
        run(PORT + '/test.cgi')
    }
    run(PORT + '/exit.esp')
    let ok = cmd.wait(10000)
    if (cmd.status != 0) {
        App.log.error('valgrind status: ' + cmd.status)
        App.log.error('valgrind error: ' + cmd.error)
        App.log.error('valgrind output: ' + cmd.response)
    }
    ttrue(cmd.status == 0)
    cmd.stop()

} else {
    if (Config.OS == 'linux' && !valgrind) {
        tskip('Run with valgrind installed')
    } else {
        tskip('Run on Linux at depth 4 with valgrind installed')
    }
}
