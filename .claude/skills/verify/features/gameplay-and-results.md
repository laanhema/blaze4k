# Gameplay and results

After a chart is picked, the note field scrolls arrows toward the receptors in time with the music, with a difficulty badge, a life bar, and judgment pops. The run ends when the chart finishes or life empties (when fail is on). The SCORE SCREEN then shows the grade, percent, dance points, judgment counts, max combo, holds and mines, and a FAILED or NEW RECORD ribbon. Confirm returns to song select.

## Sub-features

- `play-start` loads the chart and plays it against the music clock.
- `play-input` turns Left, Down, Up, Right presses into judgments.
- `play-fail` ends the run with FAILED when life reaches zero and fail is on.
- `play-abort` returns to select on Back with no result recorded.
- `results-show` shows grade, percent, DP, per-window counts, combo, holds, mines.
- `results-save` saves a passing run to `scores.json` and flags NEW RECORD when it beats the best.

## How to get to it (user POV)

- On song select, highlight a song and chart and press Enter.

## Driving it with b4k.sh

Preconditions:

- A fresh instance on `Select` (see `song-select.md`), with fail on. The steps below run in this order in one instance.
- For music-driven play and for `play-input`, a second instance launched with `-- --songs "$PWD/songs"`.

- **Start a chart and fail.** Highlight `Blaze Anthem` (`CHALLENGE 10`) and run `$B keys RUN Return`, `$B wait-screen RUN Gameplay`, and `$B shot RUN 02-gameplay` at once. The shot has the badge, the life bar, and arrows crossing the receptors. The log shows `[GameplayView] Loaded chart 'Challenge' (meter 10): taps=... holds=...`, a line with `time source stub`, and `[GameplayView] Failed: life empty at 1.28333s`. Then run `$B wait-screen RUN Results 15`.
- **Results.** Run `$B keys RUN wait:3` (the percent counts up) and `$B shot RUN 03-results`. Compare it with the log lines `[GameplayView] Score: DP -72/70 (...) grade D | combo ... | F 0 E 0 G 0 D 0 W 0 M 6 ...` and `[ResultsScreen] Blaze Anthem Challenge 10: D 0.00% DP -72/70 FAILED`. The shot has MISS `6`, DANCE POINTS `0 / 70`, and the FAILED ribbon.
- **Back to select.** Run `$B keys RUN Return` and `$B wait-screen RUN Select`.
- **Abort.** Run `$B keys RUN wait:1.5 Right Right Right Right` to highlight `BEGINNER 1`. Then run `$B keys RUN Return`, `$B wait-screen RUN Gameplay`, `$B keys RUN Escape`, and `$B wait-screen RUN Select`. The log has a `[GameplayView] Score:` line and no `[ResultsScreen]` line after it.
- **Pass and save.** Run `$B keys RUN wait:1.5 Return`, `$B wait-screen RUN Gameplay`, press nothing, and run `$B wait-screen RUN Results 20`. The log shows `merciful beginner on`, `Score: DP 0/30 (0.00%) grade D ... M 4 ... life 0.100 alive`, and `[ResultsScreen] Blaze Anthem Beginner 1: D 0.00% DP 0/30 CLEARED NEW RECORD`. After `$B keys RUN wait:3`, `$B shot RUN 04-pass` has the NEW RECORD ribbon.
- **Best on select.** Run `$B keys RUN Return`, `$B wait-screen RUN Select`, and `$B shot RUN 05-best`. The `BEGINNER` row reads `0.00%`. A second pass of the same chart logs `CLEARED` without `NEW RECORD`.
- **Saved score.** After `$B stop RUN`, read `RUN/evidence/scores.json`. It has one record with `"grade": "D"`, `"percent": 0.0`, and `"dp": 0`. A run with only failed or aborted plays leaves `"scores": {}`.
- **Press arrows.** On the real-songs instance, highlight a `BEGINNER` chart and start it. Check the log for `time source audio`. Run `$B keys RUN $(for i in $(seq 80); do printf '+Left +Down +Up +Right wait:0.05 -Left -Down -Up -Right wait:0.12 '; done)`, which presses all four arrows about six times a second for 14 s. Then run `$B keys RUN Escape`. The `[GameplayView] Score:` line has counts above zero among `F E G D W` and `max` combo above zero.

## Gotchas

- The fixture pack's music files are placeholders (`[SoundStream] Failed to load audio file ...`), so fixture songs run on a synthetic stub clock. That is fine for screen-flow proofs and wrong for timing proofs.
- Fixture charts start at chart time zero with no lead-in. Arrows are already at the receptors when `wait-screen RUN Gameplay` returns, so take the gameplay shot at once and do not expect taps to land.
- Synthetic keys are stamped on arrival, so the judgments they produce say nothing about timing accuracy.
- The pass above needs no taps: four misses leave life at 0.1, and Beginner charts clamp DP at zero, so the run clears at `0.00%` and is saved. An all-miss run on a harder chart has negative DP.
- The score screen clamps the display. DP can be negative in the log and show `0` on screen.
- The results reveal takes about 2.4 s. Screenshot after `wait:3`. A Confirm during the reveal only skips it, and a second Confirm leaves. Escape leaves at once.
- Do not send `Return Return` in one `keys` call on Results: the first leaves, and the second starts the same chart again.
