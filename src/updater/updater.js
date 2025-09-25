#!/usr/env node
/*
   updater.js - NodeJS version of the updater
*/
import {execFile} from 'child_process'
import fs from 'fs'
import crypto from 'crypto'
import {Readable} from 'stream'
import path from 'path'

function usage() {
    console.log(
        `usage: update [options] [key=value,...]
            "--cmd script        # Script to invoke to apply the update
            "--device ID         # Unique device ID
            "--file image/path   # Path to save the downloaded update
            "--host host.domain  # Device cloud endpoint from the Builder cloud edit panel
            "--product ProductID # ProductID from the Buidler token list
            "--token TokenID     # CloudAPI access token from the Builder token list
            "--version SemVer    # Current device firmware version
            "--verbose           # Trace execution
            "key:value,...       # Device-specific properties for the distribution policy`)
    process.exit(2)
}

let file = 'update.bin'
let properties = {}
let cmd, device, host, product, token, version, verbose

async function main() {
    parseArgs()

    /*
        Request update
     */
    let body = Object.assign({
        id: device,
        product: product,
        version: version,
    }, properties)
    if (verbose) {
        console.log('Check for updates\n', JSON.stringify(body, null, 2), '\n')
    }
    let response = await fetch(`${host}/tok/provision/update`, {
        method: 'POST',
        headers: {
            'Authorization': token,
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(body), 
    })
    if (!response.ok) {
        console.error(response)
        throw new Error('Cannot fetch update')
    }
    let data = await response.text()
    if (verbose) {
        console.log('Update response\n', data, '\n')
    }
    if (!data) {
        console.log('No update required')
    } else {
        try {
            data = JSON.parse(data)
        } catch (error) {
            console.error('Invalid JSON response', data)
            throw new Error('Invalid JSON response')
        }
        /*
            Update available if "url" defined
        */
        if (data.url) {
            //  Download update image to "path"
            await download(data.url, file)

            //  Verify checksum
            let sum = await getChecksum(file)
            if (sum != data.checksum) {
                throw new Error('Update checksum does not match')
            }
            if (cmd) {
                console.log(`Checksum matches, apply update`)
                let success = await applyUpdate(cmd, file)
                //  Post update report
                let body = {
                    success,
                    id: device,
                    update: data.update,
                }
                if (verbose) {
                    console.log(`Post update results ${success ? 'success' : 'failed'}`)
                }
                await fetch(`${host}/tok/provision/updateReport`, {
                    method: 'POST',
                    headers: {
                        'Authorization': token,
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify(body), 
                })
            }
        }
    }
}

/*
    Apply the update by running the external "cmd" script
 */
async function applyUpdate(cmd, path) {
    if (verbose) {
        console.log(`Apply update ${path} using ${cmd}`)
    }
    let status = await new Promise((resolve, reject) => {
        execFile(cmd, [path], (error) => {
            if (error) {
                console.error(`Update failed`, error.message)
                resolve(false)
            } else {
                console.log(`Update applied successfully`)
                resolve(true)
            }
        })
    })
    return status
}

function parseArgs() {
    let args = process.argv.slice(2)
    let i = 0
    for (; i < args.length; i++) {
        let arg = args[i]
        if (arg[0] != '-') break
        if (arg == "--cmd") {
            cmd = args[++i]

        } else if (arg == "--device") {
            device = args[++i]

        } else if (arg == "--file") {
            file = args[++i]
            if (path.isAbsolute(file) || file.includes('..')) {
                console.error('Invalid file path. Only relative paths in current directory are allowed.')
                usage()
            }

        } else if (arg == "--host") {
            host = args[++i]

        } else if (arg == "--product") {
            product = args[++i]

        } else if (arg == "--token") {
            token = args[++i]

        } else if (arg == "--version") {
            version = args[++i]

        } else if (arg == "--verbose" || arg == "-v") {
            verbose = 1

        } else {
            usage()
        }
    }
    for (; i < args.length; i++) {
        let [key, value] = args[i].split('=')
        properties[key.trim()] = value.trim()
    }
    if (!file || !host || !product || !token || !device || !version) {
        usage()
    }
}

/*
    Stream download the file from the given url to the path
 */
async function download(url, path) {
    let res = await fetch(url)
    if (!res.ok) {
        throw new Error(`Failed to fetch ${url}: ${res.statusText}`)
    }
    const stream = fs.createWriteStream(path)
    Readable.fromWeb(res.body).pipe(stream)

    return await new Promise((resolve, reject) => {
        stream.on('error', reject)
        stream.on('finish', resolve)
    })
}

/*
    Get the checksum of the given file
 */
async function getChecksum(path) {
    return new Promise((resolve, reject) => {
        const hash = crypto.createHash('sha256')
        const stream = fs.createReadStream(path)
        stream.on('error', (err) => { reject(err) })
        stream.on('data', (chunk) => { hash.update(chunk) })
        stream.on('end', () => {
            const sha256 = hash.digest('hex')
            resolve(sha256)
        })
    })
}
        
main()