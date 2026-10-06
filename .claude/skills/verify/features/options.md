# Options overlay

On song select, Tab opens an options overlay with rows SPEED TYPE (XMOD, CMOD, MMOD), SPEED, SCROLL (UP, DOWN), FAIL (ON, OFF), ASSIST TICK (ON, OFF), CALIBRATE OFFSET, and REMAP INPUT. Changes show in the top-right badges and carry into the next song. They are saved to `config.json` on exit.

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
- **Scroll down.** Run `$B keys RUN Down Down Right` and `$B shot RUN 02-scroll`. The SCROLL row reads `DOWN`.
- **Fail off.** Run `$B keys RUN Down Right`. The FAIL row reads `OFF`.
- **Close.** Run `$B keys RUN Tab` and `$B shot RUN 03-closed`. The badge at the top right reads `DOWNSCROLL`.
- **In play.** Start a song (`$B keys RUN Return`, `$B wait-screen RUN Gameplay`). The log's `[GameplayView] Speed ...` line shows `downscroll` and `fail off`.
- **Persisted.** Run `$B stop RUN` and read `RUN/evidence/config.json`. `gameplay.scroll` is `"down"` and `gameplay.fail_enabled` is `false`.

## Gotchas

- Escape also closes the overlay (its footer reads `ESC CLOSE`) and does not leave song select.
- Left and Right change a value row. On CALIBRATE OFFSET and REMAP INPUT, Right or Enter opens another screen instead.
- Values reach `config.json` only on a clean exit. Use `$B stop RUN`, never kill the process, before reading it.
- Row positions come from the overlay order above. Check the screenshot before pressing Right, because a wrong row changes a different setting.
