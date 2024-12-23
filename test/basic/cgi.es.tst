/*
    cgi.tst - CGI tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http: Http = new Http

if (thas('ME_GOAHEAD_CGI')) {
    /* Suport routines */

    function contains(pat): Void {
        ttrue(http.response.contains(pat))
    }

    function keyword(pat: String): String? {
        pat.replace(/\//, "\\/").replace(/\[/, "\\[")
        let reg = RegExp(".*" + pat + "=([^<]*).*", "s")
        return http.response.replace(reg, "$1")
    }

    function match(key: String, value: String): Void {
        if (keyword(key) != value) {
            print(http.response)
            print("\nKey \"" + key + "\"")
            print("Expected: " + value)
            // print("Actual  : " + keyword(value))
        }
        ttrue(keyword(key) == value)
    }


    /* Tests */

    function forms() {
        //  Test various forms to invoke cgi programs
        http.get(HTTP + "/cgi-bin/cgitest")
        ttrue(http.status == 200)
        contains("cgitest: Output")
        http.close()

        if (Config.OS == "windows") {
            http.get(HTTP + "/cgi-bin/cgitest.exe")
            ttrue(http.status == 200)
            contains("cgitest: Output")
            http.close()
        }
    }

    function extraPath() {
        http.get(HTTP + "/cgi-bin/cgitest")
        ttrue(http.status == 200)
        match("PATH_INFO", "")
        match("PATH_TRANSLATED", "")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest/extra/path")
        match("PATH_INFO", "/extra/path")
        let scriptFilename = keyword("SCRIPT_FILENAME")
        let path = Path(scriptFilename).dirname.join("extra/path")
        let translated = Path(keyword("PATH_TRANSLATED"))
        ttrue(path == translated)
        http.close()
    }

    function query() {
        http.get(HTTP + "/cgi-bin/cgitest/extra/path?a=b&c=d&e=f")
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("PATH_INFO", "/extra/path")
        contains("QVAR a=b")
        contains("QVAR c=d")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a+b+c")
        match("QUERY_STRING", "a+b+c")
        contains("QVAR a b c")
        http.close()

        //
        //  Query string vars should not be turned into variables for GETs
        //  Extra path only supported for cgi programs with extensions.
        //
        http.get(HTTP + "/cgi-bin/cgitest/extra/path?var1=a+a&var2=b%20b&var3=c")
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
        match("QUERY_STRING", "var1=a+a&var2=b%20b&var3=c")
        match("QVAR var1", "a a")
        match("QVAR var2", "b b")
        match("QVAR var3", "c")
        match("PVAR name", "Peter")
        match("PVAR address", "777 Mulberry Lane")
        http.close()
    }

    function encoding() {
        http.get(HTTP + "/cgi-bin/cgitest/extra%20long/a/../path/a/..?var%201=value%201")
        match("QUERY_STRING", "var%201=value%201")
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("QVAR var 1", "value 1")
        match("PATH_INFO", "/extra long/path")

        let scriptFilename = keyword("SCRIPT_FILENAME")
        let path = Path(scriptFilename).dirname.join("extra long/path")
        let translated = Path(keyword("PATH_TRANSLATED"))
        ttrue(path == translated)
        http.close()
    }

    function status() {
        let http = new Http
        http.setHeader("SWITCHES", "-s%20711")
        http.get(HTTP + "/cgi-bin/cgitest?pat=111")
        ttrue(http.status == 711)
        http.close()
    }

    function location() {
        let http = new Http
        http.setHeader("SWITCHES", "-l%20/index.html")
        http.followRedirects = false
        http.get(HTTP + "/cgi-bin/cgitest")
        ttrue(http.status == 302)
        http.close()
    }

    function quoting() {
        http.get(HTTP + "/cgi-bin/cgitest?a+b+c")
        match("QUERY_STRING", "a+b+c")
        match("QVAR a b c", "")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a=1&b=2&c=3")
        match("QUERY_STRING", "a=1&b=2&c=3")
        match("QVAR a", "1")
        match("QVAR b", "2")
        match("QVAR c", "3")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a%20a=1%201+b%20b=2%202")
        match("QUERY_STRING", "a%20a=1%201+b%20b=2%202")
        match("QVAR a a", "1 1 b b=2 2")
        http.close()

        http.get(HTTP + "/cgi-bin/cgitest?a%20a=1%201&b%20b=2%202")
        match("QUERY_STRING", "a%20a=1%201&b%20b=2%202")
        match("QVAR a a", "1 1")
        match("QVAR b b", "2 2")
        http.close()
    }

    function post() {
        //  Simple post
        http.post(HTTP + '/cgi-bin/cgitest', 'Some data')
        ttrue(http.status == 200)
        match('CONTENT_LENGTH', '9')
        http.close()

        //  Simple form
        http.form(HTTP + '/cgi-bin/cgitest', {name: 'John', address: '700 Park Ave'})
        ttrue(http.status == 200)
        match('PVAR name', 'John')
        match('PVAR address', '700 Park Ave')
        http.close()
    }

    forms()
    extraPath()
    query()
    encoding()
    status()
    location()
    quoting()
    post()

} else {
    tskip("CGI not enabled")
}
