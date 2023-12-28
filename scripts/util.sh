#!/bin/bash

# OS detection: https://stackoverflow.com/a/3792848
lowercase(){
    echo "$1" | sed "y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/"
}

OS=`lowercase \`uname\``
KERNEL=`uname -r`
MACH=`uname -m`

if [ "{$OS}" = "windowsnt" ]; then
    OS=windows
elif [ "{$OS}" = "darwin" ]; then
    OS=mac
else
    OS=`uname`
    if [ "${OS}" = "SunOS" ] ; then
        OS=Solaris
        ARCH=`uname -p`
        OSSTR="${OS} ${REV}(${ARCH} `uname -v`)"
    elif [ "${OS}" = "AIX" ] ; then
        OSSTR="${OS} `oslevel` (`oslevel -r`)"
    elif [ "${OS}" = "Linux" ] ; then
        if [ -f /etc/redhat-release ] ; then
            DistroBasedOn='RedHat'
            DIST=`cat /etc/redhat-release |sed s/\ release.*//`
            PSUEDONAME=`cat /etc/redhat-release | sed s/.*\(// | sed s/\)//`
            REV=`cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*//`
        elif [ -f /etc/SuSE-release ] ; then
            DistroBasedOn='SuSe'
            PSUEDONAME=`cat /etc/SuSE-release | tr "\n" ' '| sed s/VERSION.*//`
            REV=`cat /etc/SuSE-release | tr "\n" ' ' | sed s/.*=\ //`
        elif [ -f /etc/mandrake-release ] ; then
            DistroBasedOn='Mandrake'
            PSUEDONAME=`cat /etc/mandrake-release | sed s/.*\(// | sed s/\)//`
            REV=`cat /etc/mandrake-release | sed s/.*release\ // | sed s/\ .*//`
        elif [ -f /etc/debian_version ] ; then
            DistroBasedOn='Debian'
            DIST=`cat /etc/lsb-release | grep '^DISTRIB_ID' | awk -F=  '{ print $2 }'`
            PSUEDONAME=`cat /etc/lsb-release | grep '^DISTRIB_CODENAME' | awk -F=  '{ print $2 }'`
            REV=`cat /etc/lsb-release | grep '^DISTRIB_RELEASE' | awk -F=  '{ print $2 }'`
        fi
        if [ -f /etc/UnitedLinux-release ] ; then
            DIST="${DIST}[`cat /etc/UnitedLinux-release | tr "\n" ' ' | sed s/VERSION.*//`]"
        fi
        OS=`lowercase $OS`
        DistroBasedOn=`lowercase $DistroBasedOn`
        readonly OS
        readonly DIST
        readonly DistroBasedOn
        readonly PSUEDONAME
        readonly REV
        readonly KERNEL
        readonly MACH
    fi
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

warning() {
    echo "Warning! $1" 1>&2
}

error() {
    echo "Error! $1" 1>&2
    exit 1
}

# OS abstraction layer. Runs 'premake5 gmake' on Linux, 'premake5 vs2022' on
# Windows and 'premake5 xcode4' on Mac
run_premake() {
    if [ "$OS" = "linux" ]; then
        premake5 gmake
    elif [ "$OS" = "windows" ]; then
        premake5 vs2022
    elif [ "$OS" = "mac" ]; then
        premake5 xcode4
    else
        error "Unknown OS \"$OS\""
    fi
}

# $1=project_name, $2=configuration
build_project() {
    if [ "$OS" = "linux" ]; then
        make -j14 $1 "config=`lowercase $2`"
    elif [ "$OS" = "windows" ]; then
        error "No Windows support"
    elif [ "$OS" = "mac" ]; then
        xcodebuild -project "$PROJ_DIR/$1/$1.xcodeproj" -configuration $2 -quiet
    else
        error "Unknown OS \"$OS\""
    fi
}

# 
compile() {
    xcodebuild -project "$PROJ_DIR/$1" -configuration $2 -quiet
}

