# EmbedThis Updater

The EmbedThis Updater is a command-line utility and library for downloading and applying device software updates published on the [EmbedThis Builder](https://admin.embedthis.com).

## Description

Devices can use the **updater** command or library to enable Over-The-Air software update capabilities. The EmbedThis Builder offers hosting and global distribution for updates, policy-based update distribution, and gradual rollout and control of updates. Additionally, graphical metrics and reporting are available.

## Variants

This repository provides different versions of a standalone **updater** program.

* A C code command line utility
* A NodeJS command line utility
* A C updater library for integration in your programs

A shell script sample **updater.sh** is also provided. This must be customized with necessary arguments.

## Device Agents

The [Ioto](https://www.embedthis.com/ioto/) device agent includes the updater functionality internally. The [Appweb](https://www.embedthis.com/appweb/) and [GoAhead](https://www.embedthis.com/goahead/) web servers include this repositiory under their **src/updater** directories.

All devices using other embedded web servers and device agents can include this Updater to include OTA software update functionality.

## Updater Command

    updater [options] key=value,...

Where options are:

Option | Description
-|-
--cmd script        | Script to invoke to apply the update
--device ID         | Unique device ID
--file image/path   | Path to save the downloaded update
--host host.domain  | Device cloud endpoint from the Builder cloud edit panel
--product ProductID | ProductID from the Buidler token list
--token TokenID     | CloudAPI access token from the Builder token list
--version SemVer    | Current device firmware version

The key=value pairs can provide device specific properties that can be used by the Builder software
update policy to determine which devices receive the update.

### Example:

    updater -v --device "A123456789" \
        --host "https://ygp2t8ckqj.execute-api.ap-southeast-1.amazonaws.com" \
        --product "XABCDACC2T1234567890123455" \
        --token "TT488ETG5H1234567890123456" \
        --version "2.1.2" \
        --cmd ./apply.sh \
        pro=true
    
Replace the host, product and token with values from your Builder account.

## Library

You can use the updater.c source file and invoke the update() API from your programs.

## Building

You can use the supplied Makefile to build the updater program and library.

## Files

File | Description
-|-
Makefile | Local Makefile to build update program.
apply.sh | Script to apply the update to the device. Customize as you need.
main.c | Main program for the updater.
updater.c | Update library source.
updater.h | Update library header.
updater.sh | Sample shell script to customize.
updater.js | NodeJS command line updater utility.