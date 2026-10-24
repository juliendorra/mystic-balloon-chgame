# Continuous 128×128 splash

`original-title.png` is the exact four 32×64 `titleScreen` slices decoded
from the upstream MIT-licensed `bitmaps.h`. `original-title-eyes.png` also
includes the original eye sprites at their original coordinates. These are
references, not the production background.

`continuous-source.png` is the base drawing made with the built-in
image_gen tool using that extraction. The [final prompt](prompt.txt) records
the composition requirements. The child, large balloon, enemy silhouettes
and surrounding flowing shapes form one scene. The miniature hero, coins,
platforms and detached balloons from the previous splash are removed.

`continuous-base.png` is the unretouched indexed import. `eyes.json` is a
direct, hand-drawn pixel correction applied after import; no image generation
is used for this correction. Its two 8-column stencils contain 120 cells
and change 42 pixels. A first round-eyed pass was checked and redrawn into
smaller tilted eyes with a clearer gaze. The source hash prevents a patch
from silently landing on a different drawing.

`polish_splash.py` then refines the entire composition with exact indexed
pixels, polygons, lines and a finger stencil. It removes quantizer shades
and stray skin colors, keeps skin on the child, gives vapor a controlled
mint ramp, resolves the trousers, and draws a large angled balloon with
a taut diagonal string. The balloon deliberately extends beyond the right
edge, restoring the upward pull; the string is not bent around menus.
Its foreground mask separates
the child from the wind; the hand stencil closes the fingers around the
string. This whole-scene refinement uses no image generation.

`continuous-128.png` is the production background: a 128×128 indexed PNG
whose indices have the same semantic meaning as the game palettes. The
source is reduced with nearest-neighbor sampling and quantized without
dithering to the Pastel Island palette. The original title lettering is
added separately by `build_art.py`. The logo matte follows its visible
frame: six blank source rows and its rounded outer corners are transparent.
It no longer covers 12 native rows of illustration below the frame.
Game menu text remains a live overlay. CONF is shifted right five pixels
to keep the child's face clear; its bordered opaque card occludes the
diagonal string cleanly instead of distorting the illustration to fit UI.
The old eye animation is no longer stamped at coordinates belonging to
the previous illustration.
The cartridge menu reserves five palette slots for its own UI, so its
picture can use up to eleven colors. This eight-color drawing now fits
exactly, so the cartridge picture matches the game art without merging shades.

From the workspace root:

```sh
.venv/bin/python MysticBalloon/tools/art/import_splash.py
.venv/bin/python MysticBalloon/tools/art/build_art.py
.venv/bin/python MysticBalloon/tools/preview_splash.py
```

The [full composite review](../../../docs/splash-global-review.png) compares
the previous and current main menu at 3× and 1×, with an exact difference
image. The [art-only view](../../../docs/splash-art-review.png) and
[all menu/display modes](../../../docs/splash-review.png) check the complete
composition as well as local edges. Do not judge a drawing only from crops.
The later [motion correction](../../../docs/splash-motion-review.png)
compares the small upright balloon/slack string with the restored large
angled balloon and taut diagonal at both 3× and native size.

For further corrections, use the [draw/check/redraw workflow](../../../docs/pixel-art-workflow.md)
and `pixelpatch.py`. The importer reapplies the eye correction and the
whole-scene refinement on every import, so regeneration cannot erase them.

The importer always uses Pastel Island to assign indices; changing the
selected display palette must not reassign the drawing's color roles.
`build_art.py` packs both color and one-bit splash variants with RLE.
The preview renders the actual game menus under UBSan and writes
`docs/splash-review.png` plus individual native-size images under
`out/splash-review/`.

Visual review at 128×128 checks the tilted face and spiral curl, joined
shoulders and hands, one string from the gripping hand to the balloon
knot, continuous shapes across the old 64-row boundary, and title/main/CONF
legibility in Color, Original B&W, DMG Green and Amber. Main-menu choices
and all five CONF cursor positions are rendered by the review runner.
Original gameplay sprite and level arrays remain unchanged.
