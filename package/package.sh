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
# FREECAD_PATCH_VERSION=`grep "set(PACKAGE_VERSION_PATCH" ../CMakeLists.txt | cut -d \" -f 2`

# Actually for PATCH version the following is implemented:
FREECAD_PATCH_VERSION=`git rev-list HEAD | wc -l | sed -e 's/ *//g' | xargs -n1 printf %04d`

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
	cmake 	-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr/lib/freecad \
		-DCMAKE_INSTALL_DATADIR=/usr/lib/freecad/data \
		-DCMAKE_INSTALL_DOCDIR=/usr/doc \
		-DCMAKE_INSTALL_INCLUDEDIR=/usr/include/freecad \
		-DCMAKE_INSTALL_LIBDIR=/usr/lib/freecad/lib \
		 ../..
	if [ $? != 0 ]; then echo "Failed to configure FreeCAD"; exit 1; fi
	$MAKE -j3
	if [ $? != 0 ]; then echo "Failed to Make FreeCAD"; exit 1; fi

	echo "Installing FreeCAD to  $TARGET_DIR"
	rm -Rf $TARGET_DIR
	mkdir -p $TARGET_DIR
# Installing
	$MAKE DESTDIR=$TARGET_DIR install
	if [ $? != 0 ]; then echo "Failed to Install FreeCAD"; exit 1; fi
	cd $SCRIPT_DIR
# Additional Debian-specific stuff: share directory
	mkdir -p ${TARGET_DIR}/usr
	mkdir -p ${TARGET_DIR}/usr/share
	
	mkdir -p ${TARGET_DIR}/usr/share/applications
	cp debian/freecad.desktop  ${TARGET_DIR}/usr/share/applications/

	# doc
	# doc/freecad
	# changelog.Debian.gz, hangelog.gz copyright

	mkdir -p ${TARGET_DIR}/usr/share/freecad
	ln -s ../../lib/freecad/Mod ${TARGET_DIR}/usr/share/freecad/Mod
	cp ${TARGET_DIR}/usr/lib/freecad/data/freecad.xpm ${TARGET_DIR}/usr/share/freecad/freecad.xpm
#	ln -s ../../lib/freecad/data/freecad.xpm ${TARGET_DIR}/usr/share/freecad/freecad.xpm

	mkdir -p ${TARGET_DIR}/usr/share/lintian
	mkdir -p ${TARGET_DIR}/usr/share/lintian/overrides
	cp debian/freecad.lintian-overrides ${TARGET_DIR}/usr/share/lintian/overrides/freecad

	mkdir -p ${TARGET_DIR}/usr/share/man
	mkdir -p ${TARGET_DIR}/usr/share/man/man1
	gzip -c  debian/freecad.1 >  ${TARGET_DIR}/usr/share/man/man1/freecad.1.gz
	ln -s freecad.1.gz ${TARGET_DIR}/usr/share/man/man1/freecadcmd.1.gz

	mkdir -p ${TARGET_DIR}/usr/share/menu
	mkdir -p ${TARGET_DIR}/usr/share/menu/freecad
	cp debian/menu ${TARGET_DIR}/usr/share/menu/freecad/menu

	mkdir -p ${TARGET_DIR}/usr/share/mime
	mkdir -p ${TARGET_DIR}/usr/share/mime/packages
	cp debian/freecad.sharedmimeinfo ${TARGET_DIR}/usr/share/mime/packages/freecad.xml

	mkdir -p ${TARGET_DIR}/usr/share/python
	mkdir -p ${TARGET_DIR}/usr/share/python/runtime.d
	cp debian/freecad.rtupdate ${TARGET_DIR}/usr/share/python/runtime.d/
	chmod a+x ${TARGET_DIR}/usr/share/python/runtime.d/freecad.rtupdate
# Additional Debian-specific stuff: bin directory:
	mkdir -p ${TARGET_DIR}/usr/bin
	ln -s ../lib/freecad/bin/FreeCAD ${TARGET_DIR}/usr/bin/freecad
	ln -s ../lib/freecad/bin/FreeCADCmd  ${TARGET_DIR}/usr/bin/freecadcmd

# Let's Remove bulcu doc directory for now
	rm -rf  ${TARGET_DIR}/usr/doc
	
# Debian package directory should reside inside the target directory
	mkdir -p ${TARGET_DIR}/DEBIAN
	cat debian/control | sed "s/\[BUILD_VERSION\]/${FULL_VERSION}/" | sed "s/\[ARCH\]/${BUILD_ARCH}/" > ${TARGET_DIR}/DEBIAN/control
	cp debian/postinst ${TARGET_DIR}/DEBIAN/postinst
	cp debian/postrm ${TARGET_DIR}/DEBIAN/postrm
	cp debian/prerm ${TARGET_DIR}/DEBIAN/prerm
# Now that the directory structure is ready, let's build a package
	rm -Rf ${SCRIPT_DIR}/freecad_*.deb
# Let's delete the old builds:
	fakeroot sh -ec "
		chown root:root ${TARGET_DIR} -R
		chmod u+w,a+rX,go-w ${TARGET_DIR} -R
		chmod a+x ${TARGET_DIR}/DEBIAN -R
		dpkg-deb -Zgzip --build ${TARGET_DIR} ${SCRIPT_DIR}/freecad_${FULL_VERSION}_${BUILD_ARCH}.deb
		chown `id -un`:`id -gn` ${TARGET_DIR} -R
	"

	exit
fi
