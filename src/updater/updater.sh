#!/usr/bin/env bash
#
#   updater.sh - Shell-based Over-The-Air (OTA) software update client
#
#   This is a sample shell script implementation of the EmbedThis Updater demonstrating the
#   complete OTA update workflow. It is provided as a reference implementation and starting point.
#
#   Features:
#   - Uses standard Unix tools (curl, jq, openssl)
#   - Secure HTTPS communication with the Builder service
#   - SHA-256 checksum verification
#   - Secure temporary file handling
#   - Comprehensive error checking
#   - Safety limits on download size and timeout
#
#   Security:
#   - Creates secure temporary directory with mktemp
#   - Automatic cleanup on exit via trap
#   - HTTPS-only downloads with fail-fast on errors
#   - Validates JSON responses
#   - Enforces maximum file size and timeout limits
#   - Checksum verification before applying updates
#
#   Dependencies: curl, jq, openssl
#
#   Copyright (c) EmbedThis Software. All Rights Reserved.
#

#
#   Display usage information and exit
#
usage() {
    if [[ "$QUIET" != "1" ]]; then
        cat <<EOF
usage: updater.sh [options] [key=value ...]
        --cmd script        # Script to invoke to apply the update
        --device ID         # Unique device ID
        --file image/path   # Path to save the downloaded update
        --host host.domain  # Device cloud endpoint from the Builder cloud edit panel
        --product ProductID # ProductID from the Builder token list
        --quiet, -q         # Suppress all output (completely silent)
        --token TokenID     # CloudAPI access token from the Builder token list
        --version SemVer    # Current device firmware version
        --verbose, -v       # Trace execution and show errors
        key=value ...       # Device-specific properties for the distribution policy
EOF
    fi
    exit 2
}

#
#   Verbose logging (trace output - only when --verbose)
#
log() {
    if [[ "$VERBOSE" == "1" ]]; then
        echo "$@"
    fi
}

#
#   Error output (suppressed by --quiet, always shown by default or with --verbose)
#
error() {
    if [[ "$QUIET" != "1" ]]; then
        echo "$@" >&2
    fi
}

#
#   Parse command-line arguments
#
parse_args() {
    local key value

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --cmd)
                APPLY="$2"
                shift 2
                ;;
            --device)
                DEVICE="$2"
                shift 2
                ;;
            --file)
                UPDATE="$2"
                shift 2
                ;;
            --host)
                ENDPOINT="$2"
                shift 2
                ;;
            --product)
                PRODUCT="$2"
                shift 2
                ;;
            --token)
                TOKEN="$2"
                shift 2
                ;;
            --version)
                VERSION="$2"
                shift 2
                ;;
            --verbose|-v)
                VERBOSE=1
                shift
                ;;
            --quiet|-q)
                QUIET=1
                shift
                ;;
            -*)
                error "Unknown option: $1"
                usage
                ;;
            *=*)
                # Device property in key=value format
                key="${1%%=*}"
                value="${1#*=}"
                if [[ -z "$key" || -z "$value" ]]; then
                    error "Invalid property format. Expected key=value"
                    usage
                fi
                if [[ -n "$PROPERTIES" ]]; then
                    PROPERTIES="$PROPERTIES,\"$key\":\"$value\""
                else
                    PROPERTIES="\"$key\":\"$value\""
                fi
                shift
                ;;
            *)
                error "Invalid argument: $1"
                usage
                ;;
        esac
    done

    # Validate required parameters
    if [[ -z "$DEVICE" || -z "$ENDPOINT" || -z "$PRODUCT" || -z "$TOKEN" || -z "$VERSION" ]]; then
        error "Missing required parameters"
        usage
    fi

    # Set default file path if not provided
    if [[ -z "$UPDATE" ]]; then
        UPDATE="$TMPDIR/update.bin"
    fi
}

# Initialize variables
DEVICE=""
ENDPOINT=""
PRODUCT=""
TOKEN=""
VERSION=""
APPLY=""
UPDATE=""
PROPERTIES=""
VERBOSE=0
QUIET=0

#
#   Create a secure temporary directory and configure automatic cleanup
#
TMPDIR=$(mktemp -d)

if [[ ! "$TMPDIR" || ! -d "$TMPDIR" ]]; then
    error "Could not create temp dir"
    exit 1
fi

# Ensure temporary directory is cleaned up on exit (success or failure)
trap "rm -rf '$TMPDIR'" EXIT

# Parse command-line arguments
parse_args "$@"

# Define paths for temporary files
DATA="$TMPDIR/data.tmp" # Request/response data
OUTPUT="$TMPDIR/output.tmp" # API responses

log "Checking for updates..."
log "Device: $DEVICE"
log "Version: $VERSION"
log "Endpoint: $ENDPOINT"

#
#   Update request. Can add custom device properties to use in the distribution policy
#
if [[ -n "$PROPERTIES" ]]; then
    cat >"$DATA" <<EOF
{
    "id":"${DEVICE}",
    "product":"${PRODUCT}",
    "version":"${VERSION}",
    ${PROPERTIES}
}
EOF
else
    cat >"$DATA" <<EOF
{
    "id":"${DEVICE}",
    "product":"${PRODUCT}",
    "version":"${VERSION}"
}
EOF
fi

log "Request: $(cat "$DATA")"

#
#   Check for an update. Silent and fail on non-200 status.
#
if [[ "$VERBOSE" == "1" ]]; then
    curl_verbose="-v"
else
    curl_verbose="-s"
fi

curl -f $curl_verbose --max-time 30 -X POST \
    -H "Authorization:${TOKEN}" \
    -H "Content-Type:application/json" \
    -d "@${DATA}" "${ENDPOINT}/tok/provision/update" >"$OUTPUT"
if [ $? -ne 0 ] ; then
    error "Failed to check for update"
    exit 1
fi

cat "$OUTPUT" | jq empty >/dev/null 2>&1
if [ $? -ne 0 ] ; then
    # Response did not parse
    error "Invalid JSON response"
    if [[ "$QUIET" != "1" ]]; then
        cat "$OUTPUT" >&2
        echo >&2
    fi
    exit 1
fi

log "Response: $(cat "$OUTPUT")"

if [ "$(cat "$OUTPUT")" = "{}" ] ; then
    exit 0
fi

#
#   Parse output and extract fields
#
URL=$(cat "$OUTPUT" | jq -r .url)
CHECKSUM=$(cat "$OUTPUT" | jq -r .checksum)
UPDATEID=$(cat "$OUTPUT" | jq -r .update)
VERSION_NEW=$(cat "$OUTPUT" | jq -r .version)

if [ "$URL" = "null" -o "$URL" = "" ] ; then
    exit 0
fi

#
#   Validate required fields
#
if [ "$CHECKSUM" = "null" -o "$UPDATEID" = "null" -o "$VERSION_NEW" = "null" ] ; then
    error "Incomplete update response"
    exit 1
fi

#
#   Validate HTTPS
#
if [[ ! "$URL" =~ ^https:// ]] ; then
    error "Insecure download URL (HTTPS required)"
    exit 1
fi

log "Download URL: $URL"

#
#   Fetch the update with safety checks
#
log "Downloading update..."
curl -f $curl_verbose --max-filesize 104857600 --max-time 600 "$URL" >"$UPDATE"
if [ $? -ne 0 ] ; then
    error "Failed to download update"
    exit 1
fi

log "Download complete"

#
#   Validate the checksum
#
log "Verifying checksum..."
SUM=$(openssl dgst -sha256 "$UPDATE" | awk '{print $2}')
if [ "$SUM" != "$CHECKSUM" ] ; then
    error "Checksum does not match"
    log "Expected: $CHECKSUM"
    log "Received: $SUM"
    exit 1
fi
log "Checksum matches, apply update"

#
#   Customize to apply update here and set success to true/false
#
if [ -n "$APPLY" ] ; then
    log "Applying update with: $APPLY $UPDATE"
    "$APPLY" "$UPDATE"
    if [ $? -ne 0 ] ; then
        error "Apply update failed"
        success=false
    else
        log "Update applied successfully"
        success=true
    fi
else
    log "No apply script specified - update downloaded and verified only"
    success=true
fi

#
#   Post update status
#
log "Reporting update status..."
cat >"$DATA" <<EOF
{
    "success":"${success}",
    "id":"${DEVICE}",
    "update":"${UPDATEID}"
}
EOF

curl -f $curl_verbose --max-time 30 -X POST \
    -H "Authorization:${TOKEN}" \
    -H "Content-Type:application/json" \
    -d "@${DATA}" "${ENDPOINT}/tok/provision/updateReport" >/dev/null 2>&1
if [ $? -ne 0 ] ; then
    error "Failed to post update report"
    exit 1
fi

log "Update complete"
exit 0
