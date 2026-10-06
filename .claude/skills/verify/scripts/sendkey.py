#!/usr/bin/env python3
"""Send synthetic X11 key events straight to one window, without moving focus.

usage: sendkey.py WINDOW_ID TOKEN...
  Key       press and release (a tap)
  +Key      press only (start a hold)
  -Key      release only (end a hold)
  wait:SECS sleep between tokens
"""
import sys
import time

from Xlib import X, XK, display
from Xlib.protocol import event

GAP_AFTER_TAP = 0.25


def send(d, win, cls, keycode):
    ev = cls(time=X.CurrentTime, root=d.screen().root, window=win, same_screen=1,
             child=X.NONE, root_x=0, root_y=0, event_x=0, event_y=0, state=0, detail=keycode)
    win.send_event(ev, propagate=True)
    d.flush()


def keycode_for(d, name):
    keysym = XK.string_to_keysym(name)
    code = d.keysym_to_keycode(keysym) if keysym else 0
    if not code:
        sys.exit(f"unknown key '{name}' (use X keysym names: Return, Escape, Tab, Left, a, ...)")
    return code


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    d = display.Display()
    win = d.create_resource_object('window', int(sys.argv[1], 16))
    for token in sys.argv[2:]:
        if token.startswith('wait:'):
            time.sleep(float(token[5:]))
        elif token.startswith('+'):
            send(d, win, event.KeyPress, keycode_for(d, token[1:]))
        elif token.startswith('-'):
            send(d, win, event.KeyRelease, keycode_for(d, token[1:]))
        else:
            code = keycode_for(d, token)
            send(d, win, event.KeyPress, code)
            time.sleep(0.06)
            send(d, win, event.KeyRelease, code)
            time.sleep(GAP_AFTER_TAP)


if __name__ == '__main__':
    main()
