#!/usr/bin/env bash
# Pseudo X/Y pad (L'Cronx): package a built plugin as the installable zip (install.sh, uninstall.sh, ARMv7 build) with the escaped release.py.
#   MPC_VST=/path/to/mpc-vst-plugins ./release.sh xyz 0.1.0
set -euo pipefail
P="${1:?usage: ./release.sh xyz|xyz-sound <version>}"; VER="${2:?version}"
HERE="$(cd "$(dirname "$0")" && pwd)"; MPC_VST="${MPC_VST:?set MPC_VST}"
cd "$HERE/$P"
NAME="$(python3 -c "import json;print(\"L'Cronx - VST - \"+json.load(open('vst.json'))['name'])")"
SO="$(python3 -c "import json;print(json.load(open('vst.json'))['so'])")"
case "$P" in xyz) ABOUT="XYZ: a free X/Y touch pad for MPC OS plugins. Two parameters, X and Y (-5 to +5), automatable, Q-Link mappable. No sound.";;
  *) ABOUT="XYZ-SOUND: the X/Y pad plus a test sine through a low-pass filter. Pad X = cutoff, pad Y = resonance, a circular-drag knob = pitch, a volume knob.";; esac
mkdir -p ../dist
python3 "$MPC_VST/tools/release.py" --so "build/arm/$SO" --skin "build/skin/$NAME" --extra pad:pad --entry build/pluginlist-entry.xml \
  --version "$VER" --about "$ABOUT" -o ../dist
# the apostrophe in L'Cronx must not break the installers
D="$(mktemp -d)"; PN="$(python3 -c "import json;print(json.load(open('vst.json'))['name'])")"
unzip -q "../dist/$PN-$VER-mpc-armv7.zip" -d "$D"
for f in "$D"/*/install.sh "$D"/*/uninstall.sh; do sh -n "$f"; done && echo "sh -n: install.sh and uninstall.sh OK"
rm -rf "$D"
