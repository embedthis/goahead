/*
    user-information-consistency.tst - The CRA user information agrees with itself and with the deployment guide

    It must point at the secure-configuration guide it names elsewhere, tell integrators behind a TLS-terminating
    proxy to declare it, list every file GoAhead persists, and say that a Digest hash is a password equivalent.
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const version = JSON.parse(readFileSync(join(ROOT, 'package.json'), 'utf8')).version
const INFO = join(ROOT, 'doc', 'releases', version, 'user-information.md')

function section(text: string, n: number): string {
    let start = text.indexOf(`\n## ${n}.`)
    let end = text.indexOf(`\n## ${n + 1}.`)
    ttrue(start >= 0 && end > start, `the user information has section ${n}`)
    return text.slice(start, end)
}

if (!existsSync(INFO)) {
    tskip('the user information is not in this tree')
} else {
    let info = readFileSync(INFO, 'utf8')
    ttrue(!info.includes('No consolidated secure-configuration guide'),
        'the user information must not deny the existence of the guide its own section 10 names')
    let commissioning = section(info, 7)
    tcontains(commissioning, 'SECURITY-DEPLOYMENT.md', 'secure commissioning must point at the deployment guide')
    ttrue(/--proxy|ME_GOAHEAD_TRUSTED_PROXIES|websSetTrustedProxies/.test(commissioning),
        'integration behind a proxy must say to declare a TLS-terminating proxy as trusted')
    let misuse = section(info, 5)
    ttrue(/X-Forwarded-Proto/.test(misuse) || /X-Forwarded-Proto/.test(commissioning),
        'the proxy guidance must name the forwarded scheme header the proxy has to set')
    let decommissioning = section(info, 9)
    tcontains(decommissioning, 'ME_GOAHEAD_PUT_DIR', 'the persisted data list must include the PUT staging directory')
    tcontains(decommissioning, 'ME_GOAHEAD_TEMP_DIR', 'the persisted data list must include the temporary directory')
    ttrue(/access\.log|access log/.test(decommissioning), 'the persisted data list must include the access log')
    let properties = section(info, 4)
    ttrue(/password equivalent/.test(properties),
        'the credential storage property must say an MD5 Digest hash is a password equivalent')
}
