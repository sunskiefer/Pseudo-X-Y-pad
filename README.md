# Pseudo X/Y pad for akai mpc community plugins

**By L'Cronx.** A free X/Y touch pad that any Akai Force / MPC OS community plugin (VST2 `.so`) can have, drawn and read by the plugin itself.
Use it, change it, put it in your own plugins. **You must credit the author**, see [Licence and credit](#licence-and-credit).

The pad is a picture of a grid (axes −5 … +5, all four quadrants) with a dot under your finger. X and Y are ordinary host parameters, so
**automation, Q-Link mapping and the data wheel keep working** on them.

This repo holds two plugins built on it, and the pad code itself:

| Plugin | What it is |
|---|---|
| **XYZ** (`xyz/`) | the plain pad. Two parameters, X and Y (−5 … +5). No sound. Two knobs, X and Y, mirror the pad. |
| **XYZ-SOUND** (`xyz-sound/`) | the same pad plus a test sound: a sine through a resonant low-pass filter. Pad X = **CUTOFF**, pad Y = **RES**, a **PITCH** knob that uses circular drag, and a **VOLUME** knob. |

Installable zips (ARMv7, with `install.sh` and `uninstall.sh`) are on the [Releases](../../releases) page.

## Status, honestly

Tested by the author on an Akai Force running MPC OS 3.9.1:

- **XYZ** (this is XYZ 0.0.2's code, unchanged): the pad draws, follows the finger and moves X and Y. Worked.
- **XYZ-SOUND** without the volume knob (XYZ 0.0.3): reported by the author as working as it should: the pad, the sine and the filter, and the knobs with their three drag modes. (Not measured; the author tried it by hand.)
- **XYZ-SOUND 0.1.0 has not been run on a Force yet.** What is new in it: the **VOLUME** knob, and it is now a separate plugin (name `XYZ-SOUND`, its own id, file `xyz_sound.so`), so both can be installed side by side.
- Both 0.1.0 zips were built, checked with `sh -n` and unpacked here, but ARM code can't be run outside the Force.

Known gaps:

- The knobs showed **no number** in the author's test (the readouts are written, but did not appear). Cause not found yet. So it is not confirmed that the pad edges reach exactly ±5, or 100 %.
- Not confirmed yet: the dot reaching all four edges, and Q-Link / automation *recording* of X and Y.
- One pad per plugin instance, and one instance per process owns the pad. The pad needs MPC OS 3.x.
- The pad shows only while its plugin page is on screen. Until then it is hidden by design.

## Install on a Force

Unzip the release zip on your computer, then copy the unzipped folder to the Force and run the installer there (as root). From Windows PowerShell:

```
scp -r "XYZ-0.1.0" root@<force-ip>:/tmp/
ssh root@<force-ip> "sh /tmp/XYZ-0.1.0/uninstall.sh -y && sh /tmp/XYZ-0.1.0/install.sh -y && reboot"
```

(use `XYZ-SOUND-0.1.0` for the sound plugin). The installer stops MPC, backs up `MPC.settings`, and restarts MPC. Then add the plugin to a track.
If something is wrong, the pad writes a log: `<plugin folder>/pad/xyz.log`. Create an empty file `pad/pad.off` to switch the pad off.

## How it works

**Why a plugin has to draw and read the touch itself.** MPC OS (the Force's software) is one JUCE program. A plugin's screen is not code; it is a *skin* (`TUI.json`)
that MPC draws: pictures, labels, knobs (film strips) and buttons. A skin has **no canvas** and no way to receive a finger's x/y position: the only
touch actions a skin knows are Mouse Down, Double Click, Enter Pressed and Focus Lost, and a knob only turns by dragging along one line.
MPC reads the touchscreen itself (through libinput) and keeps that to itself: the plugin never gets coordinates. (This is from reading the Force 3.9.0 update image and from the
Force's own log on 3.9.1; the author's analysis is still ongoing.) So a free X/Y control cannot be made from a skin alone.

**So the plugin does it, in three steps:**

1. **Draws the pad on its own display layer.** The Force's display has spare hardware overlay planes. The plugin finds MPC's own DRM handle among the process's files, picks a spare plane that takes alpha (plane 38 on the author's Force), and puts the pad picture on it at the place set in `pad/layout.txt`. The route is the one VADER 0.5.0 uses.
2. **Finds its own page.** The Force's display is portrait (800 × 1280) and MPC draws its 1280 × 800 landscape screen turned 90° anticlockwise into it. The skin's background carries two 2 × 2 pixel marker blocks of colours no other page uses (written by `common/skin_post.py`). Every ~100 ms the plugin reads MPC's picture (read-only), looks for the markers in any of the four turns, and shows the pad only while both are visible. That is also how it knows where the page is.
3. **Reads the touchscreen itself.** On the author's Force the touch device is `/dev/input/event0` ("ILI2117 Touchscreen", multitouch, range 0 … 2048 on both axes). MPC does not grab it, so the plugin's own thread receives every event too. A finger that goes down inside the pad owns the dot until it lifts; raw touch → display pixels → page → pad → X, Y.

X and Y are written as plain floats; the wrapper is told about each change (the weak `mpc_engine_changed` hook of mpc-vst-plugins), and tells MPC as if a knob had moved. That's why Q-Links, automation and the data wheel work.

More detail, and every fact it stands on: [docs/INTEGRATION.md](docs/INTEGRATION.md).

## Drag modes, and the circular drag (research, still going)

Every skin in the stock firmware uses vertical-only knobs (`dragOrientation: "Vertical"`). MPC's skin parser also lists the words **`Circular`**, **`Horizontal`** and
**`HorizontalVertical`**. These came from reading MPC's string table; the author *inferred* they are drag modes for `Knob` (like JUCE's rotary drag styles). Nothing documents them.

XYZ-SOUND is the author's test of that inference: each knob has its own copy of the knob definition with its own `dragOrientation`
(`xyz-sound/drag.json`, applied by `common/skin_post.py`):

| Knob | Drag mode |
|---|---|
| CUTOFF | `Vertical` |
| RES | `Horizontal` |
| PITCH | `Circular` |
| VOLUME | `Vertical` |

On the author's Force the three modes worked in the test build. What is **still under research**: how MPC turns a circular drag into a value (the angle around which point, its sensitivity,
the direction, wrap-around), how it behaves with a film strip versus other knob types (`RotatingImage`, `AbsoluteRotaryKnob` are also in the string table), what `HorizontalVertical` does,
and whether a version of MPC OS treats these values differently. Treat this as findings from one device, not a spec. Reports from other Forces / MPC OS versions are welcome as issues.

## Build

You need `python3` with Pillow, a host `gcc` (only for the skin tool), Zig (`pip install ziglang`, no Docker), and a checkout of
[mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins). The L'Cronx builds use the `lcronx-release-quoting` branch of `sunskiefer/mpc-vst-plugins`
at commit `3845bea` (it carries the Q-Link / automation display fix and a `release.py` that handles the apostrophe in "L'Cronx" safely).

```
export MPC_VST=/path/to/mpc-vst-plugins
./build.sh xyz            # or: ./build.sh xyz-sound
./release.sh xyz 0.1.0    # packages dist/XYZ-0.1.0-mpc-armv7.zip and checks the installers with sh -n
```

`build.sh` cross-compiles for ARMv7 hard-float (only the Force's binary is built), and stops if the `.so` needs a glibc symbol newer than 2.17 (the Force's is older than the
toolchain's target; `powf` is one trap, the sound engine has its own `2^x`). `stb_image` is built with `STBI_NO_THREAD_LOCALS` for the same reason.

## Put the pad into your own plugin

Short version: copy `common/xyz_pad.c`, `xyz_pad.h`, `stb_image.h` and `skin_post.py`, call `pad_start(...)` / `pad_stop(...)` from your engine, report changes from
`mpc_engine_changed`, ship `pad/grid.png` + `pad/layout.txt`. The 8 steps, the limits and the traps are in [docs/INTEGRATION.md](docs/INTEGRATION.md). Please credit as below.

## Layout of the repo

```
common/     the pad (xyz_pad.c/.h), stb_image.h, skin_post.py (page markers + drag modes)
xyz/        the plain pad plugin: src/xyz.c, vst.json, params.json, layout.conf, pad/
xyz-sound/  the sound plugin: same, plus drag.json
docs/       INTEGRATION.md
build.sh, release.sh
```

## Licence and credit

MIT, with one extra condition: **credit the author** wherever your users can see it (README, user guide or about text):

> Pseudo X/Y pad for akai mpc community plugins, by L'Cronx (https://github.com/sunskiefer/Pseudo-X-Y-pad)

Full text in [LICENSE](LICENSE). Third-party parts (stb_image, VADER's overlay code, the mpc-vst-plugins wrapper compiled into the binaries) keep their licences: [NOTICE.md](NOTICE.md).
Tips for the free plugins: ko-fi.com/lcronx
