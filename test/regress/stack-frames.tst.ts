/*
    No library function keeps a multi-KiB frame on the stack. The request path runs in whatever task or thread the
    embedder gives websServiceEvents, which on a small RTOS or no-MMU target is a few KiB with no guard page, so the
    documented minimum stack only holds while frames stay small. Compile each library source and fail naming any
    function over the limit.
 */
import { Config } from '@embedthis/ejscript'
import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const LIMIT = 2048

//  Programs, not library
const PROGRAMS = ['goahead.c', 'test.c', 'cgitest.c']

//  clang says "stack frame size (N) exceeds limit (M) in 'fn'", gcc says "the frame size of N bytes is larger than M bytes"
const CLANG = /(\S+?):\d+:\d+: (?:warning|error): stack frame size \((\d+)\) exceeds limit \(\d+\) in '([^']+)'/
const GCC = /(\S+?):\d+:\d+: (?:warning|error): the frame size of (\d+) bytes is larger than/

function findCompiler(): string | null {
    for (let cc of ['cc', 'gcc', 'clang']) {
        let r = spawnSync(cc, ['--version'], {encoding: 'utf8'})
        if (r.status === 0) {
            return cc
        }
    }
    return null
}

function compile(cc: string, args: string[], source: string): {status: number, output: string} {
    let r = spawnSync(cc, [...args, '-c', source, '-o', process.platform == 'win32' ? 'NUL' : '/dev/null'],
        {encoding: 'utf8', cwd: ROOT})
    return {status: r.status ?? -1, output: (r.stdout || '') + (r.stderr || '')}
}

function oversize(output: string): string[] {
    let found: string[] = []
    for (let line of output.split('\n')) {
        let m = line.match(CLANG)
        if (m) {
            found.push(`${m[3]} (${m[2]} bytes)`)
        } else if (GCC.test(line)) {
            found.push(line.trim())
        }
    }
    return found
}

const isWindows = String(Config.OS).match(/^win/) != null
const cc = isWindows ? null : findCompiler()
if (!cc) {
    tskip('No GCC or Clang on PATH for the frame-size check')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-frames-'))
    let probe = join(dir, 'probe.c')
    writeFileSync(probe, 'int sink(char *);\nint big(void) { char buf[4096]; return sink(buf); }\n')

    const OPTS = ['-O2', `-Wframe-larger-than=${LIMIT}`, '-Isrc', '-Isrc/osdep', '-DME_VERSION="0.0.0"']
    for (let inc of ['/opt/homebrew/include', '/usr/local/include']) {
        if (existsSync(join(inc, 'openssl')) || existsSync(join(inc, 'mbedtls'))) {
            OPTS.push('-I' + inc)
        }
    }

    //  Control: the compiler reports an oversized frame, so a clean result below means something
    let control = compile(cc, OPTS, probe)
    if (oversize(control.output).length == 0) {
        tskip(`${cc} does not support -Wframe-larger-than`)
    } else {
        let sources = readdirSync(join(ROOT, 'src')).filter(f => f.endsWith('.c') && !PROGRAMS.includes(f)).sort()
        ttrue(sources.length >= 18, `expected the library sources, found ${sources.length}`)


        let passes: {name: string, args: string[]}[] = [{name: 'native', args: []}]
        let m32 = process.platform == 'darwin' ? ['-target', 'i386-apple-macos10.13'] : ['-m32']
        let probe32 = join(dir, 'probe32.c')
        writeFileSync(probe32, '#include <stdio.h>\nint main(void) { return 0; }\n')
        if (compile(cc, [...m32, '-O0'], probe32).status == 0) {
            passes.push({name: 'ILP32', args: m32})
        } else {
            tinfo('Cannot build for a 32-bit target here: checking the native data model only')
        }

        let failures: string[] = []
        for (let pass of passes) {
            for (let source of sources) {
                let src = join('src', source)
                let r = compile(cc, [...pass.args, ...OPTS], src)
                if (r.status != 0 && /file not found|No such file/.test(r.output)) {
                    tinfo(`${src}: skipped, its headers are not installed`)
                    continue
                }
                if (r.status != 0) {
                    failures.push(`${src} (${pass.name}) failed to compile\n${r.output}`)
                }
                let big = oversize(r.output)
                if (big.length) {
                    failures.push(`${src} (${pass.name}) frames over ${LIMIT} bytes: ${big.join(', ')}`)
                }
            }
        }
        ttrue(failures.length == 0, `\n${failures.join('\n')}`)
    }
    rmSync(dir, {recursive: true, force: true})
}
