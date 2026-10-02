# Cel noteskin

The default gameplay noteskin: the base "Cel - Workshop" skin from
[HURG-IIDX/Noteskin-Workshop-Cel-and-Metal](https://github.com/HURG-IIDX/Noteskin-Workshop-Cel-and-Metal)
(commit `5ba831a`), itself based on the ITG default "Cel" skin. Released into the
public domain under the Unlicense (see `LICENSE`).

Only the textures Blaze 4k draws are vendored, under their original file
names. That means any PNG from the Workshop's `Customizations/.../Cel/<variant>/`
folders can be dropped in here to replace the matching file (e.g. another arrow
color set or hold color).

| File | Used for |
| ---- | -------- |
| `_Down Tap Note 16x16 (doubleres).png` | Tap/hold/roll heads: 8 quantization colors x 32 frames animated over 2 beats |
| `_Down Receptor tex 4x1 (res 256x64).png` | Receptors (frame 0), beat-pulsed |
| `Down {Hold,Roll} Body {Active,Inactive} (res 64x256).png` | Hold/roll bodies, tiled from the tail |
| `Down {Hold,Roll} BottomCap {Active,Inactive} (res 64x64).png` | Hold/roll end caps, centred on the hold's end (the body stops half a note before it: `metrics.ini` `StopDrawingHoldBodyOffsetFromTail=-32`) |
| `_mine tex.png` | Mine ring (top half) and core colors (bottom half) |
| `explosions/Down Tap Explosion Dim W{1..5} (res 125x125).png` | Receptor flash per tap judgment (Fantastic..Way Off); W1 also flashes when a hold/roll ends OK |
| `explosions/down hold explosion (res 125x125).png` | Receptor glow while a hold/roll is active |
| `explosions/Fallback HitMine Explosion.png` | Additive burst when a mine is hit. Not part of the Workshop skin: it is StepMania's `common` fallback noteskin art (the Cel skin sets `FallbackNoteSkin=common`), under StepMania's license (see `explosions/LICENSE-StepMania.txt`) |

All art faces Down; the engine rotates it per column, as StepMania does.
Explosions are optional: a missing explosion PNG only disables that explosion.
The Bright (100+ combo) W1 explosion, the explosion glow flicker, lifts and the
3D mine model are not used.
