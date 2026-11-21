#!/bin/sh
# Builds hedgewars-<version>-<rev>-x86_64.hpkg. Run on Haiku from the top of the tree:
#   sh haiku/package/build-hpkg.sh            -> haiku/build/hedgewars-*.hpkg
set -eu
VERSION=1.1.0~dev
REVISION=1
ARCH=$(getarch)
STAGE=haiku/build/stage
OUT=haiku/build/hedgewars-$VERSION-$REVISION-$ARCH.hpkg

make -f haiku/Makefile DATADIR=/boot/system/data/hedgewars

rm -rf "$STAGE" "$OUT"
mkdir -p "$STAGE/apps/Hedgewars" "$STAGE/data/hedgewars" "$STAGE/data/deskbar/menu/Applications"
cp -a haiku/build/Hedgewars/Hedgewars haiku/build/Hedgewars/hwengine haiku/build/Hedgewars/lib \
	"$STAGE/apps/Hedgewars/"
# the game data, minus the build files that live among it
tar -C share/hedgewars --exclude=CMakeLists.txt --exclude='*.in' -cf - Data \
	| tar -C "$STAGE/data/hedgewars" -xf -
ln -s ../../../../apps/Hedgewars/Hedgewars "$STAGE/data/deskbar/menu/Applications/Hedgewars"

cat > "$STAGE/.PackageInfo" <<EOF
name			hedgewars
version			$VERSION-$REVISION
architecture	$ARCH
summary			"Turn-based artillery game with hedgehogs"
description		"Hedgewars is a turn-based strategy, artillery, action and comedy game: \
teams of hedgehogs fight with bazookas, grenades, air strikes and stranger weapons \
on destructible maps. This is a native Haiku port: the engine runs on the Haiku API \
without SDL, and a native launcher sets up local games against the computer or a \
second player on the same computer."
packager		"Ian <idumych@users.noreply.github.com>"
vendor			"Hedgewars project"
licenses		{ "GNU GPL v2" }
copyrights		{ "2004-2026 Andrey Korotaev and the Hedgewars contributors" }
urls			{ "https://hedgewars.org" }
provides		{
	hedgewars = $VERSION-$REVISION
	app:Hedgewars = $VERSION-$REVISION
}
requires		{
	haiku
	lib:libGL
	lib:libcrypto
	lib:libcurl
	lib:libstdc++
}
EOF

package create -C "$STAGE" "$OUT"
ls -la "$OUT"
