---
name: verify
description: Launch the real Blaze 4k game window on the local X11 desktop, press keys in it, and capture screenshots, logs, and saved JSON as proof. Use to prove a change to a screen (title, song select, options, gameplay, results, calibration, input remap) works in the actual app, or to reproduce a reported bug before fixing it.
---

# Verify Blaze 4k

Blaze 4k is an SDL3/OpenGL desktop game. The surface a player touches is one window driven by the keyboard (or a dance pad). This skill drives that window with real key events and records what happened. Unit tests in `tests/` cover logic; this covers "does the shipped binary behave."

Everything goes through `scripts/b4k.sh` (paths below are relative to this skill's directory, `.claude/skills/verify/`). Run `scripts/b4k.sh --help` for the full usage.

## Requirements

- An X11 session with `DISPLAY` set. The owner runs i3 on X11. Wayland-only sessions are not supported by this harness.
- `python3` with `python-xlib`, `wmctrl`, ImageMagick `import`, `stdbuf`.
- A current Release build in `build/`. Rebuild with `cmake --build build -j` after changing code. `doctor` fails when any file in `src/`, `assets/`, or `CMakeLists.txt` is newer than `build/blaze-4k`.

## Launch

```bash
B=.claude/skills/verify/scripts/b4k.sh
$B launch --run /tmp/blaze4k-verify/<task-name>             # fixture songs
$B launch --run /tmp/blaze4k-verify/<task-name> -- --songs "$PWD/songs"   # owner's real packs, real audio
```

Ready means the command printed `run=... pid=... wid=... screen=Title`. Each instance gets:

- its own data dir (`RUN/data`, passed as `--data-dir`), so the owner's `build/data/config.json` and `scores.json` are never touched;
- `--songs tests/fixtures/reference_pack` unless you pass your own `--songs` after `--`;
- `--attract-timeout 0`, so it never wanders into Attract while you work. Pass `-- --attract-timeout 3` to test Attract.

Anything after `--` goes to the game (`--start-screen select`, `--perf-report`, ...). Do not pass `--data-dir`; the run dir owns it.

The window opens on the owner's visible i3 workspace as a floating, sticky 1280x720 window, and focus is handed back to whatever the owner had focused. Keys are sent straight to the game window, so the owner can keep typing elsewhere. Music and UI sounds play through the real speakers.

Two instances can run side by side with different `--run` dirs. `launch` refuses a run dir that already has a live instance.

## Doctor

```bash
$B doctor RUN
```

Read-only. Checks the environment, build freshness, that `RUN/pid` is a live `build/blaze-4k`, that the window belongs to it, prints the active screen, and surfaces load errors from `RUN/game.log`. Run it first whenever anything looks off. Without `RUN` it checks only the environment and build.

## Drive

```bash
$B keys RUN Return                      # tap
$B keys RUN Down Down Return            # taps, 0.25 s apart
$B keys RUN +Left wait:0.8 -Left        # hold Left for 0.8 s
$B wait-screen RUN Gameplay [SECS]      # block until the screen is active (default 15 s)
$B screen RUN                           # print the active screen
```

Key names are X keysyms. Default bindings (from `default_key_bindings()` in `src/data/config_loader.cpp`):

| Action | Keys |
| --- | --- |
| Left / Down / Up / Right | `Left` `Down` `Up` `Right`, or `d` `f` `j` `k` |
| Confirm | `Return`, `KP_Enter` |
| Back | `Escape` |
| Options | `Tab` |

Screen names come from `[ScreenManager] A -> B` lines in `RUN/game.log`: `Title`, `Attract`, `Select`, `Gameplay`, `Results`, `Calibration`, `InputRemap`. Always `wait-screen` after a transition before you press the next key or take a screenshot; transitions are not instant.

Per-feature recipes live in `features/`. Start at `features/README.md`.

## Evidence

```bash
$B shot RUN 02-results                  # -> RUN/evidence/02-results.png
```

If `shot` fails with `import: missing an image filename`, the window is mapped but not viewable (the owner switched workspace or covered it with a fullscreen window). Do not move the owner's windows. Report the missing shot, or wait and retry.

Look at every screenshot you cite (read the PNG). A proof includes:

- a screenshot of the state before the action and after it, numbered in order;
- the matching `RUN/game.log` lines (`[ScreenManager]`, `[GameplayView]`, `[ResultsScreen]`, ...), which carry the numbers the screen rounds (DP, judgment counts, fail time);
- side effects on disk when the feature writes any: `config.json` and `scores.json` are saved only on a clean exit, so check them in `RUN/evidence/` after `stop`.

Proof standards:

- Drive the real user path with keys. No test-only flags as a substitute: `--gameplay-demo` is a throwaway harness, not the player's path.
- Check the log line `time source audio` before claiming anything about timing or sync. The fixture pack's audio files are placeholders, so fixture songs play on a synthetic stub clock (`time source stub`). Use `-- --songs "$PWD/songs"` for audio-driven claims.
- Synthetic key events are stamped when the game receives them, not by real hardware. They prove that input reaches the right action. They do not prove judgment accuracy or latency. Use the unit tests and `tests/metronome_sync_test.cpp` for that.

## Cleanup

```bash
$B stop RUN
```

Closes the window like a player would (the game saves config and scores), waits up to 10 s, then kills only `RUN`'s own PID if it hangs. It copies `game.log`, `config.json` and `scores.json` into `RUN/evidence/` and deletes `RUN/data`. Evidence stays in `RUN/evidence/`. Never `pkill blaze-4k`: the owner may be playing.

Run `stop` after every attempt, including failed ones.

## Helpers

- `scripts/b4k.sh` is the entry point for every step above.
- `scripts/sendkey.py WINDOW_ID TOKEN...` sends X11 key events to one window via `XSendEvent`. `b4k.sh keys` calls it; use it directly only for a window you did not launch with `b4k.sh`.
