/* The client filename is validated as sent: one plain name, never normalized, no trailing dot or device name */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { existsSync, readdirSync, unlinkSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)
const BOUNDARY = '----GoAheadUploadFilename'

//  Upload staging directory and the directory the sample handler saves to, relative to this test directory
const STAGING = '../tmp'
const SAVED = '../web/tmp'

async function rawPost(body: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('POST /action/uploadTest HTTP/1.0\r\n' +
                  'Host: localhost\r\n' +
                  'Content-Type: multipart/form-data; boundary=' + BOUNDARY + '\r\n' +
                  'Content-Length: ' + body.length + '\r\n' +
                  'Connection: close\r\n\r\n' + body)
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

function upload(filename: string): Promise<string> {
    return rawPost('--' + BOUNDARY + '\r\n' +
        'Content-Disposition: form-data; name="upload"; filename="' + filename + '"\r\n' +
        'Content-Type: application/octet-stream\r\n\r\n' +
        'data\r\n' +
        '--' + BOUNDARY + '--\r\n')
}

function listing(): string {
    return readdirSync(STAGING).sort().join(',') + '|' + readdirSync(SAVED).sort().join(',')
}

async function refused(filename: string) {
    let before = listing()
    let response = await upload(filename)
    ttrue(status(response) == 400, `[${filename}] must be refused with 400, got ${status(response)}`)
    ttrue(listing() == before, `[${filename}] must leave no staging or saved file`)
}

async function accepted(filename: string) {
    let response = await upload(filename)
    ttrue(status(response) == 200, `[${filename}] must be accepted, got ${status(response)}`)
    ttrue(response.contains('\r\nCLIENT=' + filename + '\r\n'), `[${filename}] must be published exactly as sent`)
    let saved = SAVED + '/' + filename
    if (filename != '' && existsSync(saved)) {
        unlinkSync(saved)
    }
}

if (thas('ME_GOAHEAD_UPLOAD')) {

    if (existsSync(SAVED + '/malicious.cgi')) {
        unlinkSync(SAVED + '/malicious.cgi')
    }

    //  Dot segments and separators are refused, never collapsed into a different name
    for (let name of ['a/../../malicious.cgi', '..', '.', '../../../tmp/malicious.cgi', '%2e%2e/x', '..\\x',
                      'dir/a.txt', 'C:\\dir\\a.txt']) {
        await refused(name)
    }
    ttrue(!existsSync(SAVED + '/malicious.cgi'), 'a collapsed name must not be saved')

    //  A trailing dot is refused on every platform: Windows silently drops it
    for (let name of ['report.', 'a.jst.', 'report.exe.', '.hidden']) {
        await refused(name)
    }

    //  Plain names, including a no-file-selected input, are unchanged
    for (let name of ['small.dat', 'a.tar.gz', '']) {
        await accepted(name)
    }

    //  Windows device names are reserved only on Windows, where they would open a device
    if (process.platform == 'win32') {
        for (let name of ['NUL', 'CON.txt', 'COM1', 'com1.txt', 'aux']) {
            await refused(name)
        }
    } else {
        for (let name of ['NUL', 'CON.txt', 'COM1', 'aux.txt']) {
            await accepted(name)
        }
    }
} else {
    tskip('Upload support not enabled')
}
