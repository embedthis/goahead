/*
    digest.tst - Digest authentication http tests. Exercises the Digest branch of
    websVerifyPasswordFromFile (src/auth.c), including the regression case where an
    incorrect comparand silently fails every Digest round-trip (Bug 001 aftermath).
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let http: Http = new Http

if (thas('ME_GOAHEAD_AUTH')) {
    //  No credentials -> challenge
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Legacy (MD5-HA1) user with correct password -> success
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 200)

    //  Legacy (MD5-HA1) user with wrong password -> 401
    //  Regression test: Bug 001 fix compared expected response against stored HA1,
    //  which can never match — this case would have been rejected for the wrong reason.
    http.setCredentials("joshua", "wrongpass")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Unknown user -> 401 (exercises the early websLookupUser == 0 return)
    http.setCredentials("nobody", "anything")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Second valid user -> success (group/ability coverage for 'view')
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 200)

    //  Mary lacks 'manage' ability -> 401 for admin subtree
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/auth/digest/admin/index.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Empty password on digest path -> 401
    http.setCredentials("joshua", "")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        BF1-stored user authenticating via digest must be rejected explicitly
        rather than silently failing via the BF1 branch. Digest transmits an
        MD5-HA1 response hash and can never succeed against a bcrypt hash.
        testbf1 has a BF1 stored password and the 'view' ability.
     */
    http.setCredentials("testbf1", "pass1")
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        parseDigestDetails robustness. Send crafted Authorization: Digest headers
        directly (no credentials set, so no client-side challenge/response kicks in).
        These exercise server-side parsing and validation, which the client-driven
        tests above cannot reach because the client will not send an invalid header.
     */
    //  Missing required nonce field -> parseDigestDetails rejects.
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", response="deadbeef", uri="/auth/digest/digest.html"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Malformed (non-base64) nonce -> parseDigestNonce fails.
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="!!!not-base64!!!", ' +
        'response="x", uri="/", qop="auth", nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Wrong realm in the client response -> reject.
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="wrong.example", nonce="abc", ' +
        'response="x", uri="/", qop="auth", nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        Oversize field values just under ME_GOAHEAD_LIMIT_HEADER (2048). These
        exercise parseDigestDetails' per-field sclone path for buffers larger
        than any fixed-size stack allocation in calcDigest (256 bytes). The
        server must not crash and must reject with 401.
     */
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="abc", ' +
        'response="' + 'a'.repeat(1500) + '", uri="/", qop="auth", ' +
        'nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    http.reset()
    http.setHeader("Authorization",
        'Digest username="' + 'a'.repeat(1500) + '", realm="example.com", ' +
        'nonce="abc", response="x", uri="/", qop="auth", nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Garbage after the Digest scheme -> no recognised fields, reject.
    http.reset()
    http.setHeader("Authorization", 'Digest nothing=valid here at all')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        Backslash unescape robustness (Bug 018, CWE-125). parseDigestDetails
        unescapes backslash sequences in field values. A field value ending in a
        lone backslash previously double-advanced the source pointer past the NUL
        terminator, reading past the heap allocation. The server must unescape
        safely, reject (401), and stay alive (verified by the liveness check below).
     */
    //  Field value that is a single lone backslash (offset 0 -> the OOB trigger).
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="abc", ' +
        'response="x", uri="/", qop="auth", nc="00000001", cnonce="\\"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Trailing backslash at an even offset ("ab\") -> also exercises the OOB path.
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="abc", ' +
        'response="x", uri="/", qop="auth", nc="00000001", cnonce="ab\\"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        Timestamp validation in parseDigestDetails. The nonce format is
        base64("hash:hex_time:hex_counter"). parseDigestNonce splits on ':',
        so we can craft a nonce whose timestamp falls outside the valid window
        without knowing the server's masterSecret. Timestamp checks run before
        the hash check.
     */
    //  Future nonce (hextime 0xffffffff = year 2106) -> rejected as future timestamp.
    let futureNonce = btoa("x:ffffffff:0")
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="' + futureNonce + '", ' +
        'response="x", uri="/", qop="auth", nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Stale nonce (hextime 0 = 1970) -> rejected as stale (past NONCE_DURATION).
    let staleNonce = btoa("x:0:0")
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="' + staleNonce + '", ' +
        'response="x", uri="/", qop="auth", nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        qop="auth-int" is not supported; parseDigestDetails rejects any qop
        other than "auth". Pins this invariant as a regression guard so adding
        auth-int support is a deliberate change, not an accidental one.
     */
    http.reset()
    http.setHeader("Authorization",
        'Digest username="joshua", realm="example.com", nonce="abc", ' +
        'response="x", uri="/", qop="auth-int", nc="00000001", cnonce="c"')
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    /*
        Empty-field tests that require a server-authentic nonce. Issue an
        unauthenticated request first to harvest a real nonce from the
        challenge, then forge Authorization headers that pass nonce validation
        but have an empty field the verifier must reject.
     */
    http.reset()
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)
    let wwwAuth = http.header('www-authenticate')
    let nonceMatch = wwwAuth.match(/nonce="([^"]+)"/)
    ttrue(nonceMatch != null)
    let realNonce = nonceMatch[1]

    //  Empty response (digest) -> pmatch mismatch against computed HA1 -> 401.
    http.reset()
    http.setHeader("Authorization",
        `Digest username="joshua", realm="example.com", nonce="${realNonce}", ` +
        `response="", uri="/auth/digest/digest.html", qop="auth", nc="00000001", cnonce="c"`)
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Empty username -> websLookupUser("") returns 0 -> 401.
    http.reset()
    http.setHeader("Authorization",
        `Digest username="", realm="example.com", nonce="${realNonce}", ` +
        `response="x", uri="/auth/digest/digest.html", qop="auth", nc="00000001", cnonce="c"`)
    http.get(HTTP + "/auth/digest/digest.html")
    await http.wait()
    ttrue(http.status == 401)

    //  Liveness: server still alive after all the oversize/malformed cases above.
    http.reset()
    http.setCredentials("", "")
    http.get(HTTP + "/index.html")
    await http.wait()
    ttrue(http.status == 200)
}
