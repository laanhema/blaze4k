# Title and attract

The game opens on a title screen with the BLAZE 4K logo, an `ARROW RHYTHM GAME` caption, four arrows, a blinking PRESS START plate, and a footer (`SINGLE · 4 PANEL` on the left, `BLAZE 4K v0.1.0` on the right). Confirm enters song select. Left idle on Title or Select, the game switches to an attract loop and returns on Confirm or Back.

## Sub-features

- `title-render` shows the logo, the `ARROW RHYTHM GAME` caption, the arrows, and the footer.
- `title-enter` goes to song select on Confirm.
- `title-quit` exits the game on Back.
- `attract-idle` enters Attract after the idle timeout on Title or Select.
- `attract-exit` returns to the screen it came from on Confirm or Back. The arrow keys do not exit.

## How to get to it (user POV)

- Start the game. Title is the first screen.
- From song select, press Escape to return to Title.
- Wait without pressing anything for the idle timeout (default 120 s).

## Driving it with b4k.sh

Preconditions:

- A fresh instance from `$B launch`. For the two attract steps, use a second instance launched with `-- --attract-timeout 6`.

- **Title renders.** Run `$B shot RUN 01-title`. The logo, caption, arrows, and both footer texts are visible. PRESS START blinks, so it can be dim in the shot.
- **Enter select.** Run `$B keys RUN Return` and `$B wait-screen RUN Select`. The log shows `[ScreenManager] Title -> Select`.
- **Back to title.** Run `$B keys RUN Escape` and `$B wait-screen RUN Title`.
- **Quit.** On Title, run `$B keys RUN Escape`. The process exits by itself and the log ends with `[App] Back pressed. Exiting.` and `Blaze 4k shut down cleanly.`
- **Attract on idle.** On the `--attract-timeout 6` instance, run `$B wait-screen RUN Attract 15` and `$B shot RUN 02-attract`. The log shows `[AttractScreen] title loop active (any Confirm exits)` and `[ScreenManager] Title -> Attract`. The shot has grey arrows lighting up in turn and no caption or footer.
- **Leave attract.** Run `$B keys RUN Left` and `$B screen RUN`, which still prints `Attract`. Then run `$B keys RUN Return` and `$B wait-screen RUN Title`. Attract returns to the screen it interrupted: entered from Select, it logs `Attract -> Select`.

## Gotchas

- `$B launch` passes `--attract-timeout 0` (attract off) so other recipes are not interrupted. Override it only for this feature.
- The idle timer counts only while Title or Select is active. A press of a key bound to an action resets it. An unbound key such as `x` does not.
- With a short timeout, the instance is back in Attract after any pause, such as reading a screenshot. A key sent then only leaves Attract: `Escape` returns to Title instead of quitting. Run `$B screen RUN` before the key, and run the quit step on a default launch.
- Escape on Title ends the instance. Take your screenshots first.
