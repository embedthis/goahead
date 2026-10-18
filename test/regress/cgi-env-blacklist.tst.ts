/*
    cgi-env-blacklist.tst - The CGI environment filter must screen values, not just names.

    Four defects in the env-build filter, all observable through the CGI environment dump:

    1. The Shellshock guard tested strstr(name, "=()"). A name can never contain '=', so the
       test was dead code that could not fire. The payload is carried in the value and begins
       "() {", so the test belongs on the value.
    2. A name that carries its own '=' composes a second assignment inside the "name=value"
       string, defining a variable under a name the name-based filters never see.
    3. The interpreter/loader set omitted the modern hijack variables (NODE_PATH, PHPRC,
       PHP_INI_SCAN_DIR, GEM_HOME, LUA_PATH, GLIBC_TUNABLES).
    4. REMOTE_HOST was blacklisted although the server sets it itself, silently withholding a
       variable RFC 3875 lists as mandatory.

    The /blacklist-test route in src/test.c sets each probe with websSetVar() and forwards to
    /cgi-bin/cgitest, which dumps every variable it received as "<P>NAME=VALUE</P>". Absence is
    asserted on the literal "NAME=" text. The probes are set with arg == 0 (the server-trusted
    flag) on purpose: the filter must screen server-set variables too, which is also what keeps
    the httpoxy mitigation in test/basic/cgi.tst.ts working.
 */

import { Http, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

async function get(uri: string): Promise<Http> {
    let h = new Http
    h.get(HTTP + uri)
    await h.wait()
    return h
}

if (thas('ME_GOAHEAD_CGI')) {
    let h = await get('/blacklist-test/anything')
    ttrue(h.status == 200, 'the blacklist probe route runs the CGI program')
    ttrue(h.response.contains('BLACKLIST_SENTINEL=ok'), 'an unfiltered variable still propagates')

    /*
        Defect 1: the Shellshock guard must fire on the value. Before the fix the probe name
        held no '=', the dead "=()" name test could not match, and the function definition was
        handed to the child intact.
     */
    ttrue(!h.response.contains('SHELLSHOCK_PROBE='), 'a "() {" value must not reach the CGI env')
    ttrue(!h.response.contains('shellshock-executed'), 'the Shellshock payload must not execute')

    /*
        Defect 2: a name containing '=' smuggles an assignment past the name-based filters.
        cgitest would otherwise print "SMUGGLED=SMUGGLE_TARGET=/tmp/evil-smuggled".
     */
    ttrue(!h.response.contains('SMUGGLE_TARGET'), 'a name containing "=" must not reach the CGI env')

    /*
        Defect 3: modern interpreter and loader hijack variables.
     */
    for (let name of ['NODE_PATH', 'PHPRC', 'PHP_INI_SCAN_DIR', 'GEM_HOME', 'LUA_PATH', 'GLIBC_TUNABLES']) {
        ttrue(!h.response.contains(name + '='), name + ' must be filtered from the CGI env')
    }

    /*
        Defect 4: REMOTE_HOST is set by the server and is mandatory under RFC 3875 section 4.1.9.
        Blacklisting it withheld it from every CGI program. It must be present, and it must carry
        the client address the server assigned rather than anything the client supplied.
     */
    h = await get('/cgi-bin/cgitest')
    ttrue(h.status == 200, 'the CGI program runs')
    ttrue(h.response.contains('REMOTE_HOST='), 'REMOTE_HOST is mandatory under RFC 3875')
    let host = new Uri(HTTP).host
    ttrue(h.response.contains('REMOTE_HOST=' + host), 'REMOTE_HOST carries the client address')
    ttrue(h.response.contains('REMOTE_ADDR=' + host), 'REMOTE_ADDR is unchanged')

} else {
    tskip('CGI not enabled')
}
