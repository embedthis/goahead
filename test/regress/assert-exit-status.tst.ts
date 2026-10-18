/* A failed assertion in a TypeScript test fails the test */

import { join } from 'node:path'

const CHILD = join(import.meta.dir, 'assert-exit-child.ts')

for (const mode of ['sync', 'await']) {
    const proc = Bun.spawn(['bun', CHILD, mode], {stdout: 'pipe', stderr: 'pipe', stdin: 'pipe'})
    proc.stdin.end()
    const [status, out, err] = await Promise.all([
        proc.exited,
        new Response(proc.stdout).text(),
        new Response(proc.stderr).text(),
    ])
    if (!err.includes('deliberate failure')) {
        throw new Error(`${mode}: the failed assertion was not reported: ${err}`)
    }
    if (out.includes('reached after a failed assertion')) {
        throw new Error(`${mode}: the script continued after a failed assertion`)
    }
    if (status == 0) {
        throw new Error(`${mode}: a failed assertion exited 0, so the runner would report the test as passing`)
    }
    console.log(`✓ ${mode}: a failed assertion exits ${status}`)
}
