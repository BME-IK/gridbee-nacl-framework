#!/bin/csh

# This file is part of BOINC.
# http://boinc.berkeley.edu
# Copyright (C) 2008 University of California
#
# BOINC is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later version.
#
# BOINC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

##
# Release Script for Macintosh BOINC Manager 10/31/07 by Charlie Fenton
## updated 11/18/09 by Charlie Fenton for OS 10.6 Snow Leopard
## updated 9/28/10 by Charlie Fenton for new BOINC skins
## updated 12/2/10 by Charlie Fenton to remove obsolete items
## updated 1/18/11 by Charlie Fenton to remove BOINC skins
## updated 7/26/11 by Charlie Fenton for XCode 4.1 and OS 10.7
##
## NOTE: This script uses PackageMaker, which is installed as part of the 
##   XCode developer tools.  So you must have installed XCode Developer 
##   Tools on the Mac before running this script.
##
## NOTE: PackageMaker may write 3 lines to the terminal with "Setting to : 0 (null)" 
##   and "relocate: (null) 0".  This is normal and does not indicate a problem.
##

## Usage:
## cd to the root directory of the boinc tree, for example:
##     cd [path]/boinc
##
## Invoke this script with the three parts of version number as arguments.  
## For example, if the version is 3.2.1:
##     source [path_to_this_script] 3 2 1
##
## This will create a director "BOINC_Installer" in the parent directory of 
## the current directory
##
## For testing only, you can use the development build by adding a fourth argument -dev
## For example, if the version is 3.2.1:
##     source [path_to_this_script] 3 2 1 -dev

if [ $# -lt 3 ]; then
echo "Usage:"
echo "   cd [path]/boinc"
echo "   source [path_to_this_script] major_version minor_version revision_number"
return 1
fi

#pushd ./
BOINCPath=$PWD

# XCode 3.x and 4.x use different paths for their build products.
# Our scripts in XCode's script build phase write those paths to 
# files to help this release script find the build products.
if [ "$4" = "-dev" ]; then
    exec 7<"mac_build/Build_Development_Dir"
    read -u 7 BUILDPATH
else
    exec 7<"mac_build/Build_Deployment_Dir"
    read -u 7 BUILDPATH
fi

DarwinVersion=`uname -r`;
DarwinMajorVersion=`echo $DarwinVersion | sed 's/\([0-9]*\)[.].*/\1/' `;
# DarwinMinorVersion=`echo $version | sed 's/[0-9]*[.]\([0-9]*\).*/\1/' `;
#
# echo "major = $DarwinMajorVersion"
# echo "minor = $DarwinMinorVersion"
#
# Darwin version 11.x.y corresponds to OS 10.7.x
# Darwin version 10.x.y corresponds to OS 10.6.x
# Darwin version 8.x.y corresponds to OS 10.4.x
# Darwin version 7.x.y corresponds to OS 10.3.x
# Darwin version 6.x corresponds to OS 10.2.x

if [ "$DarwinMajorVersion" = "11" ]; then
    # XCode 4.1 on OS 10.7 builds only Intel binaries
    arch="i686"
else
    # XCode 3.2 on OS 10.6 does sbuild Intel and PowerPC Universal binaries
    arch="universal"
fi

sudo rm -dfR ../BOINC_Installer/Installer\ Resources/
sudo rm -dfR ../BOINC_Installer/Installer\ Scripts/
sudo rm -dfR ../BOINC_Installer/Pkg_Root

mkdir -p ../BOINC_Installer/Installer\ Resources/
mkdir -p ../BOINC_Installer/Installer\ Scripts/

cp -fp mac_Installer/License.rtf ../BOINC_Installer/Installer\ Resources/
cp -fp mac_installer/ReadMe.rtf ../BOINC_Installer/Installer\ Resources/
cp -fp win_build/installerv2/redist/all_projects_list.xml ../BOINC_Installer/Installer\ Resources/

# Update version number
sed -i "" s/"<VER_NUM>"/"$1.$2.$3"/g ../BOINC_Installer/Installer\ Resources/ReadMe.rtf

#### We don't customize BOINC Data directory name for branding
cp -fp mac_installer/preinstall ../BOINC_Installer/Installer\ Scripts/
cp -fp mac_installer/preinstall ../BOINC_Installer/Installer\ Scripts/preupgrade
cp -fp mac_installer/postinstall ../BOINC_Installer/Installer\ Scripts/
cp -fp mac_installer/postupgrade ../BOINC_Installer/Installer\ Scripts/

cp -fpR $BUILDPATH/PostInstall.app ../BOINC_Installer/Installer\ Resources/

mkdir -p ../BOINC_Installer/Pkg_Root
mkdir -p ../BOINC_Installer/Pkg_Root/Applications
mkdir -p ../BOINC_Installer/Pkg_Root/Library
mkdir -p ../BOINC_Installer/Pkg_Root/Library/Screen\ Savers
mkdir -p ../BOINC_Installer/Pkg_Root/Library/Application\ Support
mkdir -p ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data
mkdir -p ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/locale
mkdir -p ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/switcher
mkdir -p ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/skins

cp -fpR $BUILDPATH/WaitPermissions.app ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/

cp -fpR $BUILDPATH/switcher ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/switcher/
cp -fpR $BUILDPATH/setprojectgrp ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/switcher/
## cp -fpR $BUILDPATH/AppStats ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/switcher/

## FOR NOW - Don't install WCG Skins.  If you reinstate this, also reinstate preinstall & preupgrade above
## Copy the World Community Grid skins into the installer tree, minus the CVS files
## cd "${BOINCPath}/clientgui/skins"
## cp -fpR World\ Community\ Grid ../../../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/skins/
## cd "${BOINCPath}"
## sudo rm -dfR ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/skins/World\ Community\ Grid/CVS
## sudo rm -dfR ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/skins/World\ Community\ Grid/graphic/CVS

cp -fp curl/ca-bundle.crt ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/

cp -fp doc/logo/boinc_logo_black.jpg ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/
cp -fp api/txf/Helvetica.txf ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/
cp -fp clientscr/ss_config.xml ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/
cp -fpR $BUILDPATH/boincscr ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/

cp -fpR $BUILDPATH/BOINCManager.app ../BOINC_Installer/Pkg_Root/Applications/

cp -fpR $BUILDPATH/BOINCSaver.saver ../BOINC_Installer/Pkg_Root/Library/Screen\ Savers/

## Copy the localization files into the installer tree
## Old way copies CVS and *.po files which are not needed
## cp -fpR locale/ ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/locale
## sudo rm -dfR ../BOINC_Installer/Pkg_Root/Library/Application\ Support/BOINC\ Data/locale/CVS
## New way copies only *.mo files (adapted from boinc/sea/make-tar.sh)
find locale -name '*.mo' | cut -d '/' -f 2 | awk '{print "\"../BOINC_Installer/Pkg_Root/Library/Application Support/BOINC Data/locale/"$0"\""}' | xargs mkdir -p
find locale -name '*.mo' | cut -d '/' -f 2,3 | awk '{print "cp \"locale/"$0"\" \"../BOINC_Installer/Pkg_Root/Library/Application Support/BOINC Data/locale/"$0"\""}' | bash

## Fix up ownership and permissions
sudo chown -R root:admin ../BOINC_Installer/Pkg_Root/*
sudo chmod -R u+rw,g+rw,o+r-w ../BOINC_Installer/Pkg_Root/*
sudo chmod 1775 ../BOINC_Installer/Pkg_Root/Library

sudo chown -R 501:admin ../BOINC_Installer/Pkg_Root/Library/Application\ Support/*
sudo chmod -R u+rw,g+r-w,o+r-w ../BOINC_Installer/Pkg_Root/Library/Application\ Support/*

sudo chown -R root:admin ../BOINC_Installer/Installer\ Resources/*
sudo chown -R root:admin ../BOINC_Installer/Installer\ Scripts/*
sudo chmod -R u+rw,g+r-w,o+r-w ../BOINC_Installer/Installer\ Resources/*
sudo chmod -R u+rw,g+r-w,o+r-w ../BOINC_Installer/Installer\ Scripts/*

sudo rm -dfR ../BOINC_Installer/New_Release_$1_$2_$3/

mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/
mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch
mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras
mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin
mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_SymbolTables

cp -fp ../BOINC_Installer/Installer\ Resources/ReadMe.rtf ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch
sudo chown -R 501:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/ReadMe.rtf
sudo chmod -R 644 ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/ReadMe.rtf

cp -fp COPYING ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYING.txt
sudo chown -R 501:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYING.txt
sudo chmod -R 644 ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYING.txt

cp -fp COPYING.LESSER ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYING.LESSER.txt
sudo chown -R 501:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYING.LESSER.txt
sudo chmod -R 644 ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYING.LESSER.txt

cp -fp COPYRIGHT ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYRIGHT.txt
sudo chown -R 501:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYRIGHT.txt
sudo chmod -R 644 ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/COPYRIGHT.txt

cp -fpR $BUILDPATH/Uninstall\ BOINC.app ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras
sudo chown -R root:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/Uninstall\ BOINC.app
sudo chmod -R 755 ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/extras/Uninstall\ BOINC.app

# Build the installer package inside the wrapper application's bundle
# OS 10.5 / OS 10.6 packagemaker
/Developer/usr/bin/packagemaker -r ../BOINC_Installer/Pkg_Root -e ../BOINC_Installer/Installer\ Resources/ -s ../BOINC_Installer/Installer\ Scripts/ -f mac_build/Pkg-Info.plist -t "BOINC Manager" -n "$1.$2.$3" -b -o ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/BOINC\ Installer.pkg
# Remove TokenDefinitions.plist and IFPkgPathMappings in Info.plist, which would cause installer to find a previous copy of BOINCManager and install there
sudo rm -f ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/BOINC\ Installer.pkg/Contents/Resources/TokenDefinitions.plist
defaults delete "$BOINCPath/../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/BOINC Installer.pkg/Contents/Info" IFPkgPathMappings
# Add our custom icon
ditto -xk clientgui/res/MacPkgIcon.zip ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/BOINC\ Installer.pkg/
SetFile -a CE "$BOINCPath/../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/BOINC Installer.pkg"
SetFile -a V "$BOINCPath/../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_$arch/BOINC Installer.pkg/"Icon*

# Build the stand-alone client distribution
cp -fpR mac_build/Mac_SA_Insecure.sh ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/
cp -fpR mac_build/Mac_SA_Secure.sh ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/
cp -fpR COPYING ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/COPYING.txt
cp -fpR COPYING.LESSER ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/COPYING.LESSER.txt
cp -fpR COPYRIGHT ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/COPYRIGHT.txt
cp -fp mac_Installer/License.rtf ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/
sudo chown -R 501:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/*
sudo chmod -R 644 ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/*

mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir
cp -fpR $BUILDPATH/boinc ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/
cp -fpR $BUILDPATH/boinccmd ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/
cp -fpR curl/ca-bundle.crt ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/

mkdir -p ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/switcher
cp -fpR $BUILDPATH/switcher ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/switcher/
cp -fpR $BUILDPATH/setprojectgrp ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/switcher/
## cp -fpR $BUILDPATH/AppStats ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/switcher/

sudo chown -R root:admin ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/*
sudo chmod -R u+rw-s,g+r-ws,o+r-w ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_$arch-apple-darwin/move_to_boinc_dir/*

cp -fpR $BUILDPATH/SymbolTables/ ../BOINC_Installer/New_Release_$1_$2_$3/boinc_$1.$2.$3_macOSX_SymbolTables/

cd ../BOINC_Installer/New_Release_$1_$2_$3
## Use ditto instead of zip utility to preserve resource forks and Finder attributes (custom icon, hide extension) 
ditto -ck --sequesterRsrc --keepParent boinc_$1.$2.$3_macOSX_$arch boinc_$1.$2.$3_macOSX_$arch.zip
ditto -ck --sequesterRsrc --keepParent boinc_$1.$2.$3_$arch-apple-darwin boinc_$1.$2.$3_$arch-apple-darwin.zip
ditto -ck --sequesterRsrc --keepParent boinc_$1.$2.$3_macOSX_SymbolTables boinc_$1.$2.$3_macOSX_SymbolTables.zip

#popd
cd "${BOINCPath}"

return 0
