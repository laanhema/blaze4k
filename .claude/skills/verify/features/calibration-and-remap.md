# Calibration and input remap

Two setup screens open from the options overlay. Calibration plays a click track, has the player tap along, shows the measured offset, and saves it as the global offset on Enter. Input remap lists one row per binding (keyboard rows, then pad rows), replaces a row's key with the next key pressed, rejects conflicts, and ends with a RESET TO DEFAULTS row.

## Sub-features

- `calib-open` opens the wizard from CALIBRATE OFFSET.
- `calib-measure` collects taps and shows a ready offset.
- `calib-save` writes `offset.global_offset_seconds` on Enter, only with a real audio clock and an in-range result.
- `calib-abort` leaves on Back without writing config.
- `remap-open` opens the screen from REMAP INPUT.
- `remap-capture` replaces the highlighted row's key with the next key pressed. Escape cancels.
- `remap-conflict` rejects a key bound elsewhere and keeps capturing.
- `remap-reset` restores the default bindings.
- `remap-persist` writes `input.key_bindings` to `config.json`.

## How to get to it (user POV)

- Song select, Tab, move to CALIBRATE OFFSET or REMAP INPUT, then Enter or Right.
- Back or a save returns to song select with the overlay open on the same row.

## Driving it with b4k.sh

Preconditions:

- A fresh instance on `Select`. Calibration needs real audio output, so check for `[AudioEngine] Initialized successfully` in the log.
- The steps below run in this order in one instance. The calibration steps come first because the rebind step unbinds the `Left` key.

- **Open calibration.** Run `$B keys RUN Tab Down Down Down Down Down Return`, `$B wait-screen RUN Calibration`, and `$B shot RUN 01-calibration`. The shot has the title `CALIBRATE OFFSET`, `GET READY`, SAMPLES `0/8`, and OFFSET `---`.
- **Enter too early.** Run `$B keys RUN Return`. `$B screen RUN` still prints `Calibration`.
- **Tap along.** Run `$B keys RUN wait:2.2 Left wait:0.19 Left wait:0.19 Left wait:0.19 Left wait:0.19 Left wait:0.19 Left wait:0.19 Left wait:0.19 Left wait:0.19 Left wait:0.19` and `$B shot RUN 02-ready`. The shot has `DONE`, SAMPLES `9`, an OFFSET such as `-0.211 s`, and `ENTER SAVE` in the footer.
- **Abort.** Run `$B keys RUN Escape`, `$B wait-screen RUN Select`, and `$B shot RUN 03-aborted`. The overlay is open on CALIBRATE OFFSET, which still reads `+0.000 s`.
- **Save.** Run `$B keys RUN Return`, `$B wait-screen RUN Calibration`, the tap line above, `$B keys RUN Return`, and `$B wait-screen RUN Select`. `$B shot RUN 04-saved` has the measured offset on the CALIBRATE OFFSET row.
- **Open remap.** The overlay is already open, so run `$B keys RUN Down Return`, `$B wait-screen RUN InputRemap`, and `$B shot RUN 05-remap`. The shot has the title `REMAP INPUT`, a `KEYBOARD` header, and the first row `LEFT ... Left` highlighted. From a closed overlay the keys are `Tab Down Down Down Down Down Down Return`.
- **Cancel a capture.** Run `$B keys RUN Return` and `$B shot RUN 06-capturing`. The row reads `<PRESS>`. Run `$B keys RUN Escape`. `$B screen RUN` prints `InputRemap` and the row reads `Left` again.
- **Conflict.** Run `$B keys RUN Return f` and `$B shot RUN 07-conflict`. A chip reads `IN USE: DOWN` and the row still reads `<PRESS>`.
- **Rebind.** Run `$B keys RUN a` and `$B shot RUN 08-rebound`. The row reads `LEFT ... A` and the chip reads `BOUND TO LEFT`.
- **Reset.** Send `Down` 24 times in one `$B keys RUN` call and run `$B shot RUN 09-reset-row`. `RESET TO DEFAULTS` is highlighted. Run `$B keys RUN Return`. The chip reads `DEFAULTS RESTORED`. Run `$B keys RUN Escape`, `$B wait-screen RUN Select`, `$B keys RUN wait:1 Return`, and `$B wait-screen RUN InputRemap`. The first row reads `Left` again.
- **Persisted.** Rebind again (`$B keys RUN Return a` on the first row), run `$B stop RUN`, and read `RUN/evidence/config.json`. `input.key_bindings.Left` is `["A", "D"]`, the other actions keep their defaults, and `offset.global_offset_seconds` is the saved offset.

## Gotchas

- Synthetic taps arrive with no hardware timestamp, so any offset they produce is meaningless. Use calibration recipes to prove screen flow and the save/no-save rules, never a latency number.
- The clicks are 0.5 s apart after a 2 s `GET READY` lead-in, so start tapping after it. A tap token takes 0.31 s, so `wait:0.19` between taps keeps them on the beat.
- A save prints no log line. Prove it from the CALIBRATE OFFSET row and from `config.json`.
- The wizard refuses to save when the clock is synthetic or the delay is out of range. The log says `[Calibration] ... offset not saved`. The harness cannot force either case: `launch` always has the real sound device, and the delay depends on where the taps fall between clicks. Report this path as not driven.
- Escape is reserved during capture and cancels it. It can never become a binding.
- Each key has its own row, so an action with two keys has two rows. Rebinding the first `LEFT` row replaces `Left` and keeps `D`.
- Rebinding a gameplay arrow changes later recipes in the same run. After `Left` is rebound to `A`, `$B keys RUN Left` no longer taps in calibration or moves anything. Reset or relaunch first.
