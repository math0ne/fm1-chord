#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 fm1-chord contributors
"""The fm1-chord controls diagram (screenshots/controls.png): a drawn schematic of the FM-1's panel (the
geometry of Felucca's docs/panel.jpg: 8 knobs, 12 buttons in two rows, OCT- / OCT+, 27 keys) with every
control labelled by what it does in the HiChord UI. One colour scheme throughout: a control is drawn in
the colour of what it opens or does (yellow sound, grey key, red mode, green presets and play, blue
looper, white neutral), and its card carries the same bar. Cards size themselves to their text. Pillow only.

  gen_controls_diagram.py OUT.png
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets/fonts/InterTight[wght].ttf"
W, H = 1200, 860
BG, CARD, INK, DIM = (30, 30, 32), (70, 70, 74), (255, 255, 255), (178, 178, 184)
GREY, YELLOW, RED, GREEN, BLUE, WHITE = (150, 152, 160), (245, 196, 0), (226, 62, 62), (70, 200, 120), (80, 150, 255), (225, 225, 230)
DEG = [(255, 92, 92), (255, 160, 48), (250, 220, 70), (96, 214, 120), (72, 200, 236), (120, 128, 255), (206, 112, 240)]
KEY_W, KEY_B = (236, 236, 240), (22, 22, 26)


def font(size, weight=500):
    f = ImageFont.truetype(str(FONT), size)
    try:
        f.set_variation_by_axes([weight])
    except Exception:
        pass
    return f


F_TITLE, F_BODY, F_SUB = font(11, 600), font(15, 600), font(12, 450)

# the device, drawn: a body of 760 wide at (220, 110); positions follow docs/panel.jpg, with the rows
# under the knobs pushed down 26 px to give the top buttons' leader lanes room
DX, DY, DW, DH, PUSH = 220, 110, 760, 456, 26


def dev(x, y):                       # photo pixels (1750 x 1050, crop 70..1680 x 60..960) -> canvas
    return DX + (x - 70) * DW / 1610, DY + (y - 60) * 430 / 900 + (PUSH if y > 300 else 0)


KNOBS = {"MASTER": (175, 178), "SELECT": (355, 178), "PRESETS": (175, 350), "ALGORITHM": (355, 350),
         "KNOB 1": (978, 178), "KNOB 2": (1163, 178), "KNOB 3": (1348, 178), "KNOB 4": (1533, 178)}
KNOB_CAP = {"KNOB 1": "FILTER", "KNOB 2": "RESO", "KNOB 3": "ATTACK", "KNOB 4": "RELEASE"}
BTN_X = (997, 1105, 1207, 1315, 1417, 1525)
BUTTONS = {}
for row, y, names in ((0, 352, "FX SCL ENV LFO EDIT GLO"), (1, 454, "HOME SAVE ARP SEQ PLAY REC")):
    for x, n in zip(BTN_X, names.split()):
        BUTTONS[n] = (x, y)
BUTTONS["OCT-"], BUTTONS["OCT+"] = (198, 487), (328, 487)
# what each button opens or does, by colour: sound yellow, key grey, mode red, presets / transport green, looper blue
BTN_COL = {"FX": YELLOW, "ENV": YELLOW, "LFO": YELLOW, "GLO": YELLOW, "SCL": GREY, "EDIT": RED, "ARP": RED,
           "SAVE": GREEN, "PLAY": GREEN, "SEQ": BLUE, "REC": RED, "HOME": WHITE, "OCT-": WHITE, "OCT+": WHITE}
SOLID = {"FX", "SCL", "EDIT", "SAVE", "SEQ", "PLAY", "REC"}          # the menu / transport buttons: filled
KNOB_COL = {"SELECT": GREY, "PRESETS": YELLOW, "ALGORITHM": RED, "KNOB 1": YELLOW, "KNOB 2": YELLOW, "KNOB 3": YELLOW,
            "KNOB 4": YELLOW, "MASTER": WHITE}
KW, KH = 40.5, 150                                       # a white key
BLACK_AFTER = (0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14)       # the black keys: after these white keys
BLACK_FN = ("INV", "LCK", "HLD", "↑", "↗", "→", "↘", "↓", "↙", "←", "↖")


def white_x(i):
    return dev(100, 580)[0] + i * (KW + 2)


def key_labels(d, ky):
    """the keys' labels, drawn last so the leader lines run behind them"""
    for i in range(16):
        x, deg = white_x(i), i % 7
        cx = x + KW / 2
        if i < 7:                                        # a chord key: its degree, in the degree's colour
            d.rounded_rectangle((cx - 9, ky + KH - 39, cx + 9, ky + KH - 21), 4, fill=KEY_W)
            d.text((cx, ky + KH - 30), str(deg + 1), fill=(90, 90, 96), font=font(13, 600), anchor="mm")
            d.rectangle((x + 6, ky + KH - 14, x + KW - 6, ky + KH - 8), fill=DEG[deg])
        else:                                            # a strumplate key: a bar as high as its note
            j, h = i - 7, 10 + (i - 7) * 4
            d.rounded_rectangle((cx - 8, ky + KH - 12 - h - 2, cx + 8, ky + KH - 12 + 2), 3, fill=KEY_W)
            d.rounded_rectangle((cx - 6, ky + KH - 12 - h, cx + 6, ky + KH - 12), 2, fill=(150, 150, 156))
    for p, a in enumerate(BLACK_AFTER):
        x = white_x(a) + KW - 11
        col = GREEN if p < 3 else YELLOW
        d.rounded_rectangle((x + 1, ky + 58, x + 23, ky + 82), 3, fill=KEY_B)
        d.text((x + 12, ky + 70), BLACK_FN[p], fill=col, font=font(9 if p < 3 else 15, 700), anchor="mm")


def draw_device(d):
    d.rounded_rectangle((DX, DY, DX + DW, DY + DH), 26, fill=(58, 58, 62), outline=(90, 90, 96), width=2)
    sx, sy = dev(490, 140)                                   # the screen
    d.rounded_rectangle((sx, sy, sx + 150, sy + 150), 8, fill=(16, 18, 24), outline=(110, 110, 116), width=2)
    d.rounded_rectangle((sx + 6, sy + 6, sx + 68, sy + 26), 4, fill=GREY)
    d.rounded_rectangle((sx + 72, sy + 6, sx + 118, sy + 26), 4, fill=RED)
    d.text((sx + 75, sy + 36), "C7", fill=DEG[0], font=font(34, 700), anchor="mm")
    for i in range(7):
        d.rounded_rectangle((sx + 10 + i * 19, sy + 110, sx + 24 + i * 19, sy + 134), 3, fill=(38, 40, 48))
        d.rectangle((sx + 12 + i * 19, sy + 129, sx + 22 + i * 19, sy + 132), fill=DEG[i])
    for n, (x, y) in KNOBS.items():                           # the knobs, ringed in their colour
        cx, cy = dev(x, y)
        d.ellipse((cx - 18, cy - 18, cx + 18, cy + 18), fill=(28, 28, 30), outline=KNOB_COL[n], width=3)
        d.line((cx, cy - 4, cx, cy - 16), fill=INK, width=3)
        d.text((cx, cy + 30), KNOB_CAP.get(n, n), fill=DIM, font=font(10, 600), anchor="mm")
    for n, (x, y) in BUTTONS.items():                         # the buttons: filled, or ringed, in their colour
        cx, cy = dev(x, y)
        w, h = (32, 22) if n.startswith("OCT") else (26, 20)
        col = BTN_COL[n]
        if n in SOLID:
            d.rounded_rectangle((cx - w, cy - h, cx + w, cy + h), 7, fill=col, outline=col, width=2)
            d.text((cx, cy), n, fill=INK if col in (RED, BLUE) else (20, 20, 22), font=font(11, 700), anchor="mm")
        else:
            d.rounded_rectangle((cx - w, cy - h, cx + w, cy + h), 7, fill=(36, 36, 40), outline=col, width=3)
            d.text((cx, cy), n, fill=col if col != WHITE else INK, font=font(11, 700), anchor="mm")
    ky = dev(100, 580)[1]                                     # the keys: 16 white, 11 black (labels later)
    for i in range(16):
        x = white_x(i)
        d.rounded_rectangle((x, ky, x + KW, ky + KH), 7, fill=KEY_W, outline=(120, 120, 126), width=1)
    for a in BLACK_AFTER:
        x = white_x(a) + KW - 11
        d.rounded_rectangle((x, ky - 4, x + 24, ky + 86), 5, fill=KEY_B, outline=(100, 100, 106), width=1)
    return ky


def card_size(d, title, body, sub, min_w=0):
    w = max(d.textlength(title, font=F_TITLE), d.textlength(body, font=F_BODY),
            d.textlength(sub, font=F_SUB) if sub else 0) + 22 + 14
    return int(max(w, min_w)), 64 if sub else 50


def card(d, x, y, title, body, col, sub=None, min_w=0, right=None):
    w, h = card_size(d, title, body, sub, min_w)
    if right is not None:
        x = right - w
    d.rounded_rectangle((x, y, x + w, y + h), 10, fill=CARD)
    d.rounded_rectangle((x + 8, y + 10, x + 14, y + h - 10), 3, fill=col)
    d.text((x + 22, y + 10), title, fill=DIM, font=F_TITLE)
    d.text((x + 22, y + 26), body, fill=INK, font=F_BODY)
    if sub:
        d.text((x + 22, y + 46), sub, fill=DIM, font=F_SUB)
    return (x, y, x + w, y + h)


ROUTES = []                                               # (points, dot): drawn in two passes, halos first


def route(pts, dot):
    ROUTES.append((pts, dot))


def draw_routes(d):
    for pts, dot in ROUTES:
        d.line(pts, fill=BG, width=6, joint="curve")
    for pts, dot in ROUTES:
        d.line(pts, fill=INK, width=2, joint="curve")
        d.ellipse((dot[0] - 4, dot[1] - 4, dot[0] + 4, dot[1] + 4), fill=INK, outline=BG, width=2)


def main(out):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((W / 2, 28), "FM-1 controls in fm1-chord", fill=INK, font=font(22, 600), anchor="mm")
    ky = draw_device(d)
    # left column: each card level with its control; the ALGORITHM one is reached over the PRESETS knob
    mx, my = dev(*KNOBS["MASTER"])
    px, py = dev(*KNOBS["PRESETS"])
    gx, gy = dev(*KNOBS["ALGORITHM"])
    ox, oy = dev(*BUTTONS["OCT-"])
    left = (("MASTER", "Volume", "the pot", WHITE, 126, [(mx, my)]),
            ("ALGORITHM", "Mode", "shows the list while it turns", RED, 200, [(gx, gy - 18), (gx, 232)]),
            ("PRESETS", "Sound", "shows the list while it turns", YELLOW, 272, [(px - 18, py)]),
            ("OCT−  OCT+", "Octave − / +", "in a menu: value − / +", WHITE, 344, [(ox - 32, oy)]))
    for t, b, s, col, y, pts in left:
        box = card(d, 16, y, t, b, col, s, min_w=190)
        route(pts + [(box[2], (box[1] + box[3]) / 2)], pts[0])
    # top: SELECT, the four knobs
    box = card(d, 270, 48, "SELECT", "Key · menu row · list choice", GREY)
    route([dev(*KNOBS["SELECT"]), ((box[0] + box[2]) / 2, box[3])], dev(*KNOBS["SELECT"]))
    kx4 = dev(*KNOBS["KNOB 4"])[0]
    box = card(d, 0, 48, "KNOB 1 – 4", "Filter · Resonance · Attack · Release",
               YELLOW, "the value shows as you turn; KNOB 4 on an effect row: its amount", right=kx4 + 70)
    for k in range(4):
        x, y = dev(*KNOBS[f"KNOB {k + 1}"])
        route([(x, y), (x, box[3])], (x, y))
    # right column: the top row of buttons, their lanes between the knob captions and the buttons
    right = (("FX", "SOUND menu", "sounds, effects, settings", YELLOW), ("SCL", "KEY menu", "key, scale, bass, voices", GREY),
             ("ENV", "ENVELOPE list", "LONG SHORT SWELL PLUCK …", YELLOW), ("LFO", "LFO list", "vibrato OFF LOW MED HIGH", YELLOW),
             ("EDIT", "MODE menu", "modes, tempo · 3 taps: tap tempo", RED), ("GLO", "SOUND menu", "the same as FX", YELLOW))
    for i, (n, b, s, col) in enumerate(right):
        y = 118 + i * 70
        box = card(d, 0, y, n, b, col, s, min_w=200, right=W - 10)
        ax, ay = dev(*BUTTONS[n])
        lane = DY + 100 + i * 7
        route([(ax, ay - 20), (ax, lane), (DX + DW - 6, lane), (box[0], (box[1] + box[3]) / 2)], (ax, ay - 20))
    # bottom row: HOME SAVE ARP SEQ PLAY REC, straight down through the keys to lanes under the device
    bottom = (("HOME", "Home", "held: Felucca's synth", WHITE), ("SAVE", "Presets P1–P4", "again: save", GREEN),
              ("ARP", "Arp mode", "on / off", RED), ("SEQ", "LOOPER screen", "layers, bars, metronome", BLUE),
              ("PLAY", "Play / pause", "looper, sequencer, loops", GREEN), ("REC", "Record a layer", "held: clear it", RED))
    sizes = [card_size(d, n, b, s, 140) for n, b, s, _ in bottom]
    gap, total = 10, sum(w for w, _ in sizes) + 10 * 5
    x = (W - total) / 2
    by = DY + DH + 66
    for i, ((n, b, s, col), (w, _)) in enumerate(zip(bottom, sizes)):
        box = card(d, int(x), by, n, b, col, s, min_w=140)
        ax, ay = dev(*BUTTONS[n])
        cx = (box[0] + box[2]) / 2
        lane = DY + DH + 12 + i * 7
        route([(ax, ay + 20), (ax, lane), (cx, lane), (cx, box[1])], (ax, ay + 20))
        x += w + gap
    draw_routes(d)
    key_labels(d, ky)
    # the keys' legend, under the keys
    ly = by + 80
    d.rounded_rectangle((16, ly, W - 16, H - 12), 12, fill=CARD)
    f15, f13 = font(15, 600), font(13, 450)
    d.text((34, ly + 12), "WHITE KEYS", fill=DIM, font=F_TITLE)
    d.text((34, ly + 28), "The seven at the left play chords: the scale degrees 1–7 (I, ii … vii).  The nine at the right: the "
                          "strumplate, one note of the chord each, rising — swipe to strum", fill=INK, font=f15)
    d.text((34, ly + 50), "BLACK KEYS", fill=DIM, font=F_TITLE)
    d.text((34, ly + 66), "C#4 ↑  D#4 ↗  F#4 →  G#4 ↘  A#4 ↓  C#5 ↙  D#5 ←  F#5 ↖  the HiChord's joystick, held with a chord;  "
                          "F#3 INVERT  G#3 LOCK  A#3 HOLD", fill=INK, font=f15)
    d.text((34, ly + 90), "Colours: yellow sound · grey key · red mode · green presets and play · blue looper.  "
                          "OCT− and OCT+ together: RANDOMIZE.  SAVE with HOME held: back from Felucca's synth.", fill=DIM, font=f13)
    im.save(out)
    print(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "controls.png")
