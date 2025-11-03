#!/usr/env node
/*
   updater.js - Node.js Over-The-Air (OTA) software update client

   This is a Node.js implementation of the EmbedThis Updater for IoT devices. It provides
   the same functionality as the C version but using Node.js built-in modules.

   Features:
   - Secure HTTPS communication with the Builder cloud service
   - SHA-256 checksum verification of downloaded updates
   - Timing-safe checksum comparison to prevent timing attacks
   - Configurable timeouts for downloads and script execution
   - Secure file handling with exclusive creation and restricted permissions
   - Support for custom device properties for update policy matching

   Security:
   - Uses native fetch API with HTTPS
   - Validates Content-Length to prevent excessive downloads
   - Creates files with 0600 permissions (owner read/write only)
   - Uses timing-safe comparison for checksums
   - Verifies script executability before running
   - Implements download and script execution timeouts
   - Restricts file paths to prevent directory traversal

   Copyright (c) EmbedThis Software. All Rights Reserved.
*/
import {execFile} from 'child_process'
import {Readable} from 'stream'
import fs from 'fs'
import crypto from 'crypto'
import {timingSafeEqual} from 'crypto'
import path from 'path'

const MAX_CONTENT_LENGTH = 100 * 1024 * 1024 // Maximum download size: 100 MB
const DOWNLOAD_TIMEOUT = 10 * 60 * 1000 // Download timeout: 10 minutes
const SCRIPT_TIMEOUT = 5 * 60 * 1000 // Script execution timeout: 5 minutes
const MAX_PROPERTIES = 50 // Maximum number of device properties
const FILE_MODE = 0o600 // File permissions: owner read/write only

/**
    Display usage information and exit
 */
function usage() {
    if (!quiet) {
        console.log(
            `usage: update [options] [key=value ...]
            "--cmd script        # Script to invoke to apply the update
            "--device ID         # Unique device ID
            "--file image/path   # Path to save the downloaded update
            "--host host.domain  # Device cloud endpoint from the Builder cloud edit panel
            "--product ProductID # ProductID from the Builder token list
            "--quiet, -q         # Suppress all output (completely silent)
            "--token TokenID     # CloudAPI access token from the Builder token list
            "--version SemVer    # Current device firmware version
            "--verbose, -v       # Trace execution and show errors
            "key=value ...       # Device-specific properties for the distribution policy`
        )
    }
    process.exit(2)
}

let file = 'update.bin' // Default update file path
const properties = {} // Device-specific properties for distribution policy
let cmd, device, host, product, token, version
let verbose = false // Verbose output flag - emit trace and errors
let quiet = false // Quiet output flag - suppress all output (stdout and stderr)

/**
    Main entry point for the updater

    Performs the complete OTA update workflow:
    1. Parses command-line arguments
    2. Checks for available updates from the Builder service
    3. Downloads and verifies the update if available
    4. Applies the update using the configured script
    5. Reports the update status back to Builder
 */
async function main() {
    parseArgs()

    /*
        Request update
     */
    let body = Object.assign(
        {
            id: device,
            product: product,
            version: version,
        },
        properties
    )
    if (verbose) {
        console.log('Check for updates\n', JSON.stringify(body, null, 2), '\n')
    }
    let response = await fetch(`${host}/tok/provision/update`, {
        method: 'POST',
        headers: {
            Authorization: token,
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(body),
    })
    if (!response.ok) {
        if (!quiet) {
            console.error(response)
        }
        throw new Error('Cannot fetch update')
    }
    let data = await response.text()
    if (verbose) {
        console.log('Update response\n', data, '\n')
    }
    if (!data || data.trim() === '') {
        return
    } else {
        try {
            data = JSON.parse(data)
        } catch (error) {
            if (!quiet) {
                console.error('Invalid JSON response', data)
            }
            throw new Error('Invalid JSON response')
        }
        /*
            Update available if "url" defined
        */
        if (data.url) {
            //  Validate response completeness
            if (!data.checksum || !data.update || !data.version) {
                throw new Error('Incomplete update response')
            }

            //  Validate HTTPS
            if (!data.url.startsWith('https://')) {
                throw new Error('Insecure download URL (HTTPS required)')
            }

            //  Download update image to "path"
            try {
                await download(data.url, file)
            } catch (error) {
                if (!quiet) {
                    console.error('Download failed:', error.message)
                }
                throw new Error('Failed to download update')
            }

            //  Verify checksum
            let sum
            try {
                sum = await getChecksum(file)
            } catch (error) {
                if (!quiet) {
                    console.error('Checksum calculation failed:', error.message)
                }
                throw new Error('Failed to calculate checksum')
            }
            //  Use timing-safe comparison
            if (sum.length !== data.checksum.length || !timingSafeEqual(Buffer.from(sum), Buffer.from(data.checksum))) {
                if (!quiet) {
                    console.error(`Checksum does not match\n${sum} vs\n${data.checksum}`)
                }
                throw new Error('Update checksum does not match')
            }
            if (cmd) {
                if (verbose) {
                    console.log(`Checksum matches, apply update`)
                }
                let success = await applyUpdate(cmd, file)
                //  Post update report
                let upbody = {
                    success,
                    id: device,
                    update: data.update,
                }
                if (verbose) {
                    console.log(`Post update results ${success ? 'success' : 'failed'}`)
                }
                try {
                    let reportResponse = await fetch(`${host}/tok/provision/updateReport`, {
                        method: 'POST',
                        headers: {
                            Authorization: token,
                            'Content-Type': 'application/json',
                        },
                        body: JSON.stringify(upbody),
                    })
                    if (!reportResponse.ok) {
                        if (!quiet) {
                            console.error('Cannot post update-report')
                        }
                    }
                } catch (error) {
                    if (!quiet) {
                        console.error('Failed to post update report:', error.message)
                    }
                }
            }
        }
    }
}

/**
    Apply the update by executing the configured script

    Verifies the script is executable and runs it with the update file path as an argument.
    The script execution is subject to a timeout to prevent hanging.

    @param cmd - Path to the executable update script
    @param path - Path to the downloaded update file
    @return Promise resolving to true on success, false on failure
 */
async function applyUpdate(cmd, path) {
    //  Verify script exists and is executable
    try {
        fs.accessSync(cmd, fs.constants.X_OK)
    } catch (error) {
        if (!quiet) {
            console.error(`Script ${cmd} is not executable or does not exist`)
        }
        return false
    }

    if (verbose) {
        console.log(`Apply update ${path} using ${cmd}`)
    }
    let status = await new Promise((resolve, reject) => {
        execFile(cmd, [path], {timeout: SCRIPT_TIMEOUT}, (error) => {
            if (error) {
                if (!quiet) {
                    console.error(`Update failed`, error.message)
                }
                resolve(false)
            } else {
                if (verbose) {
                    console.log(`Update applied successfully`)
                }
                resolve(true)
            }
        })
    })
    return status
}

/**
    Parse command-line arguments

    Extracts configuration parameters and device properties from process.argv.
    Validates required arguments and property format. Exits with usage on error.
 */
function parseArgs() {
    let args = process.argv.slice(2)
    let i = 0
    for (; i < args.length; i++) {
        let arg = args[i]
        if (arg[0] != '-') break
        if (arg == '--cmd') {
            cmd = args[++i]
        } else if (arg == '--device') {
            device = args[++i]
        } else if (arg == '--file') {
            file = args[++i]
            if (path.isAbsolute(file) || file.includes('..')) {
                if (!quiet) {
                    console.error('Invalid file path. Only relative paths in current directory are allowed.')
                }
                usage()
            }
        } else if (arg == '--host') {
            host = args[++i]
        } else if (arg == '--product') {
            product = args[++i]
        } else if (arg == '--token') {
            token = args[++i]
        } else if (arg == '--version') {
            version = args[++i]
        } else if (arg == '--verbose' || arg == '-v') {
            verbose = true
        } else if (arg == '--quiet' || arg == '-q') {
            quiet = true
        } else {
            usage()
        }
    }
    let propCount = 0
    for (; i < args.length && propCount < MAX_PROPERTIES; i++, propCount++) {
        let arg = args[i]
        if (arg[0] === '-') {
            if (!quiet) {
                console.error('Options must come before properties. Found:', arg)
            }
            usage()
        }
        let [key, value] = arg.split('=')
        if (!key || !value) {
            if (!quiet) {
                console.error('Invalid property format. Expected key=value')
            }
            usage()
        }
        properties[key.trim()] = value.trim()
    }
    if (i < args.length) {
        if (!quiet) {
            console.error(`Too many properties specified (max: ${MAX_PROPERTIES})`)
        }
        usage()
    }
    if (!file || !host || !product || !token || !device || !version) {
        usage()
    }
}

/**
    Download a file from the specified URL to a local path

    Uses streaming download with timeout and size validation. Creates the file
    with exclusive access and restricted permissions. Verifies the downloaded
    file is a regular file (not a symlink or device).

    @param url - HTTPS URL to download from
    @param path - Local file path to save to
    @return Promise resolving on success, rejecting on error
 */
async function download(url, path) {
    let res = await fetch(url)
    if (!res.ok) {
        throw new Error(`Failed to fetch ${url}: ${res.statusText}`)
    }

    //  Validate Content-Length
    const contentLengthHeader = res.headers.get('content-length')
    if (!contentLengthHeader) {
        throw new Error('Missing Content-Length')
    }
    const contentLength = parseInt(contentLengthHeader, 10)
    if (isNaN(contentLength) || !isFinite(contentLength) || contentLength < 0 || contentLength > MAX_CONTENT_LENGTH) {
        throw new Error('Invalid Content-Length')
    }

    if (verbose) {
        console.log(`Downloading update to ${path}`)
    }

    //  Use 'wx' flag for exclusive creation - will fail if file exists
    //  If it fails, try to remove and retry once
    let stream
    try {
        stream = fs.createWriteStream(path, {flags: 'wx', mode: FILE_MODE})
    } catch (error) {
        if (error.code === 'EEXIST') {
            fs.unlinkSync(path)
            stream = fs.createWriteStream(path, {flags: 'wx', mode: FILE_MODE})
        } else {
            throw error
        }
    }
    Readable.fromWeb(res.body).pipe(stream)

    return await new Promise((resolve, reject) => {
        //  Add timeout
        const timeout = setTimeout(() => {
            stream.destroy()
            reject(new Error('Download timeout'))
        }, DOWNLOAD_TIMEOUT)

        stream.on('error', (error) => {
            clearTimeout(timeout)
            stream.destroy()
            //  Remove partial file
            try {
                fs.unlinkSync(path)
            } catch (e) {
                // Ignore cleanup errors
            }
            reject(error)
        })
        stream.on('finish', () => {
            clearTimeout(timeout)
            //  Verify it's a regular file
            try {
                const stats = fs.statSync(path)
                if (!stats.isFile()) {
                    fs.unlinkSync(path)
                    reject(new Error('Refusing to write to non-regular file'))
                } else {
                    resolve()
                }
            } catch (error) {
                reject(error)
            }
        })
    })
}

/**
    Calculate the SHA-256 checksum of a file

    Streams the file and computes its hash to avoid loading the entire file into memory.

    @param path - File path to checksum
    @return Promise resolving to the hexadecimal checksum string
 */
async function getChecksum(path) {
    return new Promise((resolve, reject) => {
        const hash = crypto.createHash('sha256')
        const stream = fs.createReadStream(path)
        stream.on('error', (err) => {
            reject(err)
        })
        stream.on('data', (chunk) => {
            hash.update(chunk)
        })
        stream.on('end', () => {
            const sha256 = hash.digest('hex')
            resolve(sha256)
        })
    })
}

main()
