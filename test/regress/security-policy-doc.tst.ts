/*
    security-policy-doc.tst - The tree carries a security policy that names the vulnerability reporting channel

    SECURITY.md and CONTRIBUTING.md ship in the public repository and the source distribution, so they must not
    point at internal documents that neither carries.
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const POLICY = join(ROOT, 'SECURITY.md')
const INTERNAL = /doc\/(architecture|security|issues|features|compliance)\//

ttrue(existsSync(POLICY), 'SECURITY.md must be at the top of the tree')
if (existsSync(POLICY)) {
    let policy = readFileSync(POLICY, 'utf8')
    tcontains(policy, 'security@embedthis.com', 'the policy must name the private reporting address')
    tcontains(policy, 'GOAHEAD-SA-', 'the policy must name the advisory identifier')
    tcontains(policy, 'SECURITY-DEPLOYMENT.md', 'the policy must point operators at the deployment guide')
    ttrue(!INTERNAL.test(policy), 'the policy must not cite internal documents the distribution does not carry')
}
tcontains(readFileSync(join(ROOT, 'README.md'), 'utf8'), 'SECURITY.md', 'the README must link the security policy')
let contributing = readFileSync(join(ROOT, 'CONTRIBUTING.md'), 'utf8')
tcontains(contributing, 'SECURITY.md', 'the contributor guide must say vulnerabilities are reported privately')
ttrue(!INTERNAL.test(contributing), 'the contributor guide must not cite internal documents')
const BUILDLIB = join(ROOT, 'bin', 'buildLib.sh')
if (existsSync(BUILDLIB)) {
    tcontains(readFileSync(BUILDLIB, 'utf8'), 'SECURITY.md', 'the distribution build must stage the security policy')
}
