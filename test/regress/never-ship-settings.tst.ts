/*
    never-ship-settings.tst - The deployment guide and the threat model name the build settings that must never be
    on in a shipped build

    ME_GOAHEAD_AUTO_LOGIN serves every authenticated route without credentials and ME_GOAHEAD_LEGACY marks
    connections from the device's own address as local; neither was in the guide's never-ship list.
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')

let auth = readFileSync(join(ROOT, 'src', 'auth.c'), 'utf8')
ttrue(/autoLogin[^]*Security warning: ME_GOAHEAD_AUTO_LOGIN is enabled/.test(auth),
    'the server warns at startup when auto-login is built in, so the guide can say so')

let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
let features = guide.split('\n').find(l => l.includes('Turn off what you do not use')) || ''
ttrue(features != '', 'the guide has the "Turn off what you do not use" item')
tcontains(features, 'ME_GOAHEAD_AUTO_LOGIN', 'the guide must say ME_GOAHEAD_AUTO_LOGIN must never ship')
tcontains(features, 'ME_GOAHEAD_LEGACY', 'the guide must say ME_GOAHEAD_LEGACY must stay off')

const MODEL = join(ROOT, 'doc', 'security', 'threat-model.md')
if (existsSync(MODEL)) {
    let row = readFileSync(MODEL, 'utf8').split('\n').find(l => l.includes('says not to ship')) || ''
    ttrue(row != '', 'the threat model lists the configurations the guide says not to ship')
    tcontains(row, 'ME_GOAHEAD_AUTO_LOGIN', 'the threat model must list ME_GOAHEAD_AUTO_LOGIN among them')
}
