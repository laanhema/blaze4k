# Title and attract

The game opens on a title screen with the BLAZE 4K logo, four arrows, and a version footer (`BLAZE 4K v0.1.0`). Confirm enters song select. Left idle on Title or Select, the game switches to an attract loop and returns on Confirm or Back.

## Sub-features

- `title-render` shows the logo, the `SINGLE · 4 PANEL` caption, and the version footer.
- `title-enter` goes to song select on Confirm.
- `title-quit` exits the game on Back.
- `attract-idle` enters Attract after the idle timeout on Title or Select.
- `attract-exit` returns to the screen it came from on Confirm or Back.

## How to get to it (user POV)

- Start the game. Title is the first screen.
- From song select, press Escape to return to Title.
- Wait without pressing anything for the idle timeout (default 120 s).

## Driving it with b4k.sh

Preconditions:

- A fresh instance from `$B launch`. For attract, launch with `-- --attract-timeout 3`.

- **Title renders.** Run `$B shot RUN 01-title`. The logo, arrows, and `BLAZE 4K v0.1.0` footer are visible.
- **Enter select.** Run `$B keys RUN Return` and `$B wait-screen RUN Select`. The log shows `[ScreenManager] Title -> Select`.
- **Back to title.** Run `$B keys RUN Escape` and `$B wait-screen RUN Title`.
- **Attract on idle.** With `--attract-timeout 3`, run `$B wait-screen RUN Attract 10` and `$B shot RUN 02-attract`. The log shows `[AttractScreen] title loop active (any Confirm exits)`.
- **Leave attract.** Run `$B keys RUN Return` and `$B wait-screen RUN Title`. Attract returns to the screen it interrupted.
- **Quit.** On Title, run `$B keys RUN Escape`. The log ends with `Blaze 4k shut down cleanly.`

## Gotchas

- `$B launch` passes `--attract-timeout 0` (attract off) so other recipes are not interrupted. Override it only for this feature.
- The idle timer counts only while Title or Select is active and resets on any key press.
- Escape on Title ends the instance. Take your screenshots first.
