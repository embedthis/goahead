#!/usr/bin/env bash
#
#   apply.sh - Apply update to your device
#

IMAGE=$1
: ${STATUS:=0}

# Apply update ${IAMGE} here and set STATUS
echo "apply.sh ${IMAGE}"

exit $STATUS