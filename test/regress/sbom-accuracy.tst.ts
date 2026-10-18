/*
    sbom-accuracy.tst.ts - The SBOM names the release and matches the components in the source tree
 */

import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')

// The repository keeps the master SBOM; a source distribution ships the release copy as doc/sbom.json.
const MASTER = join(TOP, 'doc', 'compliance', 'sbom', 'sbom.json')
const SHIPPED = join(TOP, 'doc', 'sbom.json')
const SBOM = existsSync(MASTER) ? MASTER : SHIPPED

function readJson(path: string): any {
    return JSON.parse(readFileSync(path, 'utf8'))
}

if (!existsSync(SBOM)) {
    tskip('No SBOM in this tree')
} else {
    let sbom = readJson(SBOM)
    let pkg = readJson(join(TOP, 'package.json'))
    let version = pkg.version
    let components: Record<string, any> = {}
    for (let component of sbom.components || []) {
        components[component.name] = component
    }

    ttrue(sbom.metadata?.component?.version == version,
        `the SBOM product version "${sbom.metadata?.component?.version}" must equal package.json "${version}"`)
    ttrue(sbom.metadata?.component?.purl == `pkg:generic/goahead@${version}`,
        `the SBOM product purl "${sbom.metadata?.component?.purl}" must name ${version}`)

    for (let name of Object.keys(pkg.paks || {})) {
        let component = components[name]
        ttrue(component != null, `the SBOM must list the ${name} pak`)
        if (!component) {
            continue
        }
        ttrue(component.purl == `pkg:generic/${name}@${component.version}`,
            `the ${name} purl "${component.purl}" must name its version ${component.version}`)
        let manifest = join(TOP, 'paks', name, 'package.json')
        if (existsSync(manifest)) {
            let pakVersion = readJson(manifest).version
            ttrue(component.version == pakVersion,
                `the SBOM ${name} version "${component.version}" must equal paks/${name} "${pakVersion}"`)
        } else {
            tinfo(`paks/${name} is not installed, so its version is not checked. Run "pak install" to check it.`)
        }
    }

    let lock = readJson(join(TOP, 'package-lock.json'))
    for (let name of Object.keys(pkg.devDependencies || {})) {
        let locked = lock.packages?.[`node_modules/${name}`]?.version
        let component = components[name]
        ttrue(component != null, `the SBOM must list ${name}`)
        ttrue(component?.version == locked,
            `the SBOM ${name} version "${component?.version}" must equal package-lock.json "${locked}"`)
    }

    let osdep = createHash('sha256').update(readFileSync(join(TOP, 'src', 'osdep', 'osdep.h'))).digest('hex')
    let hash = components.osdep?.hashes?.find((h: any) => h.alg == 'SHA-256')?.content
    ttrue(hash == osdep, `the SBOM osdep SHA-256 "${hash}" must equal that of src/osdep/osdep.h "${osdep}"`)

    let premake = readFileSync(join(TOP, 'projects', 'premake5.lua'), 'utf8')
    if (/links\s*\{[^}]*"pam"/.test(premake)) {
        ttrue(components.libpam != null, 'projects/premake5.lua links pam, so the SBOM must list libpam')
    }

    let release = join(TOP, 'doc', 'releases', version, 'sbom.json')
    if (SBOM == MASTER && existsSync(release)) {
        ttrue(readFileSync(release).equals(readFileSync(MASTER)),
            `doc/releases/${version}/sbom.json must be a copy of the master doc/compliance/sbom/sbom.json`)
    }
}
