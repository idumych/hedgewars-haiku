#!/bin/sh
# make-sdk.sh: builds a link-time SDK for VitruvianOS 0.6.3 (a BeOS-like OS based on
# Linux), so Hedgewars can be compiled, linked and packaged as a .deb without a
# VitruvianOS machine: the public and private interface headers, libroot, libbe,
# libtranslation, libtracker, libgame, libopengl and libmedia2, and the rc/xres/resattr
# resource tools.
#
#   sh haiku/vos/make-sdk.sh [WORK_DIR]        (default ./.vos-sdk)
#   . WORK_DIR/sdk/env.sh && sh haiku/vos/build-deb.sh
#
# Runs natively on amd64 or arm64 Debian trixie (VitruvianOS's userland, glibc 2.41),
# with the apt packages listed in .github/workflows/vos-deb.yml. None of these libraries
# go into the package: the game uses the ones installed on VitruvianOS. The link proves
# every symbol it needs is exported by 0.6.3. (0.6.2 is not enough: its BSoundPlayer
# never calls the buffer callback, so the game would be silent.)
# Adapted from Folio's tools/vos-sdk/make-sdk.sh (github.com/idumych/folio).
set -eu

VOS_TAG=0.6.3
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mkdir -p "${1:-.vos-sdk}" && cd "${1:-.vos-sdk}" && pwd)
SRC=$WORK/src
SDK=$WORK/sdk
JOBS=$(nproc 2>/dev/null || echo 4)

case "$(dpkg --print-architecture)" in
	amd64) ARCH=amd64; LD_SO=/lib64/ld-linux-x86-64.so.2 ;;
	arm64) ARCH=arm64; LD_SO=/lib/ld-linux-aarch64.so.1 ;;
	*) echo "make-sdk: unsupported architecture $(dpkg --print-architecture)" >&2; exit 1 ;;
esac

say() { printf '\n== %s\n' "$*"; }

say "VitruvianOS $VOS_TAG source"
mkdir -p "$SRC"
if [ ! -d "$SRC/vos/.git" ]; then
	git clone -q --depth 1 --branch "$VOS_TAG" \
		https://github.com/VitruvianOS/Vitruvian.git "$SRC/vos"
	git -C "$SRC/vos" submodule update --init --depth 1 src/system/kernel/nexus
fi
git -C "$SRC/vos" diff --quiet && git -C "$SRC/vos" apply "$HERE/vos-$VOS_TAG-sdk.patch"
CMAKE_OPTS="-DVOS_DYNAMIC_LINKER=$LD_SO"

say "Buildtools (rc, xres, resattr)"
if [ ! -x "$SRC/vos/buildtools/src/bin/xres" ]; then
	mkdir -p "$SRC/vos/buildtools"
	( cd "$SRC/vos/buildtools"
	  cmake -DBUILDTOOLS_MODE=1 $CMAKE_OPTS .. -GNinja > /dev/null
	  ninja -j"$JOBS" > /dev/null )
fi

say "Kits ($ARCH)"
G=$SRC/vos/generated
mkdir -p "$G"
( cd "$G"
  cmake -DCMAKE_BUILD_TYPE=Release -DBUILDTOOLS_DIR=../buildtools \
	-DHOST_KERNEL="$(uname -r)" -DKERNEL_RELEASE="$(uname -r)" \
	-DVITRUVIAN_TARGET_ARCH=$ARCH $CMAKE_OPTS .. -GNinja > /dev/null
  ninja -j"$JOBS" root be translation tracker textencoding game opengl media2 )

say "SDK in $SDK"
rm -rf "$SDK"; mkdir -p "$SDK/include/vos" "$SDK/lib" "$SDK/bin"
( cd "$SRC/vos/headers" && tar cf - --exclude=./libs --exclude=./tools . ) \
	| tar xf - -C "$SDK/include/vos"
for lib in root be translation tracker textencoding game opengl media2; do
	f=$(find "$G/src" -name "lib$lib.so" -type f | head -1)
	[ -n "$f" ] || { echo "make-sdk: lib$lib.so was not built" >&2; exit 1; }
	cp "$f" "$SDK/lib/"
done
B=$SRC/vos/buildtools/src/bin
cp "$B/rc/rc" "$B/xres" "$B/resattr" "$SDK/bin/"
ls "$SDK/lib" "$SDK/bin"

# vos.pc the way newer VitruvianOS (vos-dev) generates it
python3 - "$SRC/vos" "$SDK" "$ARCH" "$VOS_TAG" <<'EOF'
import re, sys
vos, sdk, arch, vos_tag = sys.argv[1:]
s = open(vos + '/build/headers.cmake').read()
body = s[s.index('PUBLIC_HEADERS'):]
body = body[:body.index(')')]
flags = []
for d in re.findall(r'"([^"]+)"', body):
    d = d.rstrip('/').replace('os/arch/x86_64', 'os/arch/' + ('arm64' if arch == 'arm64' else 'x86_64'))
    f = '-I${includedir}' if d == 'headers' else '-I${includedir}/' + d[len('headers/'):]
    if f not in flags:
        flags.append(f)
flags += ['-I${includedir}/build', '-I${includedir}/build/config_headers',
          '-I${includedir}/config', '-include LinuxBuildCompatibility.h']
open(sdk + '/lib/vos.pc', 'w').write(f"""prefix={sdk}
includedir=${{prefix}}/include/vos
libdir=${{prefix}}/lib

Name: vos
Description: VitruvianOS {vos_tag} link-time SDK ({arch})
Version: {vos_tag}
Libs: -L${{libdir}} -lbe -lroot
Cflags: {' '.join(flags)}
""")
EOF

cat > "$SDK/env.sh" <<EOF
# source me: . $SDK/env.sh
export PATH="$SDK/bin:\$PATH"
export PKG_CONFIG_PATH="$SDK/lib"
export VOS_SDK="$SDK"
export VOS_VERSION=$VOS_TAG
EOF
say "done ($ARCH)"
