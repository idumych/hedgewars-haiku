#!/bin/sh
# Converts the Pascal engine (hedgewars/*.pas) to C in haiku/engine-c/ with
# upstream's pas2c, as CMake's BUILD_ENGINE_C does. Run on a system with GHC
# (pas2c is Haskell; Haiku has no GHC), from the top of the tree:
#   sh haiku/tools/gen-engine-c.sh
# then commit haiku/engine-c. The Pascal stays the source: edit it, rerun this.
set -eu
OUT=haiku/engine-c
WORK=${TMPDIR:-/tmp}/hw-pas2c
# Arch's GHC ships only shared libraries; other systems may want GHC_FLAGS=""
GHC_FLAGS=${GHC_FLAGS--dynamic}

mkdir -p "$WORK"
ghc $GHC_FLAGS -O2 --make tools/pas2c/Main.hs -itools/pas2c -outputdir "$WORK" -o "$WORK/pas2c" \
	> "$WORK/ghc.log" 2>&1 || { cat "$WORK/ghc.log"; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT"
# the same values as haiku/Makefile's config.inc
sed -e 's|${HEDGEWARS_PROTO_VER}|60|' -e 's|${HEDGEWARS_VERSION}|1.1.0-dev|' \
	-e 's|${HEDGEWARS_REVISION}|haiku|' -e 's|${HEDGEWARS_HASH}|haiku|' \
	-e 's|${HEDGEWARS_FULL_DATADIR}|/boot/system/data/hedgewars|' \
	-e 's|${FONTS_DIRS_ARRAY}|array [0..1] of PChar = (nil, nil);|' \
	hedgewars/config.inc.in > "$OUT/config.inc"
"$WORK/pas2c" -n hwengine -i hedgewars -o "$OUT" -a "$OUT" \
	-d ENDIAN_LITTLE -d DEBUGFILE -d HAIKU > "$WORK/pas2c.log" || { cat "$WORK/pas2c.log"; exit 1; }
rm -f "$OUT/config.inc" preprocess.out
echo "$(ls "$OUT"/*.c | wc -l) C files in $OUT"
