#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 fm1-chord contributors
"""The fm1-chord controls diagram (screenshots/controls.png): a drawn schematic of the FM-1's panel (the
geometry of Felucca's docs/panel.jpg: 8 knobs, 12 buttons in two rows, OCT- / OCT+, 27 keys) with every
control labelled by what it does in the HiChord UI, in the HiChord's colours. Pillow only.

  gen_controls_diagram.py OUT.png
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets/fonts/InterTight[wght].ttf"
W, H = 1200, 960
BG, CARD, INK, DIM = (30, 30, 32), (70, 70, 74), (255, 255, 255), (170, 170, 176)
GREY, YELLOW, RED, GREEN, BLUE = (150, 152, 160), (245, 196, 0), (226, 62, 62), (70, 200, 120), (80, 150, 255)
DEG = [(255, 92, 92), (255, 160, 48), (250, 220, 70), (96, 214, 120), (72, 200, 236), (120, 128, 255), (206, 112, 240)]


def font(size, weight=500):
    f = ImageFont.truetype(str(FONT), size)
    try:
        f.set_variation_by_axes([weight])
    except Exception:
        pass
    return f


# the device, drawn: a body of 760 x 420 at (220, 110); positions follow docs/panel.jpg
DX, DY, DW, DH = 220, 110, 760, 430


def dev(x, y):                       # photo pixels (1750 x 1050, crop 70..1680 x 60..960) -> canvas
    return DX + (x - 70) * DW / 1610, DY + (y - 60) * DH / 900


KNOBS = {"MASTER": (175, 178), "SELECT": (355, 178), "PRESETS": (175, 350), "ALGORITHM": (355, 350),
         "KNOB 1": (978, 178), "KNOB 2": (1163, 178), "KNOB 3": (1348, 178), "KNOB 4": (1533, 178)}
BTN_X = (997, 1105, 1207, 1315, 1417, 1525)
BUTTONS = {}
for row, y, names in ((0, 352, "FX SCL ENV LFO EDIT GLO"), (1, 454, "HOME SAVE ARP SEQ PLAY REC")):
    for x, n in zip(BTN_X, names.split()):
        BUTTONS[n] = (x, y)
BUTTONS["OCT-"], BUTTONS["OCT+"] = (198, 487), (328, 487)
BTN_COL = {"SCL": GREY, "FX": YELLOW, "EDIT": RED, "SAVE": GREEN, "SEQ": BLUE, "PLAY": GREEN, "REC": RED}


def draw_device(d):
    d.rounded_rectangle((DX, DY, DX + DW, DY + DH), 26, fill=(58, 58, 62), outline=(90, 90, 96), width=2)
    # the screen
    sx, sy = dev(490, 140)
    d.rounded_rectangle((sx, sy, sx + 150, sy + 150), 8, fill=(16, 18, 24), outline=(110, 110, 116), width=2)
    d.rounded_rectangle((sx + 6, sy + 6, sx + 68, sy + 26), 4, fill=GREY)
    d.rounded_rectangle((sx + 72, sy + 6, sx + 118, sy + 26), 4, fill=RED)
    d.text((sx + 75, sy + 36), "C7", fill=DEG[0], font=font(34, 700), anchor="mm")
    for i in range(7):
        d.rounded_rectangle((sx + 10 + i * 19, sy + 110, sx + 24 + i * 19, sy + 134), 3, fill=(38, 40, 48))
        d.rectangle((sx + 12 + i * 19, sy + 129, sx + 22 + i * 19, sy + 132), fill=DEG[i])
    # the knobs
    for n, (x, y) in KNOBS.items():
        cx, cy = dev(x, y)
        d.ellipse((cx - 18, cy - 18, cx + 18, cy + 18), fill=(28, 28, 30), outline=(120, 120, 126), width=2)
        d.line((cx, cy - 4, cx, cy - 16), fill=INK, width=3)
        d.text((cx, cy + 30), n, fill=DIM, font=font(10, 600), anchor="mm")
    # the buttons
    for n, (x, y) in BUTTONS.items():
        cx, cy = dev(x, y)
        w, h = (32, 22) if n.startswith("OCT") else (26, 20)
        col = BTN_COL.get(n, (40, 40, 44))
        d.rounded_rectangle((cx - w, cy - h, cx + w, cy + h), 7, fill=col if n in BTN_COL else (36, 36, 40),
                            outline=(110, 110, 116), width=2)
        d.text((cx, cy), n, fill=INK if n in ("EDIT", "REC") else (20, 20, 22) if n in BTN_COL else DIM,
               font=font(11, 700), anchor="mm")
    # the keys: 16 white, 11 black
    kx0, ky = dev(100, 580)
    kw, kh = 40.5, 150
    whites = []
    for i in range(16):
        x = kx0 + i * (kw + 2)
        deg = (i + 3) % 7
        d.rounded_rectangle((x, ky, x + kw, ky + kh), 7, fill=(236, 236, 240), outline=(120, 120, 126), width=1)
        d.rectangle((x + 6, ky + kh - 14, x + kw - 6, ky + kh - 8), fill=DEG[deg])
        whites.append(x)
    after = (0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14)
    fn = ("INV", "LOCK", "HOLD", "↑", "↗", "→", "↘", "↓", "↙", "←", "↖")
    for p, a in enumerate(after):
        x = whites[a] + kw - 11
        col = GREEN if p < 3 else YELLOW
        d.rounded_rectangle((x, ky - 4, x + 24, ky + 86), 5, fill=(22, 22, 26), outline=(100, 100, 106), width=1)
        d.text((x + 12, ky + 70), fn[p], fill=col, font=font(10 if p < 3 else 15, 700), anchor="mm")
    return whites, kx0, ky, kw, kh


def card(d, x, y, w, h, title, body, col=None, sub=None):
    d.rounded_rectangle((x, y, x + w, y + h), 10, fill=CARD)
    if col:
        d.rounded_rectangle((x + 8, y + 10, x + 14, y + h - 10), 3, fill=col)
    d.text((x + 22, y + 10), title, fill=DIM, font=font(11, 600))
    d.text((x + 22, y + 26), body, fill=INK, font=font(15, 600))
    if sub:
        d.text((x + 22, y + 46), sub, fill=DIM, font=font(12, 450))
    return (x, y, x + w, y + h)


def leader(d, p0, p1, mid=None):
    pts = [p0] + ([mid] if mid else []) + [p1]
    d.line(pts, fill=BG, width=5, joint="curve")
    d.line(pts, fill=INK, width=2, joint="curve")
    d.ellipse((p0[0] - 4, p0[1] - 4, p0[0] + 4, p0[1] + 4), fill=INK, outline=BG, width=2)


def main(out):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((W / 2, 28), "FM-1 controls in fm1-chord", fill=INK, font=font(22, 600), anchor="mm")
    whites, kx0, ky, kw, kh = draw_device(d)
    # left column
    left = (("MASTER", "Volume", None, None), ("PRESETS", "Sound", "turn: the next sound", None),
            ("ALGORITHM", "Mode", "Play, Strum, Lead, Drone, Arp …", None), ("OCT−", "Octave −", "a menu: value −", None),
            ("OCT+", "Octave +", "a menu: value +, load", None))
    anchors = ("MASTER", "PRESETS", "ALGORITHM", "OCT-", "OCT+")
    for i, ((t, b, s, _), a) in enumerate(zip(left, anchors)):
        y = 118 + i * 84
        box = card(d, 16, y, 190, 64, t, b, None, s)
        src = KNOBS.get(a) or BUTTONS[a]
        leader(d, dev(*src), (box[2], (box[1] + box[3]) / 2), (DX + 6, dev(*src)[1]) if a.startswith("OCT") else None)
    # top
    box = card(d, 290, 52, 150, 50, "SELECT", "Key · menu row")
    leader(d, dev(*KNOBS["SELECT"]), ((box[0] + box[2]) / 2, box[3]))
    kx1, kx4 = dev(*KNOBS["KNOB 1"])[0], dev(*KNOBS["KNOB 4"])[0]
    box = card(d, kx1 - 50, 52, kx4 - kx1 + 100, 50, "KNOB 1 – 4", "Filter · Attack · Release · Tempo")
    for k in range(4):
        x = dev(*KNOBS[f"KNOB {k + 1}"])[0]
        leader(d, (x, dev(*KNOBS["KNOB 1"])[1]), (x, box[3]))
    # right column: the top row of buttons
    right = (("FX", "SOUND menu", "sounds, effects, settings", YELLOW), ("SCL", "KEY menu", "key, scale, chords", GREY),
             ("ENV", "Envelope preset", "LONG SHORT SWELL PLUCK …", None), ("LFO", "Vibrato", "OFF LOW MED HIGH", None),
             ("EDIT", "MODE menu", "modes, tempo; 3 taps: tap tempo", RED), ("GLO", "Settings", "(the SOUND menu)", None))
    for i, (n, b, s, col) in enumerate(right):
        y = 118 + i * 70
        box = card(d, 994, y, 196, 62, n, b, col, s)
        ax, ay = dev(*BUTTONS[n])
        lane = DY + 102 + i * 7
        pts = [(ax, ay - 20), (ax, lane), (DX + DW - 6, lane), (box[0], (box[1] + box[3]) / 2)]
        d.line(pts, fill=BG, width=5, joint="curve")
        d.line(pts, fill=INK, width=2, joint="curve")
        d.ellipse((ax - 4, ay - 24, ax + 4, ay - 16), fill=INK, outline=BG, width=2)
    # bottom row: HOME SAVE ARP SEQ PLAY REC
    bottom = (("HOME", "Home", "held: Felucca's synth", None), ("SAVE", "Presets P1–P4", "again: save", GREEN),
              ("ARP", "Arp mode", "on / off", None), ("SEQ", "LOOPER screen", "layers, bars, metronome", BLUE),
              ("PLAY", "Play / pause", "looper, sequencer, loops", GREEN), ("REC", "Record a layer", "held: clear it", RED))
    by = DY + DH + 150
    for i, (n, b, s, col) in enumerate(bottom):
        x = 230 + i * 128
        box = card(d, x, by, 120, 64, n, b, col, s)
        ax, ay = dev(*BUTTONS[n])
        cx = (box[0] + box[2]) / 2
        lane = DY + 212 + i * 7
        pts = [(ax, ay + 20), (ax, lane), (cx, lane), (cx, box[1])]
        d.line(pts, fill=BG, width=5, joint="curve")
        d.line(pts, fill=INK, width=2, joint="curve")
        d.ellipse((ax - 4, ay + 16, ax + 4, ay + 24), fill=INK, outline=BG, width=2)
    # the keys' legend, under the keys
    ly = by + 80
    d.rounded_rectangle((16, ly, W - 16, H - 12), 12, fill=CARD)
    f15, f13 = font(15, 600), font(13, 450)
    d.text((34, ly + 12), "WHITE KEYS", fill=DIM, font=font(11, 600))
    d.text((34, ly + 28), "Scale degrees: C4 is I, D4 ii, E4 iii, F4 IV, G4 V, A4 vi, B4 vii; F3–B3 an octave down, C5–G5 up",
           fill=INK, font=f15)
    d.text((34, ly + 50), "BLACK KEYS", fill=DIM, font=font(11, 600))
    d.text((34, ly + 66), "C#4 ↑  D#4 ↗  F#4 →  G#4 ↘  A#4 ↓  C#5 ↙  D#5 ←  F#5 ↖  the HiChord's joystick, held with a chord;  "
                          "F#3 INVERT  G#3 LOCK  A#3 HOLD", fill=INK, font=f15)
    d.text((34, ly + 90), "OCT− and OCT+ together: RANDOMIZE (all on the KEY menu, the sound on SOUND, the pattern on MODE).  "
                          "SAVE with HOME held: back from Felucca's synth.", fill=DIM, font=f13)
    im.save(out)
    print(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "controls.png")
