/*
    format-attribute.tst - Regression test for printf format-string checking.

    The build has carried -Wformat -Wformat-security since forever, but the flags were structurally
    inert: PRINTF_ATTRIBUTE was applied to exactly one declaration (bufPut), so the compiler had
    nothing to check the other printf-family wrappers against. That is how the attacker-controlled
    format string in the CGI Content-Type path shipped with the gate nominally enabled, and why
    integrators compiling against dist/goahead.h got no format checking either.

    A printf-style declaration needs TWO annotations, because GCC/Clang and MSVC take the same
    information in different syntactic positions: PRINTF_ATTRIBUTE(a, b) after the parameter list,
    and PRINTF_FORMAT before the format parameter (SAL _Printf_format_string_, which drives MSVC's
    C4774). Either one alone leaves a toolchain unchecked, so both are asserted on every wrapper.

    Two suites:
      A) Structural - every varargs printf-family declaration in the public headers, and in the
         amalgamated dist/ copies integrators actually compile against, carries both annotations.
      B) Live gate - drive the real compiler over probe translation units and assert it diagnoses
         a non-literal format and an argument-type mismatch through each wrapper. Suite A alone
         cannot prove the annotations are effective; suite B alone cannot prove dist/ is in step,
         and it only ever exercises the host toolchain.
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { tskip, ttrue } from '@embedthis/testme'
import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const ROOT = join(import.meta.dir, '..', '..')

/*
    jsArgs() takes a scanf-style format: its conversions consume pointers to be filled, not values
    to be formatted, so neither the printf nor the scanf attribute describes it (its %s wants a
    char** where scanf wants a char*). It is the one varargs "fmt" declaration deliberately left
    unannotated.
 */
const EXEMPT = ['jsArgs']

/*
    The count of printf-family wrappers expected per header. It is asserted so that rewrapping a
    declaration across two lines - which would slip past the line-oriented scan below - fails loudly
    instead of silently reducing coverage.
 */
const EXPECTED: Record<string, number> = {
    'src/goahead.h': 16,
    'src/js.h': 1,
    'dist/goahead.h': 16,
    'dist/js.h': 1,
}

/*
    Suite A - structural
 */
function scanHeader(rel: string, expected: number) {
    let text = readFileSync(join(ROOT, rel), 'utf8')
    let found = 0

    for (let line of text.split('\n')) {
        /* Format-taking declarations: a prototype or inline definition with a char* fmt/format */
        if (!/\bc?char\s+\*(fmt|format)\s*,\s*(\.\.\.|va_list)/.test(line)) continue
        if (/^\s*#\s*define/.test(line)) continue
        if (EXEMPT.some((name) => line.includes(name + '('))) continue
        found++
        /* GCC and Clang */
        ttrue(line.includes('PRINTF_ATTRIBUTE'), rel + ': printf wrapper must carry PRINTF_ATTRIBUTE: ' +
            line.trim())
        /* MSVC. Must sit immediately before the format parameter or SAL binds it to the wrong one */
        ttrue(/PRINTF_FORMAT\s+c?char\s+\*(fmt|format)\s*,/.test(line),
            rel + ': printf wrapper must carry PRINTF_FORMAT on its format parameter: ' + line.trim())
    }
    ttrue(found == expected, rel + ': scanned ' + found + ' printf wrappers, expected ' + expected)
}

for (let rel of Object.keys(EXPECTED)) {
    scanHeader(rel, EXPECTED[rel])
}

/*
    Suite B - live compiler gate

    Two toolchains, two mechanisms. GCC/Clang read PRINTF_ATTRIBUTE and are driven by
    -Werror=format-security; MSVC reads the SAL PRINTF_FORMAT annotation and is driven by /we4774.
    Each arm mirrors the flags its own shipped build uses, so a probe that the build would reject
    is a probe this test rejects.
 */
const isWindows = String(Config.OS).match(/^win/) != null
const cc = isWindows ? Cmd.locate('cl') : (Cmd.locate('cc') || Cmd.locate('gcc') || Cmd.locate('clang'))

if (!cc) {
    tskip('No C compiler on PATH for the live format-gate probes')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-fmt-'))

    const INCLUDES = isWindows
        ? '/I' + join(ROOT, 'src') + ' /I' + join(ROOT, 'src', 'osdep')
        : '-I' + join(ROOT, 'src') + ' -I' + join(ROOT, 'src', 'osdep')

    /*
        Both arms make the non-literal format a hard error, matching the shipped build, so a
        rejected probe exits non-zero. MSVC writes diagnostics to stdout rather than stderr, so
        both streams are collected.
     */
    const FLAGS = isWindows
        ? '/nologo /c /W4 /we4774 ' + INCLUDES + ' /Fo' + join(dir, 'probe.obj')
        : '-fsyntax-only -Wformat=2 -Wformat-security -Werror=format-security ' + INCLUDES

    /* The diagnostic each arm emits for a runtime value in the format position */
    const NON_LITERAL_DIAGNOSTIC = isWindows ? /\b(C|warning )?4774\b/ : /format-security/

    /* Any format diagnostic at all, used by the control probes to assert silence */
    const ANY_FORMAT_DIAGNOSTIC = isWindows ? /\b(4774|4477|4473)\b/ : /-Wformat/

    function compile(body: string): {status: number, diagnostics: string} {
        let src = join(dir, 'probe.c')
        writeFileSync(src, '#include "goahead.h"\n#include "js.h"\n' + body + '\n')
        let cmd = new Cmd(cc + ' ' + FLAGS + ' ' + src)
        return {status: cmd.status, diagnostics: String(cmd.error) + String(cmd.response)}
    }

    /*
        B1) The CGI Content-Type shape - a runtime value in the format position - is now a hard
        compile error through every wrapper, not just websWriteHeader.
     */
    const NON_LITERAL = [
        'void p(Webs *wp, cchar *v) { websWriteHeader(wp, "Content-Type", v); }',
        'void p(Webs *wp, cchar *v) { websWrite(wp, v); }',
        'void p(Webs *wp, cchar *v) { websError(wp, 500, v); }',
        'void p(Webs *wp, cchar *v) { websSetVarFmt(wp, "k", v); }',
        'void p(cchar *v) { error(v); }',
        'void p(cchar *v) { trace(2, v); }',
        'void p(cchar *v) { logmsg(2, v); }',
        'void p(cchar *v) { wfree(sfmt(v)); }',
        'void p(char *buf, cchar *v) { fmt(buf, 64, v); }',
        'void p(WebsBuf *bp, cchar *v) { bufPut(bp, v); }',
        'void p(Js *js, cchar *v) { jsError(js, v); }',
    ]
    for (let probe of NON_LITERAL) {
        let r = compile(probe)
        ttrue(r.status != 0, 'non-literal format not rejected: ' + probe + '\n' + r.diagnostics)
        ttrue(NON_LITERAL_DIAGNOSTIC.test(r.diagnostics),
            'wrong diagnostic for: ' + probe + '\n' + r.diagnostics)
    }

    /*
        B2) Argument-type mismatches are diagnosed, which can only happen if PRINTF_ATTRIBUTE is
        actually attached - -Wformat has no other way to know these functions take a format.

        GCC/Clang only. MSVC checks argument types against a format for CRT functions (C4477) but
        not for a SAL-annotated user variadic without /analyze, so this suite would fail there for
        a reason that is not a defect. C4774 in B1 is the whole of the MSVC guarantee.
     */
    const MISMATCH = isWindows ? [] : [
        'void p(Webs *wp) { websWrite(wp, "%d", "str"); }',
        'void p(Webs *wp) { websWriteHeader(wp, "k", "%d", "str"); }',
        'void p(Webs *wp) { websError(wp, 500, "%d", "str"); }',
        'void p(void) { error("%d", "str"); }',
        'void p(void) { trace(2, "%d", "str"); }',
        'void p(void) { logmsg(2, "%d", "str"); }',
        'void p(void) { wfree(sfmt("%d", "str")); }',
        'void p(Js *js) { jsError(js, "%d", "str"); }',
    ]
    for (let probe of MISMATCH) {
        let r = compile(probe)
        ttrue(ANY_FORMAT_DIAGNOSTIC.test(r.diagnostics), 'argument type mismatch not diagnosed: ' +
            probe + '\n' + r.diagnostics)
    }
    if (isWindows) {
        tskip('B2 argument-type probes need /analyze on MSVC; C4774 in B1 is the MSVC guarantee')
    }

    /*
        B3) Control - correct usage compiles clean, so the suite above is detecting real defects
        rather than a probe harness that cannot compile anything at all.
     */
    const CLEAN = [
        'void p(Webs *wp, cchar *v) { websWriteHeader(wp, "Content-Type", "%s", v); }',
        'void p(Webs *wp, cchar *v) { websWrite(wp, "%s", v); }',
        'void p(cchar *v) { error("%s", v); }',
        'void p(void) { trace(2, "count %d", 1); }',
    ]
    for (let probe of CLEAN) {
        let r = compile(probe)
        ttrue(r.status == 0, 'correct usage rejected: ' + probe + '\n' + r.diagnostics)
        ttrue(!ANY_FORMAT_DIAGNOSTIC.test(r.diagnostics),
            'correct usage warned: ' + probe + '\n' + r.diagnostics)
    }

    rmSync(dir, {recursive: true, force: true})
}
