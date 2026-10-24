# Efficient pixel drawing for agents

Use an indexed PNG as the canvas and small text stencils as the editable
source. Each `0..F` character is an existing palette index; `.` leaves the
pixel untouched. An anchor places the stencil without listing individual
coordinates. This preserves exact pixels, palette roles and dimensions.
The local tool is [pixelpatch.py](../tools/art/pixelpatch.py).

## Draw, check, redraw

1. Begin with the entire composition at native size and integer zoom.
   Identify its focal points, lighting, palette, silhouettes and reserved
   text areas. Then inspect only the needed crop's indices and enlarged
   clusters before drawing.
2. Draw a small stencil into a separate candidate PNG. A source-pixel hash
   rejects stale coordinates. The tool emits the changed-pixel count,
   bounds, hash and a before/after/difference sheet in the same call.
3. Check both the enlarged crop and the whole image at native size after
   every pass. Use `overview` to keep before/after/difference full frames
   together at 3× and 1×. Inspect
   expression, gaze, contour continuity and contrast. More pixels and
   detail do not automatically improve a small drawing.
4. Redraw only the stencil. Apply every candidate to the same original
   base, rather than stacking accidental edits or reimporting the image.
5. Once the drawing works, render the actual game in all display modes,
   with menu overlays. Check changed-pixel bounds and palette invariants.
   Then regenerate packed assets and the cartridge once and build/check.

The eye edit used two candidates: round outlines looked like goggles;
smaller tilted shapes made the eyes part of the face and aimed their gaze
toward the balloon. The final patch changes 42 pixels. Its 120 stencil
cells replace authoring a 16,384-cell canvas; this measures pixel payload,
not a claim about tokenizer counts or every agent task.

## Commands

Run from the workspace root. The first command returns just a small text
crop; the second draws and prepares the visual feedback together.

```sh
.venv/bin/python MysticBalloon/tools/art/pixelpatch.py inspect \
  MysticBalloon/tools/art/splash/continuous-base.png --box 18 77 22 13

.venv/bin/python MysticBalloon/tools/art/pixelpatch.py draw \
  MysticBalloon/tools/art/splash/continuous-base.png \
  --patch MysticBalloon/tools/art/splash/eyes.json \
  --out MysticBalloon/out/eyes/candidate.png \
  --review MysticBalloon/out/eyes/review.png

.venv/bin/python MysticBalloon/tools/art/pixelpatch.py overview \
  MysticBalloon/docs/splash-before-polish.png \
  --after MysticBalloon/out/splash-polish/after-main-native.png \
  --out MysticBalloon/out/splash-polish/overview.png

.venv/bin/python MysticBalloon/tools/art/import_splash.py
.venv/bin/python MysticBalloon/tools/art/build_art.py
.venv/bin/python MysticBalloon/tools/preview_splash.py
```

The [final stencils](../tools/art/splash/eyes.json) are the source of truth.
The [eye comparison](eyes-review.png) shows the final edit and its exact
pixel difference. [Game review](splash-review.png) includes main and CONF
selections in Color, Original B&W, DMG Green and Amber.

## Whole-scene consistency and compositing

The [full-frame review](splash-global-review.png) led to changes that crops
alone missed: fewer intentional colors, a clearly framed balloon, a clean
grip and string crossing, background vapor separated from skin, and space
for every menu. The subsequent motion review restored the large angled
balloon and a taut diagonal string: intentional edge cropping can convey
motion better than shrinking a subject to fit. Foreground menu cards may
occlude the drawing; bending the string around UI made it look slack.
The [motion comparison](splash-motion-review.png) records the correction
at 3× and native size, including an exact pixel difference.
`polish_splash.py` records these indexed-pixel drawing
operations; candidate passes run locally before packing and compiling.

Treat the mask as part of the artwork. Keep a foreground silhouette,
restore it over background strokes, then draw contacts such as fingers
closing around the string. Avoid stray matte colors and transparent gaps
at joins. Check all edges and crossings in the full scene after local edits.

Trim padding to the actual visible shape: the original title is stored as
42×24 but its frame occupies only 42×18. Painting the entire rectangle as
opaque erased 12 rows of unrelated illustration at 2×. The corrected
composite preserves the title's ink and paper inside the frame, while
leaving outer corners and unused rows transparent. Check menus as separate
composites too, including the widest label and the bottom selection.

Validation checks the eight-color set, the bordered CONF matte, an unbroken
knot-to-grip line, unchanged title ink, transparent padding/corners, and
the actual game's menu renders in all four modes. The cartridge now fits
its eleven available art colors without quantization loss.

## Research and design choices

- [Pixel Joint's tutorial](https://pixeljoint.com/forum/forum_posts.asp?TID=11299)
  emphasizes deliberate pixel placement, a small canvas, limited colors
  and readable clusters. For these eyes, keep whites and pupils distinct,
  match the face's tilt, and reserve isolated pixels for useful highlights.
- [Aseprite drawing](https://www.aseprite.org/docs/drawing/) and
  [batch scripting](https://www.aseprite.org/docs/cli/) support exact
  pencil/pixel operations and repeatable automated work. Our existing
  Pillow environment supplies the smaller indexed-PNG tool needed here.
- [AgentPaint](https://github.com/ccc007ccc/agent-paint-pixel-skills)
  demonstrates palette-symbol rows, local chunks and separate scaled
  previews for agents. The repository's `CHGame/tools/artkit/pixel.py`
  already has a related ASCII patch primitive. This workflow adapts that
  established idea to the game's semantic palette and reproducible import.
- [Anthropic's agent-tool guidance](https://www.anthropic.com/research/writing-tools-for-agents)
  recommends focused tools, relevant filtered context, compact responses
  and actionable validation errors. Here that means crop-only inspection,
  one draw-and-review operation, and short diff reports. Visual judgment
  still requires looking at the rendered image after each drawing pass.

Do not turn every correction into a new tool or a full firmware upload.
Keep the fast local drawing loop separate from the final game/device
verification. Keep the base, stencil, palette and review commands in the
repository so the next agent can resume without reconstructing the edit.
