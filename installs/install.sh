#!/bin/bash
#
#   install: Installation script
#
#   Copyright (c) Embedthis Software LLC, 2003-2014. All Rights Reserved.
#
#   Usage: install [configFile]
#
################################################################################
#
#   The configFile is of the format:
#       FMT=[rpm|deb|tar]               # Package format to use
#       installbin=[YN]                 # Install binary package
#       runDaemon=[YN]                  # Run the program as a daemon
#       httpPort=portNumber             # Http port to listen on
#       sslPort=portNumber              # SSL port to listen on
#       username=username               # User account for goahead
#       groupname=groupname             # Group account for goahead
#       hostname=groupname              # Serving host
#

HOME=`pwd`
FMT=
SITE=localhost
PAGE=/index.html

HOSTNAME=`hostname`
COMPANY="${settings.company}"
PRODUCT="${settings.name}"
NAME="${settings.title}"
VERSION="${settings.version}"
OS="${platform.os}"
CPU="${platform.arch}"
DIST="${platform.dist}"

ROOT_PREFIX="${prefixes.root}"
BASE_PREFIX="${prefixes.base}"
STATE_PREFIX="${prefixes.state}"
APP_PREFIX="${prefixes.app}"
VAPP_PREFIX="${prefixes.vapp}"
BIN_PREFIX="${prefixes.bin}"
SBIN_PREFIX="${prefixes.sbin}"
ETC_PREFIX="${prefixes.etc}"
INC_PREFIX="${prefixes.inc}"
LIB_PREFIX="${prefixes.lib}"
MAN_PREFIX="${prefixes.man}"
WEB_PREFIX="${prefixes.web}"

ABIN="${VAPP_PREFIX}/bin"
AINC="${VAPP_PREFIX}/in"

installbin=Y
runDaemon=Y
headless=${HEADLESS:-0}
HTTP_PORT=80
SSL_PORT=443

PATH="$PATH:/sbin:/usr/sbin"
unset CDPATH
export CYGWIN=nodosfilewarning
CYGWIN=nodosfilewarning

###############################################################################

setup() {
    umask 022
    if [ $OS != windows -a `id -u` != "0" ] ; then
        echo "You must be root to install this product."
        exit 255
    fi
    #
    #   Headless install
    #
    if [ $# -ge 1 ] ; then
        if [ ! -f $1 ] ; then
            echo "Could not find installation config file \"$1\"." 1>&2
            exit 255
        else
            . $1 
            installFiles $FMT
            
            if [ "$installbin" = "Y" ] ; then
                appman stop disable uninstall >/dev/null 2>&1
                patchConfiguration
                appman install
                if [ "$runDaemon" = "Y" ] ; then
                    appman enable start
                fi
            fi

        fi
        exit 0
    fi
    sleuthPackageFormat
    getAccountDetails
    [ "$headless" != 1 ] && echo -e "\n$NAME ${VERSION} Installation\n"

}

getAccountDetails() {

    local g u

    #
    #   Select default username
    #
    for u in www-data _www nobody Administrator 
    do
        grep "$u" /etc/passwd >/dev/null
        if [ $? = 0 ] ; then
            username=$u
            break
        fi
    done
    if [ "$username" = "" ] ; then
        echo "Can't find a suitable username in /etc/passwd for $PRODUCT" 1>&2
        exit 255
    fi
    
    #
    #   Select default group name
    #
    for g in www-data _www nogroup nobody Administrators
    do
        grep "$g" /etc/group >/dev/null
        if [ $? = 0 ] ; then
            groupname=$g
            break
        fi
    done
    if [ "$groupname" = "" ] ; then
        echo "Can't find a suitable group in /etc/group for $PRODUCT" 1>&2
        exit 255
    fi
}

sleuthPackageFormat() {
    local name

    FMT=
    name=`createPackageName ${PRODUCT}-bin`
    if [ -x contents ] ; then
        FMT=tar
    else
        for f in deb rpm tgz ; do
            if [ -f ${name}.${f} ] ; then
                FMT=${f%.gz}
                break
            fi
        done
    fi
    if [ "$FMT" = "" ] ; then
        echo -e "\nYou may be be missing a necessary package file. "
        echo "Check that you have the correct $NAME package".
        exit 255
    fi
}

askUser() {
    local finished

    [ "$headless" != 1 ] && echo "Enter requested information or press <ENTER> to accept the default. "

    #
    #   Confirm the configuration
    #
    finished=N
    while [ "$finished" = "N" ]
    do
        [ "$headless" != 1 ] && echo
        installbin=`yesno "Install binary package" "$installbin"`
        if [ "$installbin" = "Y" ] ; then
            runDaemon=`yesno "Start $PRODUCT automatically at system boot" $runDaemon`
            HTTP_PORT=`ask "Enter the HTTP port number" "$HTTP_PORT"`
            # SSL_PORT=`ask "Enter the SSL port number" "$SSL_PORT"`
            username=`ask "Enter the user account for $PRODUCT" "$username"`
            groupname=`ask "Enter the user group for $PRODUCT" "$groupname"`
        else
            runDaemon=N
        fi
    
        if [ "$headless" != 1 ] ; then
            echo -e "\nInstalling with this configuration:" 
            echo -e "    Install binary package: $installbin"
        
            if [ "$installbin" = "Y" ] ; then
                echo -e "    Start automatically at system boot: $runDaemon"
                echo -e "    HTTP port number: $HTTP_PORT"
                # echo -e "    SSL port number: $SSL_PORT"
                echo -e "    Username: $username"
                echo -e "    Groupname: $groupname"
            fi
            echo
        fi
        finished=`yesno "Accept this configuration" "Y"`
    done
    [ "$headless" != 1 ] && echo
    
    if [ "$installbin" = "N" ] ; then
        echo -e "Nothing to install, exiting. "
        exit 0
    fi
}

createPackageName() {
    echo ${1}-${VERSION}-${DIST}-${OS}-${CPU}
}

# 
#   Get a yes/no answer from the user. Usage: ans=`yesno "prompt" "default"`
#   Echos 1 for Y or 0 for N
#
yesno() {
    local ans

    if [ "$headless" = 1 ] ; then
        echo "Y"
        return
    fi
    echo -n "$1 [$2] : " 1>&2
    while [ 1 ] 
    do
        read ans
        if [ "$ans" = "" ] ; then
            echo $2 ; break
        elif [ "$ans" = "Y" -o "$ans" = "y" ] ; then
            echo "Y" ; break
        elif [ "$ans" = "N" -o "$ans" = "n" ] ; then
            echo "N" ; break
        fi
        echo -e "\nMust enter a 'y' or 'n'\n " 1>&1
    done
}

# 
#   Get input from the user. Usage: ans=`ask "prompt" "default"`
#   Returns the answer or default if <ENTER> is pressed
#
ask() {
    local ans

    default=$2
    if [ "$headless" = 1 ] ; then
        echo "$default"
        return
    fi
    echo -n "$1 [$default] : " 1>&2
    read ans
    if [ "$ans" = "" ] ; then
        echo $default
    fi
    echo $ans
}

removeOld() {
    if [ -x /usr/lib/goahead/bin/uninstall ] ; then
        goahead_HEADLESS=1 /usr/lib/goahead/bin/uninstall </dev/null 2>&1 >/dev/null
    fi
}

installFiles() {
    local dir pkg doins NAME upper target

    [ "$headless" != 1 ] && echo -e "\nExtracting files ...\n"

    for pkg in bin ; do
        doins=`eval echo \\$install${pkg}`
        if [ "$doins" = Y ] ; then
            upper=`echo $pkg | tr '[:lower:]' '[:upper:]'`
            suffix="-${pkg}"
            NAME=`createPackageName ${PRODUCT}${suffix}`.$FMT
            if [ "$runDaemon" != "Y" ] ; then
                export APPWEB_DONT_START=1
            fi
            if [ "$FMT" = "rpm" ] ; then
                [ "$headless" != 1 ] && echo -e "rpm -Uhv $NAME"
                rpm -Uhv $HOME/$NAME
            elif [ "$FMT" = "deb" ] ; then
                [ "$headless" != 1 ] && echo -e "dpkg -i $NAME"
                dpkg -i $HOME/$NAME >/dev/null
            elif [ "$FMT" = "tar" ] ; then
                target=/
                [ $OS = windows ] && target=`cygpath ${HOMEDRIVE}/`
                (cd contents ; tar cf - . | (cd $target && tar xBf -))
            fi
        fi
    done

    if [ -f /etc/redhat-release -a -x /usr/bin/chcon ] ; then 
        if sestatus | grep enabled >/dev/nulll ; then
            for f in $ABIN/*.so ; do
                chcon /usr/bin/chcon -t texrel_shlib_t $f 2>&1 >/dev/null
            done
        fi
    fi

    [ "$headless" != 1 ] && echo -e "\nSetting file permissions ..."
    if [ $OS = windows ] ; then
        # Cygwin bug. Chmod fails to stick if not in current directory for paths with spaces
        home=`pwd` >/dev/null
        cd "$ETC_PREFIX" ; chmod -R g+r,o+r . ; find . -type d -exec chmod 755 "{}" \;
        cd "$APP_PREFIX" ; chmod -R g+r,o+r . ; find . -type d -exec chmod 755 "{}" \;
        cd "$WEB_PREFIX" ; chmod -R g+r,o+r . ; find . -type d -exec chmod 755 "{}" \;
        cd "$ABIN" ; chmod 755 *.dll *.exe
        cd "$ETC_PREFIX" ; chmod 777 . ; chown $username .
        cd "${home}" >/dev/null
    fi
    [ "$headless" != 1 ] && echo
}

patchConfiguration() {
    :
}

#
#   Not currently used
#
startBrowser() {
    if [ "$headless" = 1 ] ; then
        return
    fi
    #
    #   Conservative delay to allow goahead to start and initialize
    #
    sleep 5
    [ "$headless" != 1 ] && echo -e "Starting browser to view the $NAME Home Page."
    if [ $OS = windows ] ; then
        cygstart --shownormal http://$SITE:$HTTP_PORT$PAGE 
    elif [ $OS = macosx ] ; then
        open http://$SITE:$HTTP_PORT$PAGE >/dev/null 2>&1 &
    else
        for f in /usr/bin/htmlview /usr/bin/firefox /usr/bin/mozilla /usr/bin/konqueror 
        do
            if [ -x ${f} ] ; then
                sudo -H -b ${f} http://$SITE:$HTTP_PORT$PAGE >/dev/null 2>&1 &
                break
            fi
        done
    fi
}

#
#   Main program for install script
#
setup $*
askUser

if [ "$installbin" = "Y" ] ; then
    [ "$headless" != 1 ] && echo "Disable existing service"
    appman stop disable uninstall >/dev/null 2>&1
    if [ $OS = windows ] ; then
        "$ABIN/goaheadMonitor" --stop >/dev/null 2>&1
    fi
fi
removeOld
installFiles $FMT

if [ "$installbin" = "Y" ] ; then
    "$ABIN/appman" stop disable uninstall >/dev/null 2>&1
    patchConfiguration
    "$ABIN/appman" install
    if [ "$runDaemon" = "Y" ] ; then
        "$ABIN/appman" enable
        "$ABIN/appman" start
    fi
fi
if [ $OS = windows ] ; then
    "$ABIN/goaheadMonitor" >/dev/null 2>&1 &
fi
if [ "$headless" != 1 ] ; then
    echo -e "$NAME installation successful."
fi
exit 0
