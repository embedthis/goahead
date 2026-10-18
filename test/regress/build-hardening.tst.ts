/*
    build-hardening.tst.ts - Build hardening and diagnostic flags; Linux non-executable stacks
 */

import { tinfo, ttrue } from '@embedthis/testme'
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { fileURLToPath } from 'node:url'
import { execFileSync } from 'node:child_process'

const ROOT = fileURLToPath(new URL('../..', import.meta.url))

/*
    Check flags within each generated build configuration.
 */
function sections(file: string): Record<string, string[]> {
    let result: Record<string, string[]> = {}
    let current = ''
    for (let line of readFileSync(file, 'utf8').split('\n')) {
        let match = line.match(/^(?:else )?ifeq \(\$\(config\),(\w+)\)/)
        if (match) {
            current = match[1]
            result[current] = []
        } else if (current) {
            result[current].push(line)
        }
    }
    return result
}

const PROGRAMS = ['goahead.make', 'goahead-test.make', 'gopass.make', 'webcomp.make', 'cgitest.make']
const FATAL = ['-Werror=format', '-Werror=implicit-function-declaration', '-Werror=return-type', '-Werror=int-conversion',
               '-Werror=null-dereference']
const FATAL_MSVC = ['/we4013', '/we4716']

for (let name of readdirSync(ROOT + 'projects/gmake2').filter(f => f.endsWith('.make'))) {
    let configs = sections(ROOT + 'projects/gmake2/' + name)
    ttrue(Object.keys(configs).length == 9, `${name}: expected debug, release and asan on three platforms`)
    for (let [config, body] of Object.entries(configs)) {
        let cflags = body.find(line => line.startsWith('ALL_CFLAGS +=')) || ''
        let ldflags = body.find(line => line.startsWith('ALL_LDFLAGS +=')) || ''
        let defines = body.find(line => line.startsWith('DEFINES +=')) || ''
        let where = `${name} ${config}`
        ttrue(cflags.includes('-fstack-protector-strong'), `${where}: must use -fstack-protector-strong`)
        ttrue(!cflags.includes('ssp-buffer-size'), `${where}: --param=ssp-buffer-size is not needed with -strong`)
        ttrue(!cflags.includes('-Wno-unknown-warning-option'), `${where}: unknown warning options must not be silenced`)
        ttrue(cflags.includes('-Wshadow'), `${where}: must warn on shadowed declarations`)
        let off = [...new Set(cflags.match(/-Wno-[\w-]+/g) || [])]
        ttrue(off.length == 1 && off[0] == '-Wno-unused-parameter',
              `${where}: -Wno-unused-parameter must be the only warning off, got [${off}]`)
        for (let flag of FATAL) {
            ttrue(cflags.split(' ').includes(flag), `${where}: ${flag} must fail the build`)
        }
        ttrue(cflags.includes('$(CFI_FLAGS)'), `${where}: must compile with the control-flow protection CFI_FLAGS selects`)
        if (config.endsWith('_macosx')) {
            ttrue(cflags.includes('-Wimplicit-int-conversion'), `${where}: must warn on implicit int truncation`)
        }
        if (config.endsWith('_linux') || config.endsWith('_freebsd')) {
            ttrue(ldflags.includes('-Wl,-z,relro,-z,now'), `${where}: the link must use full RELRO`)
            ttrue(ldflags.includes('-Wl,-z,noexecstack'), `${where}: the link must mark the stack non-executable`)
            ttrue(!ldflags.includes('--no-warn-execstack'), `${where}: the executable-stack warning must not be silenced`)
            if (PROGRAMS.includes(name)) {
                ttrue(cflags.includes('-fPIE') && / -pie\b/.test(ldflags), `${where}: a program must be built as PIE`)
            }
        }
        if (config.startsWith('release_')) {
            ttrue(defines.includes('-D_FORTIFY_SOURCE=3'), `${where}: a release build must be fortified`)
        } else {
            ttrue(!defines.includes('_FORTIFY_SOURCE'), `${where}: fortification needs optimization, so not in debug`)
        }
        let sanitized = cflags.includes('-fsanitize=') || ldflags.includes('-fsanitize=')
        if (config.startsWith('asan_')) {
            ttrue(cflags.includes('-fsanitize=address,undefined') && ldflags.includes('-fsanitize=address,undefined'),
                  `${where}: must compile and link with AddressSanitizer and UndefinedBehaviorSanitizer`)
            ttrue(cflags.split(' ').includes('$(SANITIZE_FLAGS)') && ldflags.split(' ').includes('$(SANITIZE_FLAGS)'),
                  `${where}: must compile and link with the compiler's extra sanitizer groups`)
            ttrue(ldflags.split(' ').includes('$(SANITIZE_LDFLAGS)'), `${where}: must link with the sanitizer runtime search path`)
            ttrue(cflags.includes('-fno-sanitize-recover=all'), `${where}: every sanitizer report must abort`)
        } else {
            ttrue(!sanitized, `${where}: only the asan configuration is sanitized`)
        }
        let strips = body.some(line => /^\tstrip -x "/.test(line))
        if (config == 'release_macosx') {
            ttrue(strips, `${where}: a macOS release build must be stripped after linking`)
        } else {
            ttrue(!strips, `${where}: only a macOS release build is stripped after linking`)
        }
    }
}

//  Xcode projects.
for (let name of readdirSync(ROOT + 'projects/xcode').filter(f => f.endsWith('.xcodeproj'))) {
    let text = readFileSync(ROOT + 'projects/xcode/' + name + '/project.pbxproj', 'utf8')
    ttrue(text.includes('"-fstack-protector-strong"'), `${name}: must use -fstack-protector-strong`)
    ttrue(!text.includes('-Wno-unknown-warning-option'), `${name}: unknown warning options must not be silenced`)
    ttrue(text.includes('"-Wshadow"'), `${name}: must warn on shadowed declarations`)
    ttrue(text.includes('"-Wimplicit-int-conversion"'), `${name}: must warn on implicit int truncation`)
    let off = [...new Set(text.match(/-Wno-[\w-]+/g) || [])]
    ttrue(off.length == 1 && off[0] == '-Wno-unused-parameter',
          `${name}: -Wno-unused-parameter must be the only warning off, got [${off}]`)
    for (let flag of FATAL) {
        ttrue(text.includes(`"${flag}"`), `${name}: ${flag} must fail the build`)
    }
    ttrue(/CONFIGURATION\}\\" = \\"release\\" \]; then\\nstrip -x /.test(text),
          `${name}: a release build must be stripped after linking`)
    ttrue((text.match(/"_FORTIFY_SOURCE=3"/g) || []).length == 1, `${name}: the release build, and only it, must be fortified`)
}

for (let name of readdirSync(ROOT + 'projects/vs2022').filter(f => f.endsWith('.vcxproj'))) {
    let text = readFileSync(ROOT + 'projects/vs2022/' + name, 'utf8')
    let lists = text.match(/<DisableSpecificWarnings>[^<]*</g) || []
    ttrue(lists.length > 0, `${name}: expected a disabled-warning list`)
    for (let list of lists) {
        ttrue(!/\b4456\b|\b4459\b/.test(list), `${name}: shadowing warnings 4456 and 4459 must not be disabled`)
    }
    for (let flag of FATAL_MSVC) {
        ttrue(text.includes(flag), `${name}: ${flag} must fail the build`)
    }
    let compile = text.match(/<AdditionalOptions>[^<]*<\/AdditionalOptions>/g) || []
    ttrue(compile.some(options => options.includes('/guard:cf')), `${name}: must compile with Control Flow Guard`)
    for (let flag of ['/guard:cf', '/DYNAMICBASE', '/HIGHENTROPYVA', '/NXCOMPAT', '/CETCOMPAT']) {
        ttrue((text.match(new RegExp(flag.replace('/', '\\/'), 'g')) || []).length >= 2, `${name}: must link with ${flag}`)
    }
}

let makefile = readFileSync(ROOT + 'Makefile', 'utf8')
ttrue(/^CFI_FLAGS\s*:=.*-fcf-protection=full.*-mbranch-protection=standard/ms.test(makefile),
      'the Makefile must select CFI_FLAGS by probing the compiler')
ttrue(/^SANITIZE_FLAGS\s*:=.*-fsanitize=integer.*-fsanitize=implicit-conversion.*-fsanitize=bounds-strict/ms.test(makefile),
      'the Makefile must select SANITIZE_FLAGS by probing the compiler')
ttrue(/^test-asan:.*sanitizer-canary\.sh.*\btm\b/ms.test(makefile), 'test-asan must run the sanitizer canary before the suite')

if (process.platform == 'linux') {
    for (let binary of ['build/bin/libgoahead.so', 'build/bin/goahead', 'build/bin/goahead-test']) {
        if (!existsSync(ROOT + binary)) {
            continue
        }
        let headers = ''
        try {
            headers = execFileSync('readelf', ['-lW', ROOT + binary], { encoding: 'utf8' })
        } catch {
            tinfo('readelf is not available; binary stack flags not checked')
            break
        }
        let stack = headers.split('\n').find(line => line.includes('GNU_STACK')) || ''
        ttrue(stack != '' && !/\sRWE\s/.test(stack), `${binary}: the stack must not be executable [${stack.trim()}]`)
    }
}

/*
    Check CET landing instructions or ARM return-address signing in built code.
 */
const LIBRARY = process.platform == 'darwin' ? 'build/bin/libgoahead.dylib' : 'build/bin/libgoahead.so'
const MARKER = process.arch == 'x64' ? /\bendbr64\b/ : process.arch == 'arm64' ? /\bpacib?sp\b|\bpaciasp\b/ : null
if (MARKER && existsSync(ROOT + LIBRARY) && (process.platform == 'linux' || process.platform == 'darwin')) {
    let code = ''
    try {
        let tool = process.platform == 'darwin' ? ['otool', ['-tv', ROOT + LIBRARY]] : ['objdump', ['-d', ROOT + LIBRARY]]
        code = execFileSync(tool[0] as string, tool[1] as string[], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 })
    } catch {
        tinfo('no disassembler available; control-flow protection in the binary not checked')
    }
    if (code) {
        ttrue(MARKER.test(code), `${LIBRARY}: must be built with control-flow protection`)
    }
}


if (process.platform != 'win32') {
    let dir = mkdtempSync(tmpdir() + '/goahead-fortify-')
    try {
        writeFileSync(dir + '/probe.c',
                      '#include "goahead.h"\nvoid *probe(const char *s, size_t n)\n' +
                      '{\n    char *p = walloc(8);\n    memcpy(p, s, n);\n    return p;\n}\n')
        let compiler = process.env.CC || 'cc'
        let object = dir + '/probe.o'
        execFileSync(compiler, ['-O2', '-D_FORTIFY_SOURCE=3', '-I' + ROOT + 'src', '-I' + ROOT + 'src/osdep', '-c',
                                dir + '/probe.c', '-o', object], { stdio: 'pipe' })
        let symbols = execFileSync('nm', [object], { encoding: 'utf8' })
        ttrue(/\b_{2,3}memcpy_chk\b/.test(symbols), `${compiler}: a copy into a walloc block must be fortified`)
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
