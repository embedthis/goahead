/*
    sbom-identity.tst.ts - A modified SBOM carries a new CycloneDX version and timestamp, and every document that
    describes the SBOM describes the one in the tree

    CycloneDX gives a BOM its identity through serialNumber and version: a modified BOM keeps its serial number and
    increments its version, so two readers holding different content can tell them apart. The conformity dossier,
    the vulnerability disposition, the Annex I matrix and the vulnerability-management procedure each restate the
    SBOM's component count, timestamp or in-scope components; they must agree with the SBOM they cite.
 */

import { tinfo, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')
const SBOM_PATH = 'doc/compliance/sbom/sbom.json'
const MASTER = join(TOP, SBOM_PATH)

type Identity = { serial: string, version: number, timestamp: string, body: string }

function git(...args: string[]): string | null {
    let result = spawnSync('git', ['-C', TOP, ...args], { encoding: 'utf8' })
    return result.status == 0 ? result.stdout : null
}

/*
    The BOM's identity fields and its content without them
 */
function identity(text: string): Identity {
    let bom = JSON.parse(text)
    let metadata = { ...bom.metadata }
    delete metadata.timestamp
    let body = { ...bom, metadata }
    delete body.version
    return { serial: bom.serialNumber, version: bom.version, timestamp: bom.metadata?.timestamp, body: JSON.stringify(body) }
}

function checkModificationIdentity(current: Identity) {
    let log = git('log', '-8', '--format=%H', '--', SBOM_PATH)
    if (log == null) {
        tinfo('No git history, so the SBOM modification identity is not checked')
        return
    }
    let commits = log.trim().split('\n').filter(c => c)
    /*
        The newest earlier state whose content differs from the current content is the one the current identity
        must supersede, whether that content change is committed or still in the working copy
     */
    let prev: Identity | null = null
    let what = ''
    for (let commit of commits) {
        let state = identity(git('show', `${commit}:${SBOM_PATH}`) || '{}')
        if (state.body != current.body) {
            prev = state
            what = `the current SBOM against ${commit.slice(0, 8)}`
            break
        }
    }
    if (prev == null) {
        tinfo(`The SBOM content is unchanged over its last ${commits.length} commits, so the modification identity is not checked`)
        return
    }
    if (prev.serial == current.serial) {
        ttrue(current.version > prev.version,
            `a modified SBOM must increment its CycloneDX version: ${what} changed the components but kept version ${current.version}`)
    }
    ttrue(new Date(current.timestamp).getTime() > new Date(prev.timestamp).getTime(),
        `a modified SBOM must carry a new metadata.timestamp: ${what} changed the components but kept ${current.timestamp}`)
}

function readDoc(path: string): string | null {
    let full = join(TOP, path)
    if (!existsSync(full)) {
        tinfo(`${path} is not in this tree, so it is not checked`)
        return null
    }
    return readFileSync(full, 'utf8')
}

function checkDocuments(sbom: any) {
    let version = sbom.metadata?.component?.version
    let timestamp = sbom.metadata?.timestamp
    let components: any[] = sbom.components || []
    let count = components.length
    let inScope = components.filter(c => c.scope != 'excluded')
    let byName: Record<string, any> = {}
    for (let component of components) {
        byName[component.name] = component
        byName[component['bom-ref']] = component
    }

    let dossier = readDoc(`doc/releases/${version}/conformity-dossier.md`)
    if (dossier) {
        let m = dossier.match(/(\d+) declared \(metadata version (\S+?),.*?timestamp ([0-9TZ:-]+)/)
        ttrue(m != null, 'conformity-dossier.md must state the SBOM component count, product version and timestamp')
        if (m) {
            ttrue(Number(m[1]) == count, `conformity-dossier.md declares ${m[1]} components, the SBOM has ${count}`)
            ttrue(m[2] == version, `conformity-dossier.md names SBOM product version ${m[2]}, the SBOM has ${version}`)
            ttrue(m[3] == timestamp, `conformity-dossier.md cites SBOM timestamp ${m[3]}, the SBOM has ${timestamp}`)
        }
        let osdep = dossier.match(/osdep (\d+\.\d+\.\d+)/)
        if (osdep && byName.osdep) {
            ttrue(osdep[1] == byName.osdep.version,
                `conformity-dossier.md names osdep ${osdep[1]}, the SBOM has ${byName.osdep.version}`)
        }
    }

    let disposition = readDoc(`doc/releases/${version}/vulnerability-disposition.md`)
    if (disposition) {
        let m = disposition.match(/Component set per `doc\/compliance\/sbom\/sbom.json`.*?timestamp ([0-9TZ:-]+)/)
        ttrue(m != null, 'vulnerability-disposition.md must cite the SBOM timestamp its component set was taken from')
        if (m) {
            ttrue(m[1] == timestamp, `vulnerability-disposition.md cites SBOM timestamp ${m[1]}, the SBOM has ${timestamp}`)
        }
        let rows: Record<string, string> = {}
        for (let row of disposition.matchAll(/^\| A\d+ \| ([^|]+?) \| ([^|]+?) \|/gm)) {
            rows[row[1].trim()] = row[2].trim()
        }
        for (let component of inScope) {
            let key = component['bom-ref'] in rows ? component['bom-ref'] : component.name
            ttrue(key in rows,
                `vulnerability-disposition.md Scope A must disposition the in-scope SBOM component "${component.name}" (${component['bom-ref']})`)
            if (key in rows && component.version && /^\d+\.\d+\.\d+$/.test(rows[key])) {
                ttrue(rows[key] == component.version,
                    `vulnerability-disposition.md row ${key} names version ${rows[key]}, the SBOM has ${component.version}`)
            }
        }
    }

    let annex = readDoc('doc/compliance/cra/annex-i-matrix.md')
    if (annex) {
        let m = annex.match(/CycloneDX 1\.5, (\d+) components/)
        if (m) {
            ttrue(Number(m[1]) == count, `annex-i-matrix.md says ${m[1]} components, the SBOM has ${count}`)
        }
    }

    let management = readDoc('doc/security/vulnerability-management.md')
    if (management) {
        let m = management.match(/It lists (\d+) components/)
        if (m) {
            ttrue(Number(m[1]) == count, `vulnerability-management.md says ${m[1]} components, the SBOM has ${count}`)
        }
    }
}

if (!existsSync(MASTER)) {
    tinfo('No master SBOM in this tree, as in a source distribution')
} else {
    let text = readFileSync(MASTER, 'utf8')
    checkModificationIdentity(identity(text))
    checkDocuments(JSON.parse(text))
}
