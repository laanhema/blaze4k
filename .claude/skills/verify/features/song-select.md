# Song select

The SELECT MUSIC screen shows a scrolling wheel of pack header rows with the songs of the one open pack under it, the selected song's banner, title, artist and BPM, and a list of its 4-panel charts from hardest to easiest, each with its meter and the best saved score. Badges at the top right show the current speed mod and scroll direction. A footer lists the controls; the word after ENTER says what Confirm does on the highlighted row (`PLAY` on a song, `OPEN` on a closed pack header, `CLOSE` on the open one).

## Sub-features

- `wheel-move` moves the highlight between rows with Up and Down, and wraps at both ends. A pack header is a row the highlight stops on; the songs of a closed pack have no row and are skipped.
- `pack-toggle` opens or closes a pack when Confirm is pressed on its header. Only one pack is open at a time: opening a pack closes the other. The highlight stays on the header, the screen does not change, and the left panel shows the pack's banner (`banner.png` or another image in the pack folder with `bn` or `banner` in its name; the theme's fallback banner when the pack has none) with no song info and no charts. Every header has a marker left of its name, selected or not: a triangle pointing right when the pack is closed and down when it is open. The selected header keeps the gold bar.
- `chart-pick` steps through the selected song's charts with Left (harder) and Right (easier).
- `song-info` shows the banner (or the theme's fallback banner art), title, artist, BPM.
- `chart-score` shows the best saved percent per chart, or `---` with none.
- `start-song` starts gameplay of the highlighted chart on Confirm.
- `options-open` opens the options overlay on Tab (see `options.md`).
- `preview` plays the song's music preview while it is highlighted.

## How to get to it (user POV)

- Confirm on Title.
- Back from Gameplay or Results. The song and chart that were highlighted are restored, and the pack that holds the song is open. A highlighted header is restored too, with the same pack open.
- Back from Calibration or Input remap, which returns with the options overlay open.
- Launch with `-- --start-screen select` to skip Title.

## Driving it with b4k.sh

Preconditions:

- A fresh instance from `$B launch` (fixture pack: `Aurora Borealis SSC`, `Blaze Anthem`, `Glacier:Groove;Part 1`, `Northern Lights` in group `BLAZE PACK`).
- The fixture has one pack. To see the one-open-pack rule, copy the fixture pack folder to a second folder name inside a scratch songs directory (so there are two packs with the same songs) and launch with `--songs <scratch dir>`.

- **Open select.** Run `$B keys RUN Return`, `$B wait-screen RUN Select`, and `$B shot RUN 01-select`. On first entry, with nothing remembered, every pack is closed and the first pack header is highlighted (gold bar, right-pointing marker), as in StepMania's music wheel. The left panel shows that pack's banner and no song info, the hint bar reads `ENTER OPEN`, and no preview plays. Run `$B keys RUN Return`: the pack opens, the hint reads `ENTER CLOSE`, and its songs are listed; `Down` highlights the first song, `Aurora Borealis SSC` with a single `HARD 7` chart, and the hint reads `ENTER PLAY`.
- **Move the wheel.** From the first song run `$B keys RUN Down` and `$B shot RUN 02-wheel`. `Blaze Anthem` is highlighted and the left panel updates to it.
- **Open and close a pack.** From the first song run `$B keys RUN Up` and `$B shot RUN 02a-header`. The pack header has the gold bar and a down-pointing marker (small triangle left of the name), the hint reads `ENTER CLOSE`, and the left panel has the pack banner and no song info. Run `$B keys RUN Return` and `$B shot RUN 02b-closed`. The songs are gone, the header stays highlighted with a right-pointing marker, and `game.log` has no `[GameplayScreen] started`. With two packs, `Down` to the other header and `Return` opens it and leaves the first pack collapsed; in `02c-two-packs` the collapsed first pack, now unselected, shows a right-pointing marker and the open one a down-pointing marker.
- **Pick a chart.** On `Blaze Anthem` (five charts: `CHALLENGE 10`, `HARD 8`, `MEDIUM 5`, `EASY 3`, `BEGINNER 1`), run `$B keys RUN Right Right Right Right` and `$B shot RUN 03-chart`. The highlight moves from `CHALLENGE` to `BEGINNER`.
- **Show best saved scores.** Write `RUN/data/scores.json` before `$B launch` (`mkdir -p RUN/data` first; `launch` keeps an existing data dir). For the fixture's `Blaze Anthem`, seed Challenge `f6aa45043607306c` (`percent: 1.0`), Hard `96dd02b27954ac3b` (`1.0`), Medium `1a180a10c0d25c92` (`0.8888`) and Easy `221ef40c4dab1693` (`0.0731`), each with a `grade` string such as `"D"`; `dp` and `timestamp` are optional. Highlight `Blaze Anthem`: the rows show `100.00%`, `100.00%`, `88.88%`, `7.31%`, `---`, and `game.log` has `[Scores] Loaded`.
- **Start.** Run `$B keys RUN Return` and `$B wait-screen RUN Gameplay`. The log shows `[GameplayScreen] started '<title>' <difficulty> <meter>` naming the highlighted chart.

## Gotchas

- Charts are sorted hardest first, and every wheel move puts the cursor back on the hardest chart. Left at the top and Right at the bottom are no-ops.
- Packs and songs are listed alphabetically A–Z (packs by name, songs by title with StepMania 5 sort semantics). The order does not depend on filesystem directory order.
- The wheel lists only songs with at least one `dance-single` chart. `[NoteParser] Skipping unsupported steps type` lines in the log are expected with real packs. The fixture pack logs none.
- Keys sent right after `wait-screen RUN Select` returns can be dropped (seen with `--songs "$PWD/songs"`: 2 of 5 `Up` taps lost). It did not happen again on 2026-10-13 in three bursts of five taps on the same packs. Wait about 1.5 s before the first key, then read the song title in the screenshot before citing it.
- The fixture pack has placeholder banners and audio. The banner area shows the fallback art and no preview plays. To see a pack banner, put a real image named `banner.png` in one scratch pack folder (e.g. `convert -size 256x80 gradient:orange-red banner.png`); the copy without one shows the fallback banner on its header. Use `songs/` to check real banners and previews.
- The harness cannot observe sound. On the fixture pack, highlighting `Blaze Anthem` logs `[SoundStream] Failed to load audio file '.../music.ogg'`, which shows that the preview was attempted. With real songs, `preview` needs the owner to listen.
- A fresh run has no scores, so every row shows `---`; the keys are content hashes of the fixture charts and change if a fixture chart's notes change.
