# Calibration and input remap

Two setup screens open from the options overlay. Calibration plays a click track, has the player tap along, shows the measured delay, and saves it as the global offset on Enter. Input remap lists each action with its keys and buttons, captures the next key pressed as a new binding, rejects conflicts, and offers a RESET row.

## Sub-features

- `calib-open` opens the wizard from CALIBRATE OFFSET.
- `calib-measure` collects taps and shows a ready offset.
- `calib-save` writes `offset.global_offset_seconds` on Enter, only with a real audio clock and an in-range result.
- `calib-abort` leaves on Back without writing config.
- `remap-open` opens the screen from REMAP INPUT.
- `remap-capture` binds the next key to the highlighted action. Escape cancels.
- `remap-conflict` rejects a key bound elsewhere and keeps capturing.
- `remap-reset` restores the default bindings.
- `remap-persist` writes `input.key_bindings` to `config.json`.

## How to get to it (user POV)

- Song select, Tab, move to CALIBRATE OFFSET or REMAP INPUT, then Enter or Right.

## Driving it with b4k.sh

Preconditions:

- An instance on `Select`. Calibration needs real audio output, so check for `[AudioEngine] Initialized successfully` in the log.

- **Open calibration.** Run `$B keys RUN Tab Down Down Down Down Down Return`, `$B wait-screen RUN Calibration`, and `$B shot RUN 01-calibration`.
- **Tap along.** Send taps on the clicks, e.g. `$B keys RUN Left wait:0.4 Left wait:0.4 Left`, and so on. The samples counter rises.
- **Abort.** Run `$B keys RUN Escape` and `$B wait-screen RUN Select`. After `$B stop RUN`, `offset.global_offset_seconds` in `RUN/evidence/config.json` is unchanged.
- **Open remap.** Run `$B keys RUN Tab Down Down Down Down Down Down Return`, `$B wait-screen RUN InputRemap`, and `$B shot RUN 02-remap`.
- **Rebind.** With the first action highlighted, run `$B keys RUN Return a` and `$B shot RUN 03-rebound`. The row shows `A`.
- **Persisted.** Run `$B stop RUN` and read `input.key_bindings` in `RUN/evidence/config.json`.

## Gotchas

- Synthetic taps arrive with no hardware timestamp, so any offset they produce is meaningless. Use calibration recipes to prove screen flow and the save/no-save rules, never a latency number.
- The wizard refuses to save when the clock is synthetic or the delay is out of range. The log says `[Calibration] ... offset not saved`.
- Escape is reserved during capture and cancels it. It can never become a binding.
- Rebinding a gameplay arrow changes later recipes in the same run. Reset or relaunch first.
