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

- An instance on `Select` (see `song-select.md`).
- For music-driven play, launched with `-- --songs "$PWD/songs"`.

- **Start a chart.** Highlight `Blaze Anthem` (fixture) and run `$B keys RUN Return`, `$B wait-screen RUN Gameplay`, `$B keys RUN wait:4`, and `$B shot RUN 02-gameplay`. The log shows `[GameplayView] Loaded chart '<difficulty>' (meter N): taps=... holds=...` and a line with `time source audio` or `time source stub`.
- **Press arrows.** Run `$B keys RUN Left Down Up Right` or a hold, `$B keys RUN +Left wait:0.5 -Left`. Judgment pops appear on screen.
- **Fail.** Press nothing. With fail on, life drains and the log shows `[GameplayView] Failed: life empty at <t>s`. Then run `$B wait-screen RUN Results 180`.
- **Results.** Run `$B keys RUN wait:3` (the counters animate) and `$B shot RUN 03-results`. Compare it with the log lines `[GameplayView] Score: DP a/b (...) grade G | combo ... | F E G D W M ...` and `[ResultsScreen] <song> <difficulty> <meter>: <grade> <percent> DP a/b FAILED`. The miss count and ribbon on screen match the log.
- **Back to select.** Run `$B keys RUN Return` and `$B wait-screen RUN Select`.
- **Abort.** During gameplay, run `$B keys RUN Escape` and `$B wait-screen RUN Select`. No `[ResultsScreen]` line follows.
- **Saved score.** After `$B stop RUN`, read `RUN/evidence/scores.json`. A failed or aborted run leaves `"scores": {}`. A passing run adds a record for the chart.

## Gotchas

- The fixture pack's music files are placeholders (`[SoundStream] Failed to load audio file ...`), so fixture songs run on a synthetic stub clock. That is fine for screen-flow proofs and wrong for timing proofs.
- Synthetic keys are stamped on arrival, so the judgments they produce say nothing about timing accuracy.
- Passing a chart with synthetic taps is impractical, so `results-save` is not reachable through this harness yet. Prove the save rule with `tests/` and report the on-screen path as not driven.
- The score screen clamps the display. DP can be negative in the log and show `0` on screen.
- Results counters animate for about 2 s. Screenshot after `wait:3`.
