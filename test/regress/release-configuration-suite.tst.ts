/*
    release-configuration-suite.tst.ts - CI runs the test suite against the release configuration
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const CI = join(import.meta.dir, '..', '..', '.github', 'workflows', 'ci.yml')

if (!existsSync(CI)) {
    tskip('The CI workflow is not part of a source distribution')
} else {
    let ci = Bun.YAML.parse(readFileSync(CI, 'utf8')) as any
    let legs = ci.jobs.features.strategy.matrix.include as any[]
    let release = legs.filter(leg => leg.optimize == 'release' && leg.run && !leg.cflags && !leg.defines)
    ttrue(release.length == 1, 'a features leg must run the suite against the unmodified release configuration')
}
