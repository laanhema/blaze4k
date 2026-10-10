# Song select

The SELECT MUSIC screen shows a scrolling wheel of songs grouped by pack, the selected song's banner, title, artist and BPM, and a list of its 4-panel charts from hardest to easiest, each with its meter and the best saved score. Badges at the top right show the current speed mod and scroll direction. A footer lists the controls.

## Sub-features

- `wheel-move` moves the highlight between songs with Up and Down.
- `chart-pick` steps through the selected song's charts with Left (harder) and Right (easier).
- `song-info` shows banner (or the fallback background), title, artist, BPM.
- `chart-score` shows the best saved percent per chart, or `---` with none.
- `start-song` starts gameplay of the highlighted chart on Confirm.
- `options-open` opens the options overlay on Tab (see `options.md`).
- `preview` plays the song's music preview while it is highlighted.

## How to get to it (user POV)

- Confirm on Title.
- Back from Gameplay, Results, Calibration, or Input remap.
- Launch with `-- --start-screen select` to skip Title.

## Driving it with b4k.sh

Preconditions:

- A fresh instance from `$B launch` (fixture pack: `Aurora Borealis SSC`, `Blaze Anthem`, `Glacier:Groove;Part 1`, `Northern Lights` in group `BLAZE PACK`).

- **Open select.** Run `$B keys RUN Return`, `$B wait-screen RUN Select`, and `$B shot RUN 01-select`. `Aurora Borealis SSC` is highlighted with a single `HARD 7` chart.
- **Move the wheel.** Run `$B keys RUN Down` and `$B shot RUN 02-wheel`. `Blaze Anthem` is highlighted and the left panel updates to it.
- **Pick a chart.** On a song with several charts (e.g. `Anubis` in the owner's `songs/`), run `$B keys RUN Right Right` and `$B shot RUN 03-chart`. The highlighted row moves two steps toward Beginner.
- **Start.** Run `$B keys RUN Return` and `$B wait-screen RUN Gameplay`. The log shows `[GameplayScreen] started '<title>' <difficulty> <meter>` naming the highlighted chart.

## Gotchas

- Charts are sorted hardest first, so the cursor starts on the hardest chart. Left at the top is a no-op.
- The wheel lists only songs with at least one `dance-single` chart. `[NoteParser] Skipping unsupported steps type` lines in the log are expected.
- Keys sent right after `wait-screen RUN Select` returns can be dropped (seen with `--songs "$PWD/songs"`: 2 of 5 `Up` taps lost). Wait about 1.5 s before the first key, then read the song title in the screenshot before citing it.
- The fixture pack has placeholder banners and audio. The banner area may show generated art and no preview plays. Use `songs/` to check real banners and previews.
