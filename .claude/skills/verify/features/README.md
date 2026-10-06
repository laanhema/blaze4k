# Blaze 4k verification map

This directory is the maintained source for verifying what a player can do in Blaze 4k. Read this index, then use the matching feature file as the recipe. Every command assumes `B=.claude/skills/verify/scripts/b4k.sh` from the repo root and a `RUN` dir from `$B launch` (see `../SKILL.md`).

## Baseline preconditions

- `$B doctor` passes: X11 display, tools present, `build/blaze-4k` newer than `src/` and `assets/`.
- A fresh instance from `$B launch --run /tmp/blaze4k-verify/<task>`, which starts on `Title` with an empty data dir and the fixture pack (`tests/fixtures/reference_pack`, one group `BLAZE PACK` with 4 songs).
- Audio-driven claims need the owner's packs instead: `$B launch --run RUN -- --songs "$PWD/songs"`. `songs/` is gitignored and exists only on the owner's machine.
- Never drive an instance this run did not launch.

## Driving conventions

- Navigate with actions, not coordinates: `Left` `Down` `Up` `Right`, `Return` (Confirm), `Escape` (Back), `Tab` (Options).
- `$B wait-screen RUN <Screen>` after every key that changes screens.
- Read every screenshot you cite.
- `Escape` on `Title` quits the game. That is a clean exit, and `$B stop RUN` still collects the evidence.

## Proof and skip reporting

- Capture the screen before and after the action, plus the `RUN/game.log` lines that carry the exact values.
- For anything that persists (options, offset, bindings, high scores), prove it from `RUN/evidence/config.json` or `scores.json` after `$B stop RUN`, not from the screen alone.
- State whether the run used the fixture pack (stub clock) or real songs (`time source audio`).
- Report an unreachable path with the attempted command and the unmet precondition. Do not report a skipped entry point as verified through another one.

## Feature entry contract

Each feature file starts with an H1 title and one paragraph describing what the player sees. It then has exactly four H2 sections in this order: `Sub-features`, `How to get to it (user POV)`, `Driving it with b4k.sh`, `Gotchas`. Keep implementation details out unless they change what a verifier must do.

## Features

- [Title and attract](./title-and-attract.md) covers the title screen, entering song select, quitting, and the idle attract loop.
- [Song select](./song-select.md) covers the music wheel, groups, difficulty choice, and the high-score column. *Partly driven 2026-10-06 (wheel, start song).*
- [Gameplay and results](./gameplay-and-results.md) covers playing a chart, failing or clearing, the score screen, and high-score saving. *Driven 2026-10-06 except `results-save`.*
- [Options overlay](./options.md) covers speed mods, scroll direction, fail, and assist tick, and that they persist. *Driven end to end 2026-10-06.*
- [Calibration and input remap](./calibration-and-remap.md) covers the global offset wizard and key remapping. *Not yet driven.*
