# How to put the Pseudo X/Y pad into another Force plugin

Pseudo X/Y pad for akai mpc community plugins, by L'Cronx (see ../LICENSE: use it, credit it). This is the XYZ 0.0.2 guide, moved into the repo.
XYZ 0.0.2 (L'Cronx). A free X/Y touch pad for the Akai Force / MPC OS VST2 plugins. It worked on a Force running 3.9.1
(2026-11-04). The plugin draws the pad itself on a spare hardware plane of the display and reads the touchscreen itself;
X and Y are ordinary host parameters, so automation, Q-Links and the data wheel keep working.

## What is in this folder

| File | What it is |
|---|---|
| `xyz/src/xyz.c`, `xyz-sound/src/xyz.c` | the engine: two parameters x, y (-5..+5), no audio, starts/stops the pad, reports finger moves to the wrapper |
| `common/xyz_pad.c`, `common/xyz_pad.h` | the pad: DRM plane, finding MPC's page in its picture (any screen rotation), evdev touch, drawing, value mapping |
| `common/stb_image.h` | public domain / MIT PNG loader (reads `pad/grid.png`) |
| `common/skin_post.py` | after the skin is generated, paints the two 2x2 marker blocks into the page background |
| `<plugin>/vst.json`, `params.json`, `layout.conf`, `drag.json` (optional, knob drag modes) | the plugin description for mpc-vst-plugins `tools/gen_vst.py` (uid `AnXY`, vendor L'Cronx) |
| `build.sh`, `release.sh` (repo root) | build the ARMv7 `.so` with Zig (no Docker) and package the installer zip: `MPC_VST=/path/to/mpc-vst-plugins ./build.sh xyz` |
| `<plugin>/pad/grid.png`, `<plugin>/pad/layout.txt` | the pad picture (747x747, axes cross at 359.5,388.5, 60 px per unit) and its place on the page |

Needs: python3 + Pillow, a host gcc (for shadow_art), `pip install ziglang`, and the wrapper from
sunskiefer/mpc-vst-plugins branch `lcronx-release-quoting` (commit 3845bea: the #486-#489 fix and the escaped release.py). The repo's `release.sh` runs release.py and checks `sh -n` on both installers.
Package with `tools/release.py` (of mpc-vst-plugins; the repo's `release.sh` calls it) `--so build/arm/xyz.so --skin "build/skin/L'Cronx - VST - XYZ" --extra pad:pad --entry build/pluginlist-entry.xml --version X.Y.Z --about "..." -o dist`.
`xyz_pad.c` contains parts of VADER 0.5.0's `src/overlay.c` (MIT, same author), marked "from VADER" in the file.

## How it works (the facts it stands on, all seen in the Force's own log)

1. **The display is portrait.** On the Force (3.9.1) MPC's picture is 800x1280, XR24. MPC draws its 1280x800 landscape screen
   turned 90 degrees anticlockwise into it: landscape (u, v) is display (x = v, y = 1279 - u). The plugin page sits at landscape
   y 86..714. The code does not assume this: it detects the turn (4 cases) from the markers, so it also works unrotated.
2. **Finding the page.** The page background carries two 2x2 opaque marker blocks at its top-left and bottom-right corners
   (`common/skin_post.py`). Every ~100 ms the plugin reads MPC's picture read-only (GETCRTC -> GETFB2 -> PRIME export -> mmap, closed
   again at once) and looks for the markers; while both are visible the pad is shown, otherwise the plane is hidden. That is also
   how it knows the page is on screen and where.
3. **The pad is a DRM overlay plane.** Same route as VADER 0.5.0: find MPC's own DRM fd among the process's files, pick a spare
   plane that takes ARGB (the Force has plane 38), two dumb buffers, `SETPLANE`. The pad is drawn in landscape, then turned into the
   plane's buffer with fixed strides (`rotate_pad`).
4. **Touch.** `/dev/input/touchscreen0` does not exist on this Force. The device is `/dev/input/event0` "ILI2117 Touchscreen",
   multitouch slots, range 0..2048 on both axes, lying like the display (raw x spans display x, raw y spans display y). MPC does not
   grab it: the plugin thread reads every event too. Raw -> display pixels -> landscape (u, v) -> pad coordinates -> X, Y.
   The first finger that goes down inside the pad owns it until it lifts.
5. **Host side.** The thread writes two volatile floats and sets bits in `changed`. The wrapper calls the weak
   `mpc_engine_changed(inst, key, len)` once per block; it returns "x" then "y" and the wrapper tells MPC, as for a CC.

## Putting a pad into another plugin (steps)

1. Copy `common/xyz_pad.c`, `common/xyz_pad.h`, `common/stb_image.h` into the plugin's `src/`; add `xyz_pad.c` to its source list
   (compile flags as in `build.sh`; **stb_image needs `STBI_NO_THREAD_LOCALS`**, already set in the file, or the .so wants
   `__tls_get_addr`).
2. In the engine state add `volatile float x, y; volatile unsigned changed; unsigned reported; overlay_t pad;`.
   `create(data_dir)`: `pad_start(&s->pad, data_dir, &s->x, &s->y, &s->changed);` `destroy`: `pad_stop(&s->pad);`.
   Copy `mpc_engine_changed` from `xyz.c` (merge it with the plugin's own if it has one). `set_param` for the two keys writes
   `s->x`, `s->y`; `get_param` reads them. Use the parameter keys of the pad in `pad_set` calls (they are the floats, not names).
3. Parameters: two params, `min -5, max 5` (or change the range: edit `VAL_MAX` and `pos_to_val`/`val_to_pos` in `xyz_pad.c`;
   for 0..1 use `x01 = (x + 5) / 10`, easiest is to keep -5..5 in the pad and scale in the engine).
4. Data folder: `vst.json` `"defines": {"MODULE_SUBDIR": "\"pad\""}` makes the wrapper pass `<plugin folder>/pad` to `create`. Ship
   `pad/grid.png` and `pad/layout.txt` there with `release.py --extra pad:pad`. The log is `pad/xyz.log`; `pad/pad.off` switches the pad off.
5. Skin: in `layout.conf` leave the pad's rectangle empty (nothing in the way of a finger), and in `vst.json` add
   `"skin_post": "../common/skin_post.py"` (copy the script; an optional `drag.json` next to `vst.json` sets each knob's drag mode, see the README). It writes the markers into **the page background of every page that has a
   pad**; a page without the markers hides the pad automatically. If the plugin's pages use several background images, make
   `skin_post.py` mark only the one with the pad. The page must be the full 1280x628 background (the script asserts it).
6. Placement is the user's choice: the pad's x, y, w, h (page pixels) are the single line `pad x y w h` in `pad/layout.txt`.
7. Your own picture: replace `pad/grid.png`. If it is not the 747x747 grid, change `GRID_N`, `GRID_OX`, `GRID_OY`, `GRID_U` at the top
   of the "new in XYZ" section (centre of the axes and pixels per unit) so the value maps to the drawing.
8. Test on the Force: install, remove and re-insert the plugin, touch the pad, read `pad/xyz.log`. Lines to look for:
   `XYZ's page found: ... turned 90 degrees anticlockwise`, `shown: plane at ...`, `touch down: ... INSIDE`.

## Known limits and notes

- One pad per plugin instance, and one instance per process owns the plane (others keep their knobs). Two pads in one plugin
  needs the single `pad_x/pad_y/bw/bh` generalised to a list and a plane each.
- If MPC is not showing the page the plane is hidden (the page search runs twice a second while it is not found).
- Not yet confirmed by the user: all four quadrants out to the +-5 edges, and Q-Link / automation recording of X and Y.
- The same two faults are in VADER 0.5.0's `src/overlay.c` as committed (not fixed there): a shadowed variable `n` in
  `find_card_fds` (undefined behaviour; the compiler dropped the thread body in 0.0.1), and a page finder that only knows an
  unrotated picture (it never finds the box on the Force's portrait display). `xyz_pad.c` has both fixed.
- 0.0.1 failed on the Force for those two reasons. 0.0.2 is the first version that draws and answers touch.
