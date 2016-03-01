#!/usr/bin/env bash

#############################
# CONFIGURATION
#############################

BUILD_TARGET=${1:-none}

## Do we want to create a final archive
ARCHIVE_FOR_DISTRIBUTION=1

#############################
# Actual build script
#############################

if [ "$BUILD_TARGET" = "none" ]; then
	echo "You need to specify a build target with:"
	echo "$0 debian_i386"
	echo "$0 debian_amd64"
	exit 0
fi

MAKE=make

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

TAR=tar

FREECAD_MAJOR_VERSION=`grep "set(PACKAGE_VERSION_MAJOR" ../CMakeLists.txt | cut -d \" -f 2`
FREECAD_MINOR_VERSION=`grep "set(PACKAGE_VERSION_MINOR" ../CMakeLists.txt | cut -d \" -f 2`
FREECAD_PATCH_VERSION=`grep "set(PACKAGE_VERSION_PATCH" ../CMakeLists.txt | cut -d \" -f 2`

FULL_VERSION=${FREECAD_MAJOR_VERSION}.${FREECAD_MINOR_VERSION}.${FREECAD_PATCH_VERSION}

echo "Trying to build FreeCAD $FULL_VERSION "
echo $FULL_VERSION > BUILD_VERSION

#############################
# Debian 32bit .deb
#############################
if [[ "$BUILD_TARGET" = "debian_i386" || "$BUILD_TARGET" = "debian_amd64" ]]; then
	BUILD_DIR="$SCRIPT_DIR/build"
	TARGET_DIR="$SCRIPT_DIR/target"
	BUILD_ARCH="Unknown"
	if [ "$BUILD_TARGET" = "debian_i386" ]; then
		BUILD_ARCH="i386"
	else
		BUILD_ARCH="amd64"
	fi

	echo "Building FreeCAD in $BUILD_DIR"
#	rm -Rf $BUILD_DIR
	mkdir -p $BUILD_DIR
	cd $BUILD_DIR
	cmake 	-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_INSTALL_DATADIR=/usr/shared/freecad/data \
		-DCMAKE_INSTALL_DOCDIR=/usr/doc \
		-DCMAKE_INSTALL_INCLUDEDIR=/usr/include/freecad \
		-DCMAKE_INSTALL_LIBDIR=/usr/lib/freecad \
		 ../..
	if [ $? != 0 ]; then echo "Failed to configure FreeCAD"; exit 1; fi
	$MAKE -j3
	if [ $? != 0 ]; then echo "Failed to Make FreeCAD"; exit 1; fi
	echo "Installing FreeCAD to  $TARGET_DIR"
	rm -Rf $TARGET_DIR
	mkdir -p $TARGET_DIR
	$MAKE DESTDIR=$TARGET_DIR install
	if [ $? != 0 ]; then echo "Failed to Install FreeCAD"; exit 1; fi
	cd $SCRIPT_DIR
	# Debian package directory should reside inside the target directory
	mkdir -p ${TARGET_DIR}/DEBIAN
	cat debian_control | sed "s/\[BUILD_VERSION\]/${FULL_VERSION}/" | sed "s/\[ARCH\]/${BUILD_ARCH}/" > ${TARGET_DIR}/DEBIAN/control

#	chmod 755 scripts/linux/${TARGET_DIR}/usr -R
# 	
	fakeroot sh -ec "
		chown root:root ${TARGET_DIR} -R
		chmod 755 ${TARGET_DIR}/DEBIAN -R
		dpkg-deb -Zgzip --build ${TARGET_DIR} ${SCRIPT_DIR}/freecad_${FULL_VERSION}_${BUILD_ARCH}.deb
		chown `id -un`:`id -gn` ${TARGET_DIR} -R
	"

	exit
fi
