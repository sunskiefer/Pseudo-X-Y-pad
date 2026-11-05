#!/usr/bin/env bash
# Pseudo X/Y pad (L'Cronx): build one plugin for MPC OS / Force (armv7-a hard-float, glibc <= 2.17 symbols; the Force is ARM, nothing else is built).
#   MPC_VST=/path/to/mpc-vst-plugins ./build.sh xyz          (the plain pad, no sound)
#   MPC_VST=/path/to/mpc-vst-plugins ./build.sh xyz-sound    (pad + sine + low-pass filter + volume)
# Needs: python3 + Pillow, a host gcc (only for the skin tool shadow_art), Zig (pip install ziglang). No Docker.
# MPC_VST = a checkout of mpc-vst-plugins (github.com/sd88me/mpc-vst-plugins; the L'Cronx builds use sunskiefer/mpc-vst-plugins branch
# lcronx-release-quoting @ 3845bea, which has the Q-Link/automation fix #486-#489 and the apostrophe-safe release.py).
# Output: <plugin>/build/package/L'Cronx - VST - <NAME>/   then ./release.sh <plugin> <version> makes the installable zip.
set -euo pipefail
P="${1:?usage: ./build.sh xyz|xyz-sound}"
HERE="$(cd "$(dirname "$0")" && pwd)"
MPC_VST="${MPC_VST:?set MPC_VST to your mpc-vst-plugins checkout}"
ZIG="${ZIG:-python3 -m ziglang}"
cd "$HERE/$P"
NAME="$(python3 -c "import json;print(\"L'Cronx - VST - \"+json.load(open('vst.json'))['name'])")"
SO="$(python3 -c "import json;print(json.load(open('vst.json'))['so'])")"
mkdir -p build/tools build/arm
[ -x build/tools/shadow_art ] || gcc -O2 -w -I"$MPC_VST/tools/vendor/force-shadow/tools" -x c -o build/tools/shadow_art "$MPC_VST/tools/shadow_art.c" -lm
rm -rf build/skin
SHADOW_ART="$PWD/build/tools/shadow_art" python3 "$MPC_VST/tools/gen_vst.py" vst.json
[ -f build/params.h ] && [ -d "build/skin/$NAME/Plugin Skins" ]
TGT="-target arm-linux-gnueabihf.2.31 -mcpu=generic+v7a+vfp3d16-d32-neon+thumb2 -ffp-contract=off"
COMMON="-O2 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DNDEBUG -w -std=gnu11"
OBJS=""
for f in src/xyz.c ../common/xyz_pad.c "$MPC_VST/wrapper/vst2_wrap.c"; do
  o="build/arm/$(basename "$f").o"
  $ZIG cc $TGT $COMMON -Ibuild -Isrc -I../common -I"$MPC_VST/wrapper" -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
printf '{\n  global: VSTPluginMain;\n  local: *;\n};\n' > build/arm/exports.map
$ZIG cc $TGT -shared -fPIC -Wl,--no-undefined -Wl,--version-script=build/arm/exports.map -Wl,--gc-sections -Wl,-s \
  $OBJS -lm -ldl -lpthread -o "build/arm/$SO"
# the Force's glibc is older than the build target: refuse any symbol version newer than 2.17
MAXV="$(readelf -V "build/arm/$SO" | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -1)"
if [ "$(printf '%s\n2.17\n' "$MAXV" | sort -V | tail -1)" != "2.17" ]; then echo "ERROR: $SO needs glibc $MAXV (> 2.17)"; exit 1; fi
echo "highest glibc symbol: $MAXV"
PKG="build/package/$NAME"
rm -rf build/package && mkdir -p "$PKG"
cp -r "build/skin/$NAME/." "$PKG/"
cp "build/arm/$SO" "$PKG/"
mkdir -p "$PKG/pad" && cp pad/grid.png pad/layout.txt "$PKG/pad/"
echo "-> $PKG"
