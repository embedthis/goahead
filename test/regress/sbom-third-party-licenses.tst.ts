/*
    sbom-third-party-licenses.tst.ts - Every third-party notice in LICENSE.md is a component in the SBOM

    LICENSE.md says "The Software contains third party software under the agreements that follow" and lists each
    agreement as a "### <name> License" heading. The SBOM is the inventory of the same product, so each of those
    agreements must be carried by a component whose "embedthis:license-notice" property names the heading.
    Otherwise the two documents disagree about what third-party software the product contains.
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')
const LICENSE = join(TOP, 'LICENSE.md')
const MASTER = join(TOP, 'doc', 'compliance', 'sbom', 'sbom.json')
const SHIPPED = join(TOP, 'doc', 'sbom.json')
const SBOM = existsSync(MASTER) ? MASTER : SHIPPED

if (!existsSync(SBOM) || !existsSync(LICENSE)) {
    tskip('No SBOM or LICENSE.md in this tree')
} else {
    let license = readFileSync(LICENSE, 'utf8')
    let section = /^Third Party Licenses\s*\n-+\n([\s\S]*?)(?=^\S[^\n]*\n-+\n|(?![\s\S]))/m.exec(license)
    ttrue(section != null, 'LICENSE.md must have a "Third Party Licenses" section')
    let notices = [...(section ? section[1] : '').matchAll(/^###\s+(.+?)\s*$/gm)].map(m => m[1])
    ttrue(notices.length > 0, 'the Third Party Licenses section must list at least one agreement')

    let sbom = JSON.parse(readFileSync(SBOM, 'utf8'))
    let carried = new Set<string>()
    for (let component of sbom.components || []) {
        for (let property of component.properties || []) {
            if (property.name == 'embedthis:license-notice') {
                carried.add(property.value)
                ttrue((component.licenses || []).length > 0,
                    `component ${component.name} carries the notice "${property.value}" and must record its license`)
                ttrue(component.scope == 'required',
                    `component ${component.name} is compiled into the product, so its scope must be "required"`)
            }
        }
    }
    for (let notice of notices) {
        ttrue(carried.has(notice),
            `LICENSE.md declares third-party software under "${notice}", so the SBOM must list a component with ` +
            `property embedthis:license-notice = "${notice}"`)
    }
}
