/*
    feature-matrix-ci.tst - CI builds the non-default feature configurations
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const WORKFLOW = join(ROOT, '.github', 'workflows', 'ci.yml')

/*
    Each leg: the defines it must build with and whether it runs the suite.
 */
const LEGS: Record<string, { flags: string[], run?: boolean, optimize?: string }> = {
    'logging-off': { flags: ['-DME_GOAHEAD_LOGGING=0'], run: true },
    'tracing-off': { flags: ['-DME_GOAHEAD_TRACING=0'] },
    'legacy': { flags: ['-DME_GOAHEAD_LEGACY=1'] },
    'rom': { flags: ['-DME_ROM=1'] },
    'auth-off': { flags: ['-DME_GOAHEAD_AUTH=0'], run: true },
    'digest-off': { flags: ['-DME_GOAHEAD_DIGEST=0'] },
    'access-log': { flags: ['-DME_GOAHEAD_ACCESS_LOG=1'] },
    'no-tls': { flags: ['-DME_COM_SSL=0', '-DME_COM_OPENSSL=0', '-DME_COM_MBEDTLS=0'] },
    'replace-malloc': { flags: ['-DME_GOAHEAD_REPLACE_MALLOC=1'], run: true },
    'assert-abort': { flags: ['-DME_GOAHEAD_ASSERT_ABORT=1'] },
    'release-legacy': { flags: ['-DME_GOAHEAD_LEGACY=1'], optimize: 'release' },
    'cgi-off': { flags: ['-DME_GOAHEAD_CGI=0'], run: true },
    'javascript-off': { flags: ['-DME_GOAHEAD_JAVASCRIPT=0'], run: true },
    'upload-off': { flags: ['-DME_GOAHEAD_UPLOAD=0'], run: true },
}

if (!existsSync(WORKFLOW)) {
    tskip('The CI workflows are not part of a source distribution')
} else {
    let ci = Bun.YAML.parse(readFileSync(WORKFLOW, 'utf8')) as any
    let job = ci.jobs?.features
    ttrue(job != null, 'ci.yml must define the features job')
    ttrue(job?.['runs-on'] == 'ubuntu-latest', 'the features job must run on ubuntu-latest')
    ttrue(job?.strategy?.['fail-fast'] === false, 'one failing leg must not cancel the others')

    let legs: any[] = job?.strategy?.matrix?.include || []
    for (let [name, want] of Object.entries(LEGS)) {
        let leg = legs.find(l => l.leg == name)
        ttrue(leg != null, `the features job must have the ${name} leg`)
        if (!leg) continue
        let flags = `${leg.cflags || ''} ${leg.defines || ''}`.split(/\s+/)
        for (let flag of want.flags) {
            ttrue(flags.includes(flag), `${name} must build with ${flag}`)
        }
        ttrue(!!leg.run == !!want.run, `${name} must ${want.run ? '' : 'not '}run the suite`)
        ttrue((leg.optimize || 'debug') == (want.optimize || 'debug'), `${name} must build ${want.optimize || 'debug'}`)
    }

    let steps: any[] = job?.steps || []
    let build = steps.find(s => s.name == 'Build')?.run || ''
    ttrue(build.includes('make clean'), 'each leg must build from clean')
    ttrue(build.includes("grep -qE 'warning:"), 'a warning must fail the leg')
    let test = steps.find(s => s.name == 'Run test suite')
    ttrue(test?.if == 'matrix.run' && /make .*test/.test(test?.run || ''), 'the run legs must run make test')
    ttrue(!test?.env?.TM_STRICT_SKIPS && !job?.env?.TM_STRICT_SKIPS,
          'the run legs must let the tests of a compiled out feature skip')
}
