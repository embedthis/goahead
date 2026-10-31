/*
    static-hygiene.tst.ts - Source diagnostics the shipped warning set does not surface

    The build compiles with -Wall -Wextra -Wshadow -Wformat=2, which say nothing about a conditional on a macro that
    is never defined (a stale BLD_ name, or a block parked behind UNUSED/DISABLE), a switch label reached by falling
    through without an annotation, or a signed argument to an unsigned conversion. Each of those hides a defect
    class: a diagnostic that compiles out, dead code that drifts, and a case whose fall-through is intent or
    accident. This test compiles every library source with -Wundef -Wimplicit-fallthrough -Wformat-signedness and
    requires zero such diagnostics in src/*.c, and it requires the ME_GOAHEAD_LIMIT_BUFFER floor that http.c asserts
    at runtime (which a release build compiles out) to be a compile-time error instead.
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { spawnSync } from 'node:child_process'
import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const ROOT = join(import.meta.dir, '..', '..')

const isWindows = String(Config.OS).match(/^win/) != null
const cc = isWindows ? null : (Cmd.locate('cc') || Cmd.locate('gcc') || Cmd.locate('clang'))

if (!cc) {
    tskip('The warning probes need GCC or Clang')
} else {
    /*
        Every library translation unit plus the shipped programs. The TLS providers and PAM are disabled so the probe
        does not depend on host headers; the conditionals under test are in the provider-independent sources.
     */
    const SOURCES = ['action.c', 'alloc.c', 'auth.c', 'cgi.c', 'crypt.c', 'file.c', 'fs.c', 'http.c', 'js.c', 'jst.c',
        'mbedtls.c', 'openssl.c', 'options.c', 'osdep.c', 'rom.c', 'route.c', 'runtime.c', 'socket.c', 'time.c',
        'upload.c', 'goahead.c', 'cgitest.c', 'test.c']

    const DEFINES = ['-DME_COM_SSL=0', '-DME_COM_OPENSSL=0', '-DME_COM_MBEDTLS=0', '-DME_COMPILER_HAS_PAM=0',
        '-DME_DEBUG=1', '-DME_VERSION="test"']
    const INCLUDES = ['-I' + join(ROOT, 'src'), '-I' + join(ROOT, 'src', 'osdep')]

    function compile(flags: string[], src: string): string {
        let result = spawnSync(String(cc), ['-fsyntax-only', '-std=gnu11', ...DEFINES, ...INCLUDES, ...flags, src],
            { encoding: 'utf8' })
        return (result.stdout || '') + (result.stderr || '')
    }

    /*
        A probe flag is used only when this compiler knows it: GCC lacks -Wunknown-warning-option semantics and an
        unknown -W flag would itself be a diagnostic.
     */
    let dir = mkdtempSync(join(tmpdir(), 'goahead-hygiene-'))
    let empty = join(dir, 'empty.c')
    writeFileSync(empty, 'int goaheadHygieneProbe;\n')
    let flags: string[] = []
    for (let flag of ['-Wundef', '-Wimplicit-fallthrough', '-Wformat', '-Wformat-signedness']) {
        if (!/unknown|unrecognized|not supported/i.test(compile(['-Werror', flag], empty))) {
            flags.push(flag)
        }
    }
    ttrue(flags.includes('-Wundef') && flags.includes('-Wimplicit-fallthrough'), 'the compiler supports the probes')

    /*
        Suite A: library sources carry no conditional on a macro that nothing defines, no unannotated fall-through
        and no signed argument to an unsigned conversion. Only diagnostics located in src/*.c count: osdep.h is
        vendored and is checked upstream. A platform or feature macro (WINDOWS, VXWORKS, ME_COM_*) is undefined on
        every other platform by design, so an undefined-macro diagnostic is kept only when no header, source or
        project file in the tree defines the name at all: that is a stale conditional, and whatever it guards can
        never compile.
     */
    tinfo('--- Suite A: library sources compile clean under the probe warnings ---')
    let defining = [join(ROOT, 'src', 'osdep', 'osdep.h'), join(ROOT, 'src', 'goahead.h'), join(ROOT, 'src', 'js.h'),
        join(ROOT, 'Makefile'), ...SOURCES.map(name => join(ROOT, 'src', name))]
    for (let sub of ['gmake2', 'vs2022', 'xcode']) {
        let base = join(ROOT, 'projects', sub)
        for (let name of readdirSync(base)) {
            let path = join(base, name)
            defining.push(statSync(path).isDirectory() ? join(path, 'project.pbxproj') : path)
        }
    }
    let definitions = defining.filter(path => existsSync(path)).map(path => readFileSync(path, 'utf8')).join('\n')
    function defined(name: string): boolean {
        return new RegExp('(#\\s*define\\s+' + name + '\\b|[-/]D' + name + '\\b|\\b' + name + '\\s*=)').test(definitions)
    }
    let kept: string[] = []
    for (let name of SOURCES) {
        let output = compile(flags, join(ROOT, 'src', name))
        for (let line of output.split('\n')) {
            let match = line.match(/src\/([\w]+\.c):(\d+):\d+: warning: (.*)\[-W([\w-]+)\]/)
            if (!match || !['undef', 'implicit-fallthrough', 'format', 'format-signedness'].includes(match[4])) {
                continue
            }
            //  clang quotes the macro name with ' and gcc with "
            let macro = match[4] == 'undef' ? (match[3].match(/['"](\w+)['"]/) || [])[1] : null
            //  A name beginning with an underscore is the compiler's (_WIN32), defined by it and not by the tree
            if (macro && (macro.startsWith('_') || defined(macro))) {
                continue
            }
            kept.push(match[1] + ':' + match[2] + ': ' + match[3].trim() + ' [-W' + match[4] + ']')
        }
    }
    ttrue(kept.length == 0, 'the library sources must compile clean under ' + flags.join(' ') + ':\n' + kept.join('\n'))

    /*
        Suite B: a buffer limit below the documented floor is refused by the preprocessor, not by an assert that a
        release build compiles out.
     */
    tinfo('--- Suite B: ME_GOAHEAD_LIMIT_BUFFER below 1024 is a compile-time error ---')
    let small = compile(['-DME_GOAHEAD_LIMIT_BUFFER=512'], join(ROOT, 'src', 'http.c'))
    ttrue(/error:.*ME_GOAHEAD_LIMIT_BUFFER/.test(small), 'ME_GOAHEAD_LIMIT_BUFFER=512 must fail to compile:\n' + small)
    let floor = compile(['-DME_GOAHEAD_LIMIT_BUFFER=1024', '-Werror'], join(ROOT, 'src', 'http.c'))
    ttrue(floor.trim() == '', 'ME_GOAHEAD_LIMIT_BUFFER=1024 must compile clean:\n' + floor)

    /*
        Read conditional directives to catch undefined macros hidden by host-specific preprocessing or
        short-circuit evaluation. Exempt presence tests, provider macros and documented optional macros.
     */
    tinfo('--- Suite C: every #if operand in src/ is a macro the tree defines ---')
    const OPTIONAL = ['DOXYGEN', 'ME_CUSTOMIZE', 'ME_COM_MATRIXSSL', 'ME_COM_NANOSSL', 'HAVE_NTOA_R',
        'OPENSSL_VERSION_NUMBER', 'MBEDTLS_VERSION_NUMBER', 'SSL_OP_SINGLE_ECDH_USE']
    let stale: string[] = []
    let scanned = [...SOURCES.map(name => join(ROOT, 'src', name)), join(ROOT, 'src', 'goahead.h'),
        join(ROOT, 'src', 'js.h'), join(ROOT, 'src', 'utils', 'gopass.c'), join(ROOT, 'src', 'utils', 'webcomp.c')]
    for (let path of scanned) {
        let lines = readFileSync(path, 'utf8').split('\n')
        for (let i = 0; i < lines.length; i++) {
            let match = lines[i].match(/^\s*#\s*(?:if|elif)\b(.*)$/)
            if (!match) {
                continue
            }
            let expr = match[1].replace(/\/\*.*?\*\/|\/\/.*/g, '')
                .replace(/\b(?:defined|__has_\w+)\s*(?:\(\s*\w+\s*\)|\w+)/g, '')
            for (let name of new Set(expr.match(/\b[A-Za-z_]\w*\b/g) || [])) {
                if (name.startsWith('_') || /^\d/.test(name) || OPTIONAL.includes(name) || defined(name)) {
                    continue
                }
                stale.push(path.slice(ROOT.length + 1) + ':' + (i + 1) + ': ' + name)
            }
        }
    }
    ttrue(stale.length == 0, 'no #if in src/ may name a macro nothing defines:\n' + stale.join('\n'))

    rmSync(dir, { recursive: true, force: true })
}
