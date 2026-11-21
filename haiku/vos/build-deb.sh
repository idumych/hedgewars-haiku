#!/bin/sh
# build-deb.sh: builds hedgewars_<version>-<revision>_<arch>.deb for VitruvianOS (a BeOS-like
# OS based on Linux). From the top of the tree, on Debian trixie with the SDK's env.sh:
#
#   . <sdk>/env.sh && sh haiku/vos/build-deb.sh
#
# Layout as on Haiku: /system/apps/Hedgewars/{Hedgewars,hwengine,lib/}, the game data in
# /system/data/hedgewars/Data, a Deskbar entry and /usr/bin/hedgewars.
set -eu
cd "$(dirname "$0")/../.."
VERSION=1.1.0~dev
REVISION=1
ARCH=$(dpkg --print-architecture)
# Built against VitruvianOS 0.6.x; the Haiku API is C++ and V\OS is pre-1.0, so class
# layouts may change in 0.7: require a rebuild then.
VOS_DEPENDS="vos (>= 0.6.2), vos (<< 0.7~)"
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || date +%s)}"

for tool in rc xres resattr dpkg-deb dpkg-shlibdeps tar pkg-config; do
	command -v "$tool" >/dev/null 2>&1 || {
		echo "build-deb: '$tool' not found (source the SDK's env.sh?)" >&2; exit 1; }
done
pkg-config --exists vos || { echo "build-deb: no 'pkg-config vos' (source the SDK's env.sh)" >&2; exit 1; }

# 1. Build. Strip the engine and libraries; not the launcher, whose resources xres
#    appended after its ELF data.
make -f haiku/Makefile clean >/dev/null
make -f haiku/Makefile DATADIR=/system/data/hedgewars -j"$(nproc)"

BUILD=haiku/build/deb
PKG="$BUILD/hedgewars_${VERSION}-${REVISION}_${ARCH}"
OUT="hedgewars_${VERSION}-${REVISION}_${ARCH}.deb"
APPDIR="$PKG/system/apps/Hedgewars"

# 2. Package tree
rm -rf "$BUILD"
mkdir -p "$PKG/DEBIAN" "$APPDIR" "$PKG/system/data/hedgewars" \
	"$PKG/system/data/deskbar/menu/Applications" "$PKG/usr/bin" \
	"$PKG/usr/share/hedgewars" "$PKG/usr/share/doc/hedgewars"
cp -a haiku/build/Hedgewars/Hedgewars haiku/build/Hedgewars/hwengine haiku/build/Hedgewars/lib "$APPDIR/"
strip --strip-unneeded "$APPDIR/hwengine" "$APPDIR"/lib/*.so 2>/dev/null || true
tar -C share/hedgewars --exclude=CMakeLists.txt --exclude='*.in' -cf - Data \
	| tar -C "$PKG/system/data/hedgewars" -xf -
ln -s /system/apps/Hedgewars/Hedgewars "$PKG/system/data/deskbar/menu/Applications/Hedgewars"
ln -s /system/apps/Hedgewars/Hedgewars "$PKG/usr/bin/hedgewars"

# 3. The launcher's BeOS attributes (signature, flags, version). dpkg drops xattrs, so they
#    ship on an EMPTY placeholder in a tarball and postinst copies them onto the launcher.
STAGE="$BUILD/attrs"
mkdir -p "$STAGE/system/apps/Hedgewars"
: > "$STAGE/system/apps/Hedgewars/Hedgewars"
# absolute paths: V\OS's tools don't resolve relative ones on Linux
resattr -O -o "$(pwd)/$STAGE/system/apps/Hedgewars/Hedgewars" "$(pwd)/haiku/build/obj/Hedgewars.rsrc"
TYPE_HEX=534d494d$(printf '%s' application/x-vnd.Be-elfexecutable | od -A n -t x1 | tr -d ' \n')00
python3 -c 'import os,sys; os.setxattr(sys.argv[1], "user.beos.BEOS:TYPE", bytes.fromhex(sys.argv[2]))' \
	"$STAGE/system/apps/Hedgewars/Hedgewars" "$TYPE_HEX"
tar --xattrs --xattrs-include='user.*' --owner=0 --group=0 --numeric-owner \
	--mtime="@$SOURCE_DATE_EPOCH" -cf "$PKG/usr/share/hedgewars/apps_attrs.tar" \
	-C "$STAGE" ./system/apps/Hedgewars/Hedgewars

# 4. Docs
cp COPYING "$PKG/usr/share/doc/hedgewars/copyright"
gzip -9n -c haiku/README.md > "$PKG/usr/share/doc/hedgewars/README.md.gz"

# 5. Dependencies of the binaries on Debian packages; VitruvianOS's own libraries (libbe,
#    libgame, ...) and the game's lib/ have no Debian package and are left out.
SHL=$BUILD/shlibs
mkdir -p "$SHL/debian"
printf 'Source: hedgewars\n\nPackage: hedgewars\nArchitecture: any\n' > "$SHL/debian/control"
ABS=$(pwd)
SHLIBS=$( cd "$SHL" && dpkg-shlibdeps --ignore-missing-info -O \
	-l"$ABS/$APPDIR/lib" -l"$(pkg-config --variable=libdir vos)" \
	"$ABS/$APPDIR/Hedgewars" "$ABS/$APPDIR/hwengine" "$ABS/$APPDIR"/lib/*.so 2>/dev/null \
	| sed -n 's/^shlibs:Depends=//p' )
[ -n "$SHLIBS" ] || { echo "build-deb: dpkg-shlibdeps found no dependencies" >&2; exit 1; }

# 6. Control files
sed -e 's|@APP@|/system/apps/Hedgewars/Hedgewars|g' -e 's|@ATTRS@|/usr/share/hedgewars/apps_attrs.tar|g' \
	-e 's|@REL@|system/apps/Hedgewars/Hedgewars|g' haiku/vos/postinst.in > "$PKG/DEBIAN/postinst"
chmod 0755 "$PKG/DEBIAN/postinst"
SIZE=$(du -sk --apparent-size --exclude=DEBIAN "$PKG" | cut -f1)
cat > "$PKG/DEBIAN/control" <<EOF
Package: hedgewars
Version: ${VERSION}-${REVISION}
Architecture: ${ARCH}
Maintainer: Ian <idumych@users.noreply.github.com>
Installed-Size: ${SIZE}
Depends: ${VOS_DEPENDS}, ${SHLIBS}, attr
Section: games
Priority: optional
Homepage: https://github.com/idumych/hedgewars-haiku
Description: Turn-based artillery game with hedgehogs
 Hedgewars is a turn-based strategy, artillery, action and comedy game: teams
 of hedgehogs fight with bazookas, grenades, air strikes and stranger weapons
 on destructible maps.
 .
 This is the native port for VitruvianOS, a BeOS-like OS based on Linux: the
 engine runs on the Haiku API without SDL, with a native launcher for local
 and network games, missions, campaigns, videos and add-ons.
EOF
( cd "$PKG" && find . -type f ! -path './DEBIAN/*' -printf '%P\0' \
	| sort -z | xargs -0 md5sum ) > "$PKG/DEBIAN/md5sums"
find "$PKG" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
chmod -R u+rwX,go+rX,go-w "$PKG"

dpkg-deb --root-owner-group -Zxz --build "$PKG" "$OUT"
echo "Built $OUT"
