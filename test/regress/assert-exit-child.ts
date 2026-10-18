/*
    assert-exit-child.ts - Fixture for assert-exit-status.tst.ts: a script whose only assertion fails
 */

import { ttrue } from '@embedthis/testme'

if (process.argv[2] == 'await') {
    await new Promise((resolve) => setTimeout(resolve, 50))
}
ttrue(false, 'deliberate failure')
console.log('reached after a failed assertion')
