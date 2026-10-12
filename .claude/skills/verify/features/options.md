# Options overlay

On song select, Tab opens an options overlay with rows SPEED TYPE (XMOD, CMOD, MMOD), SPEED, SCROLL (UP, DOWN), FAIL (ON, OFF), ASSIST TICK (ON, OFF), CALIBRATE OFFSET, and REMAP INPUT. Speed and scroll changes show in the top-right badges (`SPEED 1x` and `UPSCROLL` by default). All four settings carry into the next song. They are saved to `config.json` on exit.

## Sub-features

- `opt-speed` picks the speed mod type and value.
- `opt-scroll` switches upscroll and downscroll.
- `opt-fail` turns fail on or off.
- `opt-assist` turns the assist tick on or off.
- `opt-persist` writes the choices to `config.json` under `gameplay`.
- `opt-actions` opens Calibration or Input remap (see `calibration-and-remap.md`).

## How to get to it (user POV)

- On song select, press Tab. Press Tab again to close.

## Driving it with b4k.sh

Preconditions:

- An instance on `Select`, launched fresh (defaults `1x`, `up`, fail on, assist off).

- **Open.** Run `$B keys RUN Tab` and `$B shot RUN 01-options`. The overlay lists the seven rows with the first row highlighted.
- **Speed.** Run `$B keys RUN Right Down Right` and `$B shot RUN 02-speed`. SPEED TYPE reads `CMOD` and SPEED reads `C460`.
- **Scroll down.** Run `$B keys RUN Down Right`. The SCROLL row reads `DOWN`.
- **Fail off.** Run `$B keys RUN Down Right`. The FAIL row reads `OFF`.
- **Assist tick on.** Run `$B keys RUN Down Right` and `$B shot RUN 03-values`. The ASSIST TICK row reads `ON`.
- **Close.** Run `$B keys RUN Tab` and `$B shot RUN 04-closed`. The badges at the top right read `SPEED C460` and `DOWNSCROLL`.
- **In play.** Start a song (`$B keys RUN Return`, `$B wait-screen RUN Gameplay`). The log's `[GameplayView] Speed ...` line shows `C-mod 460`, `downscroll`, `fail off`, and `assist tick on`.
- **Persisted.** Run `$B stop RUN` and read `gameplay` in `RUN/evidence/config.json`. `speed_mod` is `"C460"`, `scroll` is `"down"`, `fail_enabled` is `false`, and `assist_tick` is `true`.

## Gotchas

- Escape also closes the overlay (its footer reads `ESC CLOSE`) and does not leave song select.
- Left and Right change a value row, and Enter steps it forward like Right. On CALIBRATE OFFSET and REMAP INPUT, Right or Enter opens another screen instead.
- The row cursor stops at the first and last row. The XMOD speed wraps: Left from `1x` gives `8x`.
- Tab always opens the overlay on SPEED TYPE. Coming back from Calibration or Input remap, the overlay is already open on the row that launched it.
- Values reach `config.json` only on a clean exit. Use `$B stop RUN`, never kill the process, before reading it.
- Row positions come from the overlay order above. Check the screenshot before pressing Right, because a wrong row changes a different setting.
