#!/usr/bin/env bash
# tools/zipme.sh
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#

#set -x

WD=`pwd`
TAR=tar
GPG="gpg -sab"
SHASUM=sha512sum
verbose=0
sign=0

# A list of files and folders to exclude from the final tarball.

EXCLPAT="
  .github
  .asf.yaml

"
# Get command line parameters

USAGE="USAGE: $0 [-d|h|v|s] [-b <build]> [-e <exclude>] [-k <key-id>] [<major.minor.patch>]"
ADVICE="Try '$0 -h' for more information"

unset VERSION
unset VERSIONOPT
unset BUILD
unset DEBUG

while [ ! -z "$1" ]; do
  case $1 in
  -b )
    shift
    BUILD="-b ${1}"
    ;;
  -d )
    set -x
    DEBUG=-d
    ;;
  -e )
    shift
    EXCLPAT+=${1}
    ;;
  -v )
    verbose=1
    ;;
  -s )
    sign=1
    ;;
  -k )
    shift
    GPG+=" --default-key $1"
    ;;
  -h )
    echo "$0 is a tool for generation of release versions of ROUX"
    echo ""
    echo $USAGE
    echo ""
    echo "Where:"
    echo "  -b <build>"
    echo "     Use this build identification string.  Default: use GIT build ID"
    echo "     NOTE: GIT build information may not be available in a snapshot"
    echo "  -d"
    echo "     Enable script debug"
    echo "  -h"
    echo "     show this help message and exit"
    echo "  -e"
    echo "     Exclude a list of files or folders"
    echo "     NOTE: The list must be quoted, example -e \"*.out tmp\""
    echo "  -v"
    echo "     Be verbose. The output could be more than you care to see."
    echo "  -s"
    echo "    PGP sign the final tarballs and create digests."
    echo "  -k"
    echo "    PGP key ID.  If not provided the default ID will be used."
    echo "  <major.minor.patch>"
    echo "     The ROUX version number expressed as a major, minor and patch number separated"
    echo "     by a period"
    exit 0
    ;;
  * )
    break
    ;;
  esac
  shift
done

# The last thing on the command line is the version number

VERSION=$1
if [ -n ${VERSION} ] ; then
  VERSIONOPT="-v ${VERSION}"
fi

# Full tar options

for pat in ${EXCLPAT} ; do
  TAR+=" --exclude=${pat}"
done

TAR+=" --exclude-vcs"

if [ $verbose != 0 ] ; then
  TAR+=" -czvf"
else
  TAR+=" -czf"
fi

# Find the directory we were executed from and were we expect to
# see the directories to tar up

MYNAME=`basename $0`

if [ -x ${WD}/${MYNAME} ] ; then
  TRUNKDIR="${WD}/../.."
else
  if [ -x ${WD}/tools/${MYNAME} ] ; then
    TRUNKDIR="${WD}/.."
  else
    if [ -x ${WD}/roux/tools/${MYNAME} ] ; then
      TRUNKDIR="${WD}"
    else
      echo "You must cd into the ROUX directory to execute this script."
      exit 1
    fi
  fi
fi

# Get the ROUX directory names and the path to the parent directory

ROUXDIR=${TRUNKDIR}/roux
APPSDIR=${TRUNKDIR}/apps

# Make sure that the directories exists

if [ ! -d ${TRUNKDIR} ]; then
  echo "Directory ${TRUNKDIR} does not exist"
  exit 1
fi

cd ${TRUNKDIR} || \
  { echo "Failed to cd to ${TRUNKDIR}" ; exit 1 ; }

if [ ! -d ${ROUXDIR} ] ; then
  echo "Directory ${TRUNKDIR}/${ROUXDIR} does not exist!"
  exit 1
fi

if [ ! -d ${APPSDIR} ] ; then
  echo "Directory ${TRUNKDIR}/${APPSDIR} does not exist!"
  exit 1
fi

# Perform a full clean for the distribution

echo "Cleaning the repositories"

if [ $verbose != 0 ] ; then
  make -C ${ROUXDIR} distclean
else
  make -C ${ROUXDIR} distclean 1>/dev/null
fi

# Prepare the roux directory

# Write a version file into the ROUX directory.  The syntax of file is such that it
# may be sourced by a bash script or included by a Makefile.

VERSIONSH=${ROUXDIR}/tools/version.sh
if [ ! -x "${VERSIONSH}" ]; then
  echo "No executable script was found at: ${VERSIONSH}"
  exit 1
fi

${VERSIONSH} ${DEBUG} ${BUILD} ${VERSIONOPT} ${ROUXDIR}/.version || \
    { echo "${VERSIONSH} failed"; cat ${ROUXDIR}/.version; exit 1; }
chmod 755 ${ROUXDIR}/.version || \
    { echo "'chmod 755 ${ROUXDIR}/.version' failed"; exit 1; }

if [ -z ${VERSION} ] ; then
  source ${ROUXDIR}/.version
  VERSION=${CONFIG_VERSION_STRING}
  VERSIONOPT="-v ${VERSION}"
fi

# Update the configuration variable documentation
#
# MKCONFIGVARS=${ROUXDIR}/tools/mkconfigvars.sh
# CONFIGVARHTML=${ROUXDIR}/Documentation/ROUXConfigVariables.html
#
# if [ ! -x "${MKCONFIGVARS}" ]; then
#     echo "No executable script was found at: ${MKCONFIGVARS}"
#     exit 1
# fi
#
# cd ${ROUXDIR} || \
#    { echo "Failed to cd to ${ROUXDIR}" ; exit 1 ; }
#
# ${MKCONFIGVARS} ${DEBUG} ${VERSIONOPT} || \
#     { echo "${MKCONFIGVARS} failed"; exit 1; }
# chmod 644 ${CONFIGVARHTML} || \
#     { echo "'chmod 644 ${CONFIGVARHTML}' failed"; exit 1; }
#

# Create the versioned tarball names

ROUX_TARNAME=apache-roux-${VERSION}.tar
APPS_TARNAME=apache-roux-apps-${VERSION}.tar
ROUX_ZIPNAME=${ROUX_TARNAME}.gz
APPS_ZIPNAME=${APPS_TARNAME}.gz
ROUX_ASCNAME=${ROUX_ZIPNAME}.asc
APPS_ASCNAME=${APPS_ZIPNAME}.asc
ROUX_SHANAME=${ROUX_ZIPNAME}.sha512
APPS_SHANAME=${APPS_ZIPNAME}.sha512

# Remove any previous tarballs

if [ -f ${ROUX_TARNAME} ] ; then
  echo "Removing ${TRUNKDIR}/${ROUX_TARNAME}"
  rm -f ${ROUX_TARNAME} || \
     { echo "rm ${ROUX_TARNAME} failed!" ; exit 1 ; }
fi

if [ -f ${ROUX_ZIPNAME} ] ; then
  echo "Removing ${TRUNKDIR}/${ROUX_ZIPNAME}"
  rm -f ${ROUX_ZIPNAME} || \
     { echo "rm ${ROUX_ZIPNAME} failed!" ; exit 1 ; }
fi

if [ -f ${APPS_TARNAME} ] ; then
  echo "Removing ${TRUNKDIR}/${APPS_TARNAME}"
  rm -f ${APPS_TARNAME} || \
     { echo "rm ${APPS_TARNAME} failed!" ; exit 1 ; }
fi

if [ -f ${APPS_ZIPNAME} ] ; then
  echo "Removing ${TRUNKDIR}/${APPS_ZIPNAME}"
  rm -f ${APPS_ZIPNAME} || \
     { echo "rm ${APPS_ZIPNAME} failed!" ; exit 1 ; }
fi

# Remove any previous signatures or digests

if [ -f ${ROUX_ASCNAME} ] ; then
  echo "Removing ${TRUNKDIR}/${ROUX_ASCNAME}"
  rm -f ${ROUX_ASCNAME} || \
     { echo "rm ${ROUX_ASCNAME} failed!" ; exit 1; }
fi

if [ -f ${APPS_ASCNAME} ] ; then
  echo "Removing ${TRUNKDIR}/${APPS_ASCNAME}"
  rm -f ${APPS_ASCNAME} || \
     { echo "rm ${APPS_ASCNAME} failed!" ; exit 1; }
fi

if [ -f ${ROUX_SHANAME} ] ; then
  echo "Removing ${TRUNKDIR}/${ROUX_SHANAME}"
  rm -f ${ROUX_SHANAME} || \
     { echo "rm ${ROUX_SHANAME} failed!" ; exit 1; }
fi

if [ -f ${APPS_SHANAME} ] ; then
  echo "Removing ${TRUNKDIR}/${APPS_SHANAME}"
  rm -f ${APPS_SHANAME} || \
     { echo "rm ${APPS_SHANAME} failed!" ; exit 1; }
fi

# Then tar and zip-up the directories

echo "Archiving and zipping roux/"
${TAR} ${ROUX_ZIPNAME} `basename ${ROUXDIR}` || \
      { echo "tar of ${ROUX_ZIPNAME} failed!" ; exit 1 ; }

echo "Archiving and zipping apps/"
${TAR} ${APPS_ZIPNAME} `basename ${APPSDIR}` || \
      { echo "tar of ${APPS_ZIPNAME} failed!" ; exit 1 ; }

# Create the hashes for the two tarballs

echo "Creating the hashes"
${SHASUM} ${ROUX_ZIPNAME} > ${ROUX_SHANAME} || \
         { echo "Digest of ${ROUX_ZIPNAME} failed!" ; exit 1 ; }

${SHASUM} ${APPS_ZIPNAME} > ${APPS_SHANAME} || \
         { echo "Digest of ${APPS_ZIPNAME} failed!" ; exit 1 ; }

# Finally sign the tarballs

if [ $sign != 0 ] ; then
  echo "Signing the tarballs"
  ${GPG} ${ROUX_ZIPNAME} || \
        { echo "Signing ${ROUX_ZIPNAME} failed!" ; exit 1 ; }

  ${GPG} ${APPS_ZIPNAME} || \
        { echo "Signing ${APPS_ZIPNAME} failed!" ; exit 1 ; }
fi

cd ${ROUXDIR}
