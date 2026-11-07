/*
    auto-var-init-probe.tst.ts - The Makefile probes the compiler for automatic variable zero-initialisation

    An automatic variable read before it is assigned holds whatever the stack held. The Makefile must probe for
    -ftrivial-auto-var-init=zero as it does for the other hardening flags and pass it to every configuration, the
    deployment guide must list it, and a probe compiled with the flags the Makefile selects must read such a
    variable as zero.
 */
import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { execFileSync } from 'node:child_process'

const ROOT = join(import.meta.dir, '..', '..')
const MAKEFILE = join(ROOT, 'Makefile')
const FLAG = '-ftrivial-auto-var-init=zero'

if (!existsSync(MAKEFILE)) {
    tskip('The Makefile is not part of this tree')
} else {
    let makefile = readFileSync(MAKEFILE, 'utf8')
    ttrue(/^CFI_FLAGS\s*\+=.*-ftrivial-auto-var-init=zero/m.test(makefile),
          'the Makefile must select automatic variable zero-initialisation by probing the compiler')
    let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
    ttrue(guide.includes(FLAG), `SECURITY-DEPLOYMENT.md must list ${FLAG} for integrators building with their own toolchain`)

    /*
        The flags the Makefile selects on this host. A compiler that rejects the flag leaves it out, and the
        behavioural check does not apply.
     */
    let selected = ''
    try {
        selected = execFileSync('make', ['-s', '--no-print-directory', '-C', ROOT, '-f', '-', 'print-cfi-flags'], {
            input: 'include Makefile\nprint-cfi-flags:\n\t@echo $(CFI_FLAGS)\n', encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe'],
        }).trim()
    } catch {
        tinfo('make is not available; the selected flags were not read')
    }
    if (selected && !selected.includes(FLAG)) {
        tinfo(`the compiler does not accept ${FLAG}; selected [${selected}]`)
    } else if (selected) {
        /*
            dirty() leaves a non-zero pattern where probe()'s frame will be, and probe() reads a local it never
            assigned. volatile keeps both the fill and the read in the generated code.
         */
        let dir = mkdtempSync(join(tmpdir(), 'goahead-autoinit-'))
        try {
            writeFileSync(join(dir, 'probe.c'),
                '#include <stdio.h>\n' +
                '__attribute__((noinline)) static void dirty(void)\n{\n    volatile unsigned char fill[4096];\n' +
                '    for (unsigned i = 0; i < sizeof(fill); i++) fill[i] = 0xA5;\n}\n' +
                '__attribute__((noinline)) static unsigned probe(void)\n{\n    volatile unsigned char local[64];\n' +
                '    return local[7];\n}\n' +
                'int main(void)\n{\n    dirty();\n    printf("%u\\n", probe());\n    return 0;\n}\n')
            let compiler = process.env.CC || 'cc'
            let binary = join(dir, 'probe')
            execFileSync(compiler, ['-O2', ...selected.split(/\s+/), '-o', binary, join(dir, 'probe.c')], { stdio: 'pipe' })
            let value = execFileSync(binary, { encoding: 'utf8' }).trim()
            ttrue(value == '0', `with the selected flags [${selected}] an unassigned local must read as zero, read ${value}`)
        } finally {
            rmSync(dir, { recursive: true, force: true })
        }
    }
}
