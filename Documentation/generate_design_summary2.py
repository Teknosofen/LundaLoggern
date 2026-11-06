"""Build LundaLoggern_Design_Summary2.docx from DesignSummary.md.

Redrawn figures (readable at page width), styled tables, code blocks and callouts,
cover block with a table of contents. If `comtypes` and MS Word are available, the
TOC is refreshed through Word automation; otherwise Word shows a placeholder until
the user right-clicks the TOC and chooses "Update field".

Usage:  python generate_design_summary2.py
"""
from __future__ import annotations

import math
import os
import re
import sys
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor, Emu
from markdown_it import MarkdownIt

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "DesignSummary.md"
OUT = ROOT / "LundaLoggern_Design_Summary2.docx"

# ---------------------------------------------------------------- palette
INK = "#1B2B34"
MUTED = "#5B6E77"
TEAL = "#0B7A80"
BLUE = "#2A66A0"
GREEN = "#3F7D52"
AMBER = "#B26A1E"
GREY = "#7A8A91"
LINE = "#4F656F"
BG = "#FFFFFF"


def rgb(h: str) -> RGBColor:
    return RGBColor.from_string(h.lstrip("#"))


# ================================================================ diagrams
S = 2  # supersampling


def font(size: float, bold: bool = False):
    name = "segoeuib.ttf" if bold else "segoeui.ttf"
    try:
        return ImageFont.truetype(str(Path("C:/Windows/Fonts") / name), int(size * S))
    except OSError:
        return ImageFont.load_default()


def mono(size: float):
    try:
        return ImageFont.truetype("C:/Windows/Fonts/consola.ttf", int(size * S))
    except OSError:
        return font(size)


class Canvas:
    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self.img = Image.new("RGB", (w * S, h * S), BG)
        self.d = ImageDraw.Draw(self.img)

    # -- primitives
    def _dashed(self, a, b, color, width, dash=10, gap=7):
        (x1, y1), (x2, y2) = a, b
        length = math.hypot(x2 - x1, y2 - y1)
        if not length:
            return
        dx, dy = (x2 - x1) / length, (y2 - y1) / length
        pos = 0.0
        while pos < length:
            end = min(pos + dash * S, length)
            self.d.line([(x1 + dx * pos, y1 + dy * pos), (x1 + dx * end, y1 + dy * end)],
                        fill=color, width=width)
            pos = end + gap * S

    def arrow(self, pts, label="", at=None, dashed=False, color=LINE, both=False,
              label_size=17, anchor="mm"):
        p = [(x * S, y * S) for x, y in pts]
        width = int(2.5 * S)
        for a, b in zip(p, p[1:]):
            if dashed:
                self._dashed(a, b, color, width)
            else:
                self.d.line([a, b], fill=color, width=width)
        self._head(p[-2], p[-1], color)
        if both:
            self._head(p[1], p[0], color)
        if label:
            self.label(at, label, label_size, anchor)

    def _head(self, a, b, color):
        ang = math.atan2(b[1] - a[1], b[0] - a[0])
        ln, wg = 13 * S, 6.5 * S
        bx, by = b[0] - ln * math.cos(ang), b[1] - ln * math.sin(ang)
        self.d.polygon([b, (bx + wg * math.sin(ang), by - wg * math.cos(ang)),
                        (bx - wg * math.sin(ang), by + wg * math.cos(ang))], fill=color)

    def label(self, at, txt, size=17, anchor="mm", color=MUTED, bold=True, pad=4):
        f = font(size, bold)
        x, y = at[0] * S, at[1] * S
        box = self.d.textbbox((x, y), txt, font=f, anchor=anchor)
        self.d.rectangle((box[0] - pad * S, box[1] - pad * S, box[2] + pad * S, box[3] + pad * S), fill=BG)
        self.d.text((x, y), txt, font=f, fill=color, anchor=anchor)

    def box(self, x, y, w, h, title, lines=(), color=TEAL, dashed=False, body_size=17,
            title_size=20, mono_lines=False):
        X, Y, W, H = x * S, y * S, w * S, h * S
        r = 10 * S
        hh = 40 * S
        self.d.rounded_rectangle((X, Y, X + W, Y + H), radius=r, fill="#FFFFFF",
                                 outline=color, width=int(2.5 * S))
        if dashed:
            # overpaint outline with dashes for a "not wired" look
            self.d.rounded_rectangle((X, Y, X + W, Y + H), radius=r, fill="#FFFFFF",
                                     outline="#FFFFFF", width=int(2.5 * S))
            for a, b in [((X + r, Y), (X + W - r, Y)), ((X + W, Y + r), (X + W, Y + H - r)),
                         ((X + W - r, Y + H), (X + r, Y + H)), ((X, Y + H - r), (X, Y + r))]:
                self._dashed(a, b, color, int(2.5 * S))
            fill = "#EEF1F2"
            self.d.rounded_rectangle((X + 2 * S, Y + 2 * S, X + W - 2 * S, Y + hh), radius=r - 2 * S, fill=fill)
            self.d.rectangle((X + 2 * S, Y + hh - r, X + W - 2 * S, Y + hh), fill=fill)
            tcolor = color
        else:
            self.d.rounded_rectangle((X, Y, X + W, Y + hh), radius=r, fill=color)
            self.d.rectangle((X, Y + hh - r, X + W, Y + hh), fill=color)
            tcolor = "#FFFFFF"
        self.d.text((X + W / 2, Y + hh / 2), title, font=font(title_size, True), fill=tcolor, anchor="mm")
        f = mono(body_size) if mono_lines else font(body_size)
        line_h = body_size * 1.45
        for i, ln in enumerate(lines):
            if mono_lines:
                self.d.text((X + 16 * S, Y + hh + (12 + i * line_h) * S), ln, font=f, fill=INK)
            else:
                self.d.text((X + W / 2, Y + hh + (12 + line_h / 2 + i * line_h) * S), ln,
                            font=f, fill=INK, anchor="mm")

    def note(self, x, y, txt, size=15):
        self.d.text((x * S, y * S), txt, font=font(size), fill=MUTED)

    def png(self) -> BytesIO:
        out = BytesIO()
        self.img.save(out, format="PNG", dpi=(300, 300))
        out.seek(0)
        return out


def fig_pins() -> BytesIO:
    c = Canvas(1320, 590)
    # boxes
    c.box(20, 150, 190, 110, "SERVO-u", ["ventilator", "RS232 port"], GREY)
    c.box(300, 120, 230, 170, "RS232 transceiver", ["e.g. MAX3221", "line side ↔ logic side", "3.3 V logic"], AMBER)
    c.box(690, 40, 280, 370, "T-Display S3", [
        "ESP32-S3", "",
        "GPIO17  UART2 TXD2", "GPIO16  UART2 RXD2", "",
        "GPIO11  HSPI MOSI", "GPIO13  HSPI SCLK", "GPIO10  HSPI CS", "GPIO12  HSPI MISO", "",
        "GPIO14  Button"], BLUE, body_size=16, mono_lines=True)
    c.box(1120, 150, 190, 220, "MicroSD reader", ["DI / MOSI", "CLK / SCK", "CS", "DO / MISO", "3.3 V logic"], GREEN)
    c.box(740, 480, 180, 90, "Button", ["user input"], AMBER)
    # ventilator <-> transceiver
    c.arrow([(210, 190), (300, 190)], "TX", (255, 172), label_size=15)
    c.arrow([(300, 225), (210, 225)], "RX", (255, 244), label_size=15)
    # transceiver <-> board
    c.arrow([(690, 175), (530, 175)], "GPIO17 → DI", (610, 157))
    c.arrow([(530, 240), (690, 240)], "RO → GPIO16", (610, 222))
    # board <-> SD
    for y, lbl, right in [(200, "GPIO11 MOSI", True), (245, "GPIO13 SCLK", True),
                          (290, "GPIO10 CS", True), (335, "GPIO12 MISO", False)]:
        pts = [(970, y), (1120, y)] if right else [(1120, y), (970, y)]
        c.arrow(pts, lbl, (1045, y - 17), label_size=15)
    c.arrow([(830, 480), (830, 410)], "GPIO14", (880, 445), label_size=15)
    c.note(20, 330, "UART2: 38 400 baud, 8E1.")
    c.note(20, 355, "Never wire ESP32 GPIO directly")
    c.note(20, 378, "to RS232 voltage levels.")
    c.note(20, 425, "SD on dedicated HSPI bus,")
    c.note(20, 448, "shared GND, 3.3 V logic.")
    return c.png()


def fig_classes() -> BytesIO:
    c = Canvas(1420, 680)
    xs = [60, 330, 600, 870, 1140]
    w, y1, h1 = 220, 215, 125
    y2, h2 = 455, 125
    c.box(560, 30, 300, 100, "main.cpp", ["setup() / loop()", "owns global objects"], BLUE)
    # bus from main to row 1
    c.d.line([(710 * S, 130 * S), (710 * S, 172 * S)], fill=LINE, width=int(2.5 * S))
    c.d.line([((xs[0] + w / 2) * S, 172 * S), ((xs[-1] + w / 2) * S, 172 * S)], fill=LINE, width=int(2.5 * S))
    for x in xs:
        c.arrow([(x + w / 2, 172), (x + w / 2, y1)])
    c.label((420, 150), "creates, services and polls", 16)
    rows = [
        ("Button", ["debounce", "short / long press"], AMBER),
        ("ImageRenderer", ["TFT status screen", "clock, SD, COM, phase"], GREEN),
        ("ServoCIEData", ["CIE state machine", "channel scaling"], TEAL),
        ("HostCli", ["USB-CDC commands", "opt-in data streams"], AMBER),
        ("WifiApServer", ["SoftAP + HTTP", "dashboard, file manager"], BLUE),
    ]
    for x, (t, ln, col) in zip(xs, rows):
        c.box(x, y1, w, h1, t, ln, col, body_size=16)
    # row 2
    c.box(60, y2, 220, h2, "QRHandler", ["implemented,", "not wired into loop"], GREY, dashed=True, body_size=16)
    c.box(330, y2, 240, h2, "DateTime", ["RTC value object", "timestamps"], AMBER, body_size=16)
    c.box(700, y2, 290, h2, "SDManager", ["mount, card status", "rolling 12 h log files"], GREEN, body_size=16)
    # HostCli -> ServoCIEData
    c.arrow([(870, 300), (820, 300)], "", None)
    c.label((845, 278), "reads", 14)
    # ImageRenderer -> DateTime
    c.arrow([(450, y1 + h1), (450, y2)], "renders", (450, 397), dashed=True, label_size=15)
    # ServoCIEData -> SDManager
    c.arrow([(760, y1 + h1), (760, y2)], "writes rows", (760, 397), label_size=15)
    # ServoCIEData -> DateTime (RTC)
    c.arrow([(630, y1 + h1), (630, 545), (570, 545)], "sets RTC", (630, 397), dashed=True, label_size=15)
    # HostCli -> SDManager
    c.arrow([(940, y1 + h1), (940, y2)], "LIST / DUMP", (940, 397), label_size=15)
    # WifiApServer -> SDManager
    c.arrow([(1250, y1 + h1), (1250, 520), (990, 520)], "SD downloads / delete", (1190, 397), label_size=15)
    # SDManager -> DateTime
    c.arrow([(700, 490), (570, 490)], "", None)
    c.label((672, 472), "time", 14)
    c.note(60, 615, "Solid arrow = calls / uses at runtime.   Dashed arrow = reads or sets time.   "
                    "Dashed box = present in source but not instantiated.", 15)
    return c.png()


def fig_flow() -> BytesIO:
    c = Canvas(1300, 760)
    c.box(20, 200, 190, 110, "SERVO-u", ["ventilator"], GREY)
    c.box(300, 185, 230, 140, "ServoCIEData", ["byte-wise CIE", "parser + scaling"], TEAL)
    c.arrow([(210, 255), (300, 255)], "RS232", (255, 237), label_size=15)
    # spine
    sx = 590
    c.d.line([(530 * S, 255 * S), (sx * S, 255 * S)], fill=LINE, width=int(2.5 * S))
    c.d.line([(sx * S, 105 * S), (sx * S, 495 * S)], fill=LINE, width=int(2.5 * S))
    lanes = [
        (50, "SDManager", ["metrics_ / settings_ /", "alarms_*.txt on SD"], GREEN,
         "WifiApServer", ["HTTP file manager", "→ browser"], BLUE, "M · S · A rows", "SD files"),
        (245, "HostCli", ["tags rows, applies", "curve decimation"], AMBER,
         "Host PC", ["USB-CDC 115 200", "opt-in streams"], GREY, "M · S · A · C", "USB"),
        (440, "main.cpp loop", ["~200 ms supervision", "timeouts, retries"], BLUE,
         "ImageRenderer", ["TFT: ID, phase,", "SD / COM, clock"], GREEN, "ID, breath phase", "redraw"),
    ]
    for y, t1, l1, c1, t2, l2, c2, lab1, lab2 in lanes:
        cy = y + 55
        c.box(760, y, 240, 110, t1, l1, c1, body_size=16)
        c.box(1060, y, 220, 110, t2, l2, c2, body_size=16)
        c.arrow([(sx, cy), (760, cy)], lab1, (675, cy - 18), label_size=15)
        c.arrow([(1000, cy), (1060, cy)], "", None)
        c.label((1030, cy - 22), lab2, 13)
    # DateTime & Button
    c.box(300, 600, 230, 100, "DateTime / RTC", ["seeded by CIE RTIM"], AMBER, body_size=16)
    c.arrow([(415, 325), (415, 600)], "RTIM time", (415, 400), dashed=True, label_size=15)
    c.arrow([(530, 650), (820, 650), (820, 550)], "timestamps", (680, 632), dashed=True, label_size=15)
    c.box(1060, 600, 220, 100, "Button", ["long press: WiFi on", "short press: WiFi off"], AMBER, body_size=16)
    c.arrow([(1060, 650), (940, 650), (940, 550)], "events", (1000, 632), label_size=15)
    c.note(20, 725, "Curves are streamed over USB only (after a complete sample set); they are not written to SD. "
                    "All USB streams and WiFi are off at boot.", 15)
    return c.png()


PINOUT_SRC = ROOT / "Lilygo-T-display_pinlayout.webp"
PINOUT_OUT = ROOT / "LundaLoggern_TDisplayS3_Pinout_Annotated.png"


def make_annotated_pinout() -> Path:
    """Overlay LundaLoggern's pin usage on LilyGo's T-Display S3 pinout drawing.

    The vendor image (1024 x 1024) is only overlaid, never redrawn. Row
    coordinates below were measured on the vendor image:
      left header  GPIO10/11/12/13 at y = 342 / 367 / 391 / 416
      right header GPIO17 / GPIO16 at y = 367 / 415
      IO14 key (bottom right of the LCD drawing) at x 153-183, y 827-843
    """
    K = 2                                   # work at 2x for crisp overlay text
    base = Image.open(PINOUT_SRC).convert("RGB")
    W, H = base.size
    band_h = 118                            # notes band below the vendor image
    canvas = Image.new("RGB", (W * K, (H + band_h) * K), "#FFFFFF")
    canvas.paste(base.resize((W * K, H * K), Image.LANCZOS), (0, 0))
    over = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(over)

    def hexa(h, a=255):
        h = h.lstrip("#")
        return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) + (a,)

    def f(size, bold=True):
        return font(size * K / S, bold)     # font() scales by S; undo that

    def band(x1, x2, y, col):
        d.rounded_rectangle((x1 * K, (y - 12) * K, x2 * K, (y + 12) * K), radius=6 * K,
                            fill=hexa(col, 60), outline=hexa(col), width=2 * K)

    def line(pts, col):
        d.line([(x * K, y * K) for x, y in pts], fill=hexa(col), width=3 * K, joint="curve")

    def tag(x, y, w, title, rows, col, size=16):
        lh = size + 7
        h = 10 + (size + 4) + 6 + len(rows) * lh + 6
        d.rounded_rectangle((x * K, y * K, (x + w) * K, (y + h) * K), radius=7 * K,
                            fill=hexa("#FFFFFF", 240), outline=hexa(col), width=3 * K)
        d.rounded_rectangle((x * K, y * K, (x + w) * K, (y + size + 14) * K), radius=7 * K, fill=hexa(col))
        d.rectangle((x * K, (y + size + 4) * K, (x + w) * K, (y + size + 14) * K), fill=hexa(col))
        d.text(((x + 8) * K, (y + 5) * K), title, font=f(size), fill=hexa("#FFFFFF"))
        for i, r in enumerate(rows):
            d.text(((x + 8) * K, (y + size + 20 + i * lh) * K), r, font=f(size - 1), fill=hexa(INK))
        return h

    # --- SD card reader on HSPI (left header) --------------------------------
    sd_rows = [(342, "GPIO10  → SD CS"), (367, "GPIO11  → SD MOSI"),
               (391, "GPIO12  ← SD MISO"), (416, "GPIO13  → SD SCLK")]
    for y, _ in sd_rows:
        band(8, 414, y, GREEN)
        line([(8, y), (3, y)], GREEN)
    line([(3, 416), (3, 214)], GREEN)
    tag(3, 96, 250, "MicroSD reader (HSPI)", [r for _, r in sd_rows], GREEN)

    # --- RS232 transceiver on UART2 (right header) ---------------------------
    uart_rows = [(367, "GPIO17 TXD2 → DI"), (415, "GPIO16 RXD2 ← RO")]
    for y, _ in uart_rows:
        band(610, 1014, y, AMBER)
        line([(1014, y), (1019, y)], AMBER)
    line([(1019, 367), (1019, 452)], AMBER)
    tag(744, 452, 278, "RS232 transceiver (UART2)", [r for _, r in uart_rows], AMBER)

    # --- On-board IO14 key ----------------------------------------------------
    d.rounded_rectangle((140 * K, 819 * K, 194 * K, 872 * K), radius=6 * K,
                        outline=hexa(BLUE), width=3 * K)
    th = tag(52, 662, 136, "GPIO14 key", ["long: WiFi on", "short: WiFi off"], BLUE, size=15)
    line([(167, 662 + th), (167, 819)], BLUE)

    canvas = Image.alpha_composite(canvas.convert("RGBA"), over).convert("RGB")

    # --- Notes band -------------------------------------------------------------
    dn = ImageDraw.Draw(canvas)
    y0 = H + 12
    dn.line((12 * K, (y0 - 6) * K, (W - 12) * K, (y0 - 6) * K), fill=hexa("#C9D3D7")[:3], width=2 * K)
    chips = [(GREEN, "SD card (HSPI)"), (AMBER, "RS232 / UART2"), (BLUE, "user button")]
    x = 14
    for col, txt in chips:
        dn.rounded_rectangle((x * K, (y0 + 4) * K, (x + 18) * K, (y0 + 20) * K), radius=3 * K, fill=col)
        dn.text(((x + 26) * K, (y0 + 1) * K), txt, font=f(16), fill=INK)
        x += 26 + int(dn.textlength(txt, font=f(16)) / K) + 30
    notes = [
        "Highlighted rows are the pins LundaLoggern uses. Pin positions as in LilyGo's drawing.",
        "HSPI is remapped by build flags (CS 10, MOSI 11, MISO 12, SCLK 13), unlike LilyGo's SPI labels.",
        "LilyGo marks GPIO18/17 as UART1 (U1 RXD/TXD); LundaLoggern runs UART2 on GPIO16 (RX) / GPIO17 (TX).",
    ]
    for i, n in enumerate(notes):
        dn.text((14 * K, (y0 + 30 + i * 24) * K), n, font=f(15, False), fill=MUTED)
    canvas.save(PINOUT_OUT, dpi=(300, 300))
    return PINOUT_OUT


# ================================================================ docx helpers
BODY_FONT = "Calibri"
HEAD_FONT = "Calibri"
CODE_FONT = "Consolas"


def set_cell_shading(cell, fill: str):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill.lstrip("#"))
    tcPr.append(shd)


def set_cell_margins(cell, top=60, bottom=60, left=100, right=100):
    tcPr = cell._tc.get_or_add_tcPr()
    mar = OxmlElement("w:tcMar")
    for side, val in (("top", top), ("bottom", bottom), ("start", left), ("end", right)):
        el = OxmlElement(f"w:{side}")
        el.set(qn("w:w"), str(val))
        el.set(qn("w:type"), "dxa")
        mar.append(el)
    tcPr.append(mar)


def set_table_borders(table, color="C9D3D7", size=4, inside_v=False, left_accent=None):
    tblPr = table._tbl.tblPr
    borders = OxmlElement("w:tblBorders")
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        el = OxmlElement(f"w:{edge}")
        if edge == "insideV" and not inside_v:
            el.set(qn("w:val"), "nil")
        elif edge == "left" and left_accent:
            el.set(qn("w:val"), "single")
            el.set(qn("w:sz"), "24")
            el.set(qn("w:color"), left_accent.lstrip("#"))
        elif edge in ("left", "right") and not inside_v:
            el.set(qn("w:val"), "nil")
        else:
            el.set(qn("w:val"), "single")
            el.set(qn("w:sz"), str(size))
            el.set(qn("w:color"), color)
        borders.append(el)
    tblPr.append(borders)


def set_col_widths(table, widths_cm):
    tbl = table._tbl
    grid = tbl.tblGrid
    for gc, wcm in zip(grid.findall(qn("w:gridCol")), widths_cm):
        gc.set(qn("w:w"), str(int(Cm(wcm).twips)))
    for row in table.rows:
        for cell, wcm in zip(row.cells, widths_cm):
            cell.width = Cm(wcm)
    # fixed layout so Word honours the widths
    layout = OxmlElement("w:tblLayout")
    layout.set(qn("w:type"), "fixed")
    tbl.tblPr.append(layout)


def repeat_header(row):
    trPr = row._tr.get_or_add_trPr()
    el = OxmlElement("w:tblHeader")
    el.set(qn("w:val"), "true")
    trPr.append(el)


def no_split(row):
    trPr = row._tr.get_or_add_trPr()
    trPr.append(OxmlElement("w:cantSplit"))


def para_border_bottom(paragraph, color="0B7A80", size=8, space=4):
    pPr = paragraph._p.get_or_add_pPr()
    pbdr = OxmlElement("w:pBdr")
    b = OxmlElement("w:bottom")
    b.set(qn("w:val"), "single")
    b.set(qn("w:sz"), str(size))
    b.set(qn("w:space"), str(space))
    b.set(qn("w:color"), color)
    pbdr.append(b)
    pPr.append(pbdr)


def run_shading(run, fill="EEF2F3"):
    rPr = run._r.get_or_add_rPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)
    rPr.append(shd)


def add_field(paragraph, instr: str, placeholder: str = ""):
    r = paragraph.add_run()
    fc = OxmlElement("w:fldChar"); fc.set(qn("w:fldCharType"), "begin"); r._r.append(fc)
    r2 = paragraph.add_run()
    it = OxmlElement("w:instrText"); it.set(qn("xml:space"), "preserve"); it.text = instr; r2._r.append(it)
    r3 = paragraph.add_run()
    fc = OxmlElement("w:fldChar"); fc.set(qn("w:fldCharType"), "separate"); r3._r.append(fc)
    r4 = paragraph.add_run(placeholder)
    r5 = paragraph.add_run()
    fc = OxmlElement("w:fldChar"); fc.set(qn("w:fldCharType"), "end"); r5._r.append(fc)
    return r4


def style_code_run(run, size=9):
    run.font.name = CODE_FONT
    run._r.get_or_add_rPr().get_or_add_rFonts().set(qn("w:cs"), CODE_FONT)
    run.font.size = Pt(size)
    run.font.color.rgb = rgb("#0A5F64")


# ================================================================ builder
class Builder:
    CONTENT_W = 17.0  # cm (A4 21 cm - 2 x 2 cm)

    def __init__(self):
        self.doc = Document()
        self.fig_no = 0
        self.tab_no = 0
        self._setup()

    # ---- page & styles
    def _setup(self):
        d = self.doc
        sec = d.sections[0]
        sec.page_width, sec.page_height = Cm(21), Cm(29.7)
        sec.left_margin = sec.right_margin = Cm(2)
        sec.top_margin, sec.bottom_margin = Cm(2.2), Cm(2)
        sec.header_distance = sec.footer_distance = Cm(1)

        st = d.styles
        normal = st["Normal"]
        normal.font.name = BODY_FONT
        normal.element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT)
        normal.font.size = Pt(10.5)
        normal.font.color.rgb = rgb(INK)
        pf = normal.paragraph_format
        pf.space_before, pf.space_after = Pt(0), Pt(6)
        pf.line_spacing = 1.12

        spec = {
            "Title": (28, TEAL, 0, 4),
            "Heading 1": (17, TEAL, 20, 8),
            "Heading 2": (13.5, BLUE, 14, 5),
            "Heading 3": (11.5, INK, 12, 4),
            "Heading 4": (10.5, MUTED, 10, 3),
        }
        for name, (size, color, before, after) in spec.items():
            s = st[name]
            s.font.name = HEAD_FONT
            rpr = s.element.get_or_add_rPr()
            rf = rpr.get_or_add_rFonts()
            for a in ("w:ascii", "w:hAnsi", "w:eastAsia", "w:cs"):
                rf.set(qn(a), HEAD_FONT)
            for a in ("w:asciiTheme", "w:hAnsiTheme", "w:eastAsiaTheme", "w:cstheme"):
                if rf.get(qn(a)) is not None:
                    del rf.attrib[qn(a)]
            s.font.size = Pt(size)
            s.font.bold = name != "Title"
            s.font.italic = False
            s.font.color.rgb = rgb(color)
            s.paragraph_format.space_before = Pt(before)
            s.paragraph_format.space_after = Pt(after)
            s.paragraph_format.keep_with_next = True
        cap = st["Caption"]
        cap.font.name = BODY_FONT
        cap.font.size = Pt(9)
        cap.font.italic = False
        cap.font.bold = False
        cap.font.color.rgb = rgb(MUTED)
        cap.paragraph_format.space_after = Pt(12)
        from docx.enum.style import WD_STYLE_TYPE
        for name, size, bold in (("TOC 1", 10.5, True), ("TOC 2", 9.5, False)):
            try:
                ts = st[name]
            except KeyError:
                ts = st.add_style(name, WD_STYLE_TYPE.PARAGRAPH)
                ts.base_style = normal
            ts.font.size, ts.font.bold = Pt(size), bold
            ts.paragraph_format.space_before = Pt(5 if bold else 0)
            ts.paragraph_format.space_after = Pt(0)
            ts.paragraph_format.line_spacing = 1.0
            ts.paragraph_format.left_indent = Cm(0 if bold else 0.6)
        for lst in ("List Bullet", "List Bullet 2"):
            st[lst].paragraph_format.space_after = Pt(3)
            st[lst].font.name = BODY_FONT

        # header / footer
        hp = sec.header.paragraphs[0]
        hp.alignment = WD_ALIGN_PARAGRAPH.RIGHT
        r = hp.add_run("LundaLoggern  ·  Design Summary  ·  v1.3.0")
        r.font.size, r.font.color.rgb = Pt(8.5), rgb(MUTED)
        para_border_bottom(hp, "C9D3D7", 4, 4)
        fp = sec.footer.paragraphs[0]
        fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
        for part in ("Page ", None, " of ", "NUMPAGES"):
            if part is None:
                run = add_field(fp, "PAGE", "1")
            elif part == "NUMPAGES":
                run = add_field(fp, "NUMPAGES", "1")
            else:
                run = fp.add_run(part)
            run.font.size, run.font.color.rgb = Pt(8.5), rgb(MUTED)
        sec.different_first_page_header_footer = True

        cp = d.core_properties
        cp.title = "LundaLoggern — Design Summary"
        cp.subject = "Firmware architecture, wiring, data flow and interfaces"
        cp.author = "Åke L"

    # ---- inline content
    def inline(self, paragraph, token, size=None, color=None):
        stack: list[str] = []
        for it in token.children or []:
            t = it.type
            if t.endswith("_open"):
                stack.append(t[:-5])
            elif t.endswith("_close"):
                if stack:
                    stack.pop()
            elif t in ("softbreak",):
                paragraph.add_run(" ")
            elif t == "hardbreak":
                paragraph.add_run().add_break()
            elif t == "image":
                pass  # handled by caller
            elif t == "code_inline":
                r = paragraph.add_run(it.content)
                style_code_run(r, (size or 10.5) - 1)
                run_shading(r)
                r.bold = "strong" in stack
            elif t in ("text", "html_inline"):
                r = paragraph.add_run(it.content)
                r.bold = "strong" in stack
                r.italic = "em" in stack
                if size:
                    r.font.size = Pt(size)
                if color:
                    r.font.color.rgb = rgb(color)
                if "link" in stack:
                    r.font.color.rgb = rgb(BLUE)

    @staticmethod
    def has_image(token):
        return [c for c in (token.children or []) if c.type == "image"]

    # ---- blocks
    def heading(self, text_token, level):
        txt = "".join(c.content for c in text_token.children if c.type in ("text", "code_inline"))
        lvl = {2: 1, 3: 2, 4: 3, 5: 4}.get(level, 4)
        p = self.doc.add_heading(level=lvl)
        self.inline(p, text_token)
        if lvl == 1:
            para_border_bottom(p, "0B7A80", 8, 3)
        return txt

    def figure(self, png: BytesIO, caption: str, width_cm: float = 17.0):
        self.fig_no += 1
        p = self.doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_before = Pt(6)
        p.paragraph_format.keep_with_next = True
        p.add_run().add_picture(png, width=Cm(width_cm))
        cp = self.doc.add_paragraph(style="Caption")
        cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = cp.add_run(f"Figure {self.fig_no}. ")
        r.bold = True
        cp.add_run(caption)

    def _new_section(self, landscape: bool):
        from docx.enum.section import WD_ORIENT
        sec = self.doc.add_section(WD_SECTION.NEW_PAGE)
        sec.different_first_page_header_footer = False   # keep header/footer on this section's first page
        if landscape:
            sec.orientation = WD_ORIENT.LANDSCAPE
            sec.page_width, sec.page_height = Cm(29.7), Cm(21)
            sec.top_margin, sec.bottom_margin = Cm(1.8), Cm(1.5)
        else:
            sec.orientation = WD_ORIENT.PORTRAIT
            sec.page_width, sec.page_height = Cm(21), Cm(29.7)
            sec.top_margin, sec.bottom_margin = Cm(2.2), Cm(2)
        sec.left_margin = sec.right_margin = Cm(2)
        return sec

    def landscape_figure(self, png: BytesIO, caption: str, aspect: float):
        """Put a wide, detailed figure (e.g. a schematic) on its own landscape page."""
        self._new_section(landscape=True)
        width_cm = min(25.7, 15.6 * aspect)   # usable area ~25.7 x 15.6 cm after caption
        self.figure(png, caption, width_cm=width_cm)
        self._new_section(landscape=False)

    def code(self, src: str, indent_cm: float = 0.0, label: str | None = None):
        lines = src.rstrip("\n").split("\n")
        t = self.doc.add_table(rows=1, cols=1)
        t.alignment = WD_TABLE_ALIGNMENT.LEFT
        set_table_borders(t, "D5DEE1", 4, inside_v=True, left_accent=TEAL)
        cell = t.rows[0].cells[0]
        set_cell_shading(cell, "F4F7F8")
        set_cell_margins(cell, 90, 90, 160, 120)
        set_col_widths(t, [self.CONTENT_W - indent_cm])
        if indent_cm:
            ind = OxmlElement("w:tblInd")
            ind.set(qn("w:w"), str(int(Cm(indent_cm).twips)))
            ind.set(qn("w:type"), "dxa")
            t._tbl.tblPr.append(ind)
        p = cell.paragraphs[0]
        p.paragraph_format.space_after = Pt(0)
        p.paragraph_format.line_spacing = 1.0
        small = len(lines) > 25 or max(len(l) for l in lines) > 95
        for i, ln in enumerate(lines):
            if i:
                p.add_run().add_break()
            r = p.add_run(ln.replace("\t", "    "))
            style_code_run(r, 8 if small else 8.5)
            r.font.color.rgb = rgb("#22343C")
        if len(lines) <= 30:
            no_split(t.rows[0])
        self.spacer(4)

    def spacer(self, pts=4):
        p = self.doc.add_paragraph()
        p.paragraph_format.space_after = Pt(0)
        p.paragraph_format.line_spacing = Pt(pts)
        f = p.add_run().font
        f.size = Pt(2)

    def callout(self, tokens, accent=AMBER, fill="FDF6EC"):
        t = self.doc.add_table(rows=1, cols=1)
        set_table_borders(t, "F0DCC0", 4, inside_v=True, left_accent=accent)
        cell = t.rows[0].cells[0]
        set_cell_shading(cell, fill)
        set_cell_margins(cell, 100, 100, 180, 140)
        set_col_widths(t, [self.CONTENT_W])
        first = True
        for tok in tokens:
            p = cell.paragraphs[0] if first else cell.add_paragraph()
            first = False
            p.paragraph_format.space_after = Pt(0)
            self.inline(p, tok, size=10)
        no_split(t.rows[0])
        self.spacer(6)

    def table(self, rows: list[list], header_inline=True):
        ncols = max(len(r) for r in rows)
        self.tab_no += 1
        t = self.doc.add_table(rows=len(rows), cols=ncols)
        t.alignment = WD_TABLE_ALIGNMENT.CENTER
        set_table_borders(t, "C9D3D7", 4)
        # column widths from content length
        def tlen(tok):
            return len("".join(c.content for c in (tok.children or []) if c.type in ("text", "code_inline")))
        lens = []
        for ci in range(ncols):
            vals = [tlen(r[ci][0]) for r in rows if ci < len(r)]
            longest = max(vals) if vals else 4
            avg = sum(vals) / max(len(vals), 1)
            lens.append(max(6, min(60, 0.55 * longest + 0.45 * avg)))
        total = sum(lens)
        widths = [self.CONTENT_W * l / total for l in lens]
        # floor narrow columns
        def longest_word(tok):
            words = re.split(r"\s+", "".join(c.content for c in (tok.children or [])
                                              if c.type in ("text", "code_inline")))
            return max((len(w) for w in words), default=0)
        mins = []
        for ci in range(ncols):
            lw = max((longest_word(r[ci][0]) for r in rows if ci < len(r)), default=4)
            mins.append(min(5.0, max(1.8, 0.21 * lw + 0.45)))
        for _ in range(3):
            widths = [max(w, m) for w, m in zip(widths, mins)]
            excess = sum(widths) - self.CONTENT_W
            if excess <= 0:
                break
            flex = [i for i, (w, m) in enumerate(zip(widths, mins)) if w > m + 0.01]
            room = sum(widths[i] - mins[i] for i in flex) or 1
            for i in flex:
                widths[i] -= excess * (widths[i] - mins[i]) / room
        scale = self.CONTENT_W / sum(widths)
        widths = [w * scale for w in widths]
        set_col_widths(t, widths)
        for ri, row in enumerate(rows):
            tr = t.rows[ri]
            if ri == 0:
                repeat_header(tr)
            no_split(tr)
            for ci in range(ncols):
                cell = tr.cells[ci]
                cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
                set_cell_margins(cell, 50, 50, 90, 90)
                p = cell.paragraphs[0]
                p.paragraph_format.space_after = Pt(0)
                p.paragraph_format.line_spacing = 1.0
                if ci >= len(row):
                    continue
                tok, is_head = row[ci]
                if ri == 0:
                    set_cell_shading(cell, "0B7A80")
                    self.inline(p, tok, size=9.5, color="#FFFFFF")
                    for r in p.runs:
                        r.bold = True
                else:
                    if ri % 2 == 0:
                        set_cell_shading(cell, "F3F7F8")
                    self.inline(p, tok, size=9.5)
                    if ci == 0 and ncols <= 4:
                        pass
        self.spacer(8)

    def bullet(self, depth: int):
        style = "List Bullet" if depth <= 1 else "List Bullet 2"
        return self.doc.add_paragraph(style=style)

    def numbered(self, n: int, depth: int):
        p = self.doc.add_paragraph()
        pf = p.paragraph_format
        pf.left_indent = Cm(0.75 + 0.6 * (depth - 1))
        pf.first_line_indent = Cm(-0.6)
        pf.space_after = Pt(4)
        pf.tab_stops.add_tab_stop(pf.left_indent)
        r = p.add_run(f"{n}.\t")
        r.bold = True
        r.font.color.rgb = rgb(TEAL)
        return p

    def cont(self, depth: int):
        p = self.doc.add_paragraph()
        p.paragraph_format.left_indent = Cm(0.75 + 0.6 * (depth - 1))
        p.paragraph_format.space_after = Pt(4)
        return p

    # ---- cover
    def cover(self, meta: dict[str, str]):
        d = self.doc
        p = d.add_paragraph()
        p.paragraph_format.space_before = Pt(90)
        p = d.add_paragraph(style="Title")
        p.add_run("LundaLoggern")
        p = d.add_paragraph()
        r = p.add_run("Design Summary")
        r.font.size, r.font.color.rgb = Pt(18), rgb(BLUE)
        p.paragraph_format.space_after = Pt(4)
        p = d.add_paragraph()
        r = p.add_run("Portable CIE data logger for SERVO-u ventilators")
        r.font.size, r.font.color.rgb, r.italic = Pt(12), rgb(MUTED), True
        para_border_bottom(p, "0B7A80", 12, 10)
        p.paragraph_format.space_after = Pt(24)

        t = d.add_table(rows=len(meta), cols=2)
        set_table_borders(t, "E1E7EA", 4)
        set_col_widths(t, [3.5, 9])
        for i, (k, v) in enumerate(meta.items()):
            a, b = t.rows[i].cells
            for c in (a, b):
                set_cell_margins(c, 70, 70, 60, 60)
                c.paragraphs[0].paragraph_format.space_after = Pt(0)
            ra = a.paragraphs[0].add_run(k)
            ra.bold, ra.font.color.rgb, ra.font.size = True, rgb(MUTED), Pt(10)
            rb = b.paragraphs[0].add_run(v)
            rb.font.size = Pt(10.5)

        d.add_paragraph().add_run().add_break(WD_BREAK.PAGE)
        p = d.add_paragraph()
        r = p.add_run("Contents")
        r.bold, r.font.size, r.font.color.rgb = True, Pt(17), rgb(TEAL)
        para_border_bottom(p, "0B7A80", 8, 3)
        p.paragraph_format.space_after = Pt(10)
        p = d.add_paragraph()
        add_field(p, 'TOC \\o "1-2" \\h \\z \\u',
                  "Right-click and choose “Update Field” to build the table of contents.")
        p.add_run().add_break(WD_BREAK.PAGE)


# ================================================================ main conversion
def collect_inline_cells(tokens, i):
    rows, row = [], None
    while tokens[i].type != "table_close":
        tk = tokens[i]
        if tk.type == "tr_open":
            row = []
        elif tk.type in ("th_open", "td_open"):
            row.append((tokens[i + 1], tk.type == "th_open"))
        elif tk.type == "tr_close":
            rows.append(row)
        i += 1
    return rows, i + 1


def image_file_as_png(path: Path, max_w_cm: float = 17.0, max_h_cm: float = 20.0):
    """Load any PIL-readable image (webp, png, jpg ...) for embedding in Word.

    Word cannot display webp, so the image is re-encoded as PNG in memory.
    Uniform dark margins (e.g. the black background around a PCB render) are
    trimmed. Returns (png_bytes, width_cm, aspect) with the width chosen so the
    figure fits within max_w_cm x max_h_cm on a portrait page, and
    aspect = width / height of the trimmed image.
    """
    im = Image.open(path).convert("RGB")
    mask = im.convert("L").point(lambda v: 255 if v > 90 else 0)
    bbox = mask.getbbox()
    if bbox:
        pad = 12
        bbox = (max(bbox[0] - pad, 0), max(bbox[1] - pad, 0),
                min(bbox[2] + pad, im.width), min(bbox[3] + pad, im.height))
        im = im.crop(bbox)
    out = BytesIO()
    im.save(out, format="PNG")
    out.seek(0)
    width_cm = min(max_w_cm, max_h_cm * im.width / im.height)
    return out, width_cm, im.width / im.height


def build():
    make_annotated_pinout()   # regenerate the annotated pinout PNG referenced by the .md
    src = SOURCE.read_text(encoding="utf-8")
    md = MarkdownIt("commonmark").enable("table")
    tokens = md.parse(src)
    b = Builder()

    # --- metadata block (first paragraph after H1)
    meta = {}
    i = 0
    while tokens[i].type != "heading_open":
        i += 1
    i += 3  # skip H1
    assert tokens[i].type == "paragraph_open"
    raw = tokens[i + 1].content
    for line in raw.split("\n"):
        m = re.match(r"\*\*(.+?):\*\*\s*(.+?)\s*$", line.strip())
        if m:
            meta[m.group(1)] = m.group(2)
    i += 3
    b.cover(meta)

    current = ""
    lists: list[dict] = []  # stack: {"kind", "n", "first"}
    while i < len(tokens):
        tk = tokens[i]
        ty = tk.type
        depth = len(lists)

        if ty == "heading_open":
            current = b.heading(tokens[i + 1], int(tk.tag[1]))
            i += 3
            continue

        if ty == "hr":
            i += 1
            continue

        if ty in ("bullet_list_open", "ordered_list_open"):
            lists.append({"kind": "ol" if ty.startswith("ordered") else "ul",
                          "n": int(tk.attrGet("start") or 1) - 1, "first": False})
            i += 1
            continue
        if ty in ("bullet_list_close", "ordered_list_close"):
            lists.pop()
            if not lists:
                b.spacer(3)
            i += 1
            continue
        if ty == "list_item_open":
            lists[-1]["n"] += 1
            lists[-1]["first"] = True
            i += 1
            continue

        if ty == "blockquote_open":
            j = i + 1
            inner = []
            while tokens[j].type != "blockquote_close":
                if tokens[j].type == "inline":
                    inner.append(tokens[j])
                j += 1
            b.callout(inner)
            i = j + 1
            continue

        if ty == "paragraph_open":
            inl = tokens[i + 1]
            imgs = b.has_image(inl)
            if imgs:
                img = imgs[0]
                src_name = img.attrGet("src") or ""
                alt = "".join(c.content for c in (img.children or [])) or img.content
                if src_name.lower().endswith(".svg"):
                    # The wiring SVG is redrawn as a raster figure (Word can't embed SVG reliably).
                    b.figure(fig_pins(), "Wiring between the T-Display S3, RS232 transceiver, "
                                         "SERVO-u ventilator and MicroSD reader.")
                else:
                    png, w_cm, aspect = image_file_as_png(ROOT / src_name)
                    caption = alt.rstrip(".") + "."
                    if aspect >= 1.3:
                        b.landscape_figure(png, caption, aspect)   # wide drawings, e.g. the schematic
                    else:
                        b.figure(png, caption, width_cm=w_cm)
            elif lists:
                top = lists[-1]
                if top["first"]:
                    p = b.numbered(top["n"], depth) if top["kind"] == "ol" else b.bullet(depth)
                    top["first"] = False
                else:
                    p = b.cont(depth)
                b.inline(p, inl)
            else:
                p = b.doc.add_paragraph()
                b.inline(p, inl)
            i += 3
            continue

        if ty == "table_open":
            rows, i = collect_inline_cells(tokens, i)
            b.table(rows)
            continue

        if ty in ("fence", "code_block"):
            info = (tk.info or "").strip().lower()
            if info == "mermaid" and current.startswith("3.2"):
                b.figure(fig_classes(), "Runtime class relationships. main.cpp owns and services every "
                                        "module; arrows show direct calls between modules.")
            elif not info and current.startswith("3.5"):
                b.figure(fig_flow(), "Runtime data flow from the ventilator to SD card, USB host, "
                                     "web browser and TFT display.")
            else:
                b.code(tk.content, indent_cm=(0.75 + 0.6 * (depth - 1)) if lists else 0)
            i += 1
            continue

        i += 1

    b.doc.save(OUT)
    print(f"Saved {OUT}")


def refresh_fields_with_word(path: Path) -> bool:
    try:
        import comtypes.client  # type: ignore
    except ImportError:
        return False
    word = comtypes.client.CreateObject("Word.Application", dynamic=True)
    try:
        d = word.Documents.Open(str(path), False, False)
        for name, size, bold, before in (("TOC 1", 10.5, True, 5), ("TOC 2", 9.5, False, 0)):
            st = d.Styles(name)
            st.Font.Size, st.Font.Bold = size, bold
            st.ParagraphFormat.SpaceBefore, st.ParagraphFormat.SpaceAfter = before, 0
            st.ParagraphFormat.LineSpacingRule = 0  # single
        for k in range(1, d.TablesOfContents.Count + 1):
            d.TablesOfContents(k).Update()
        d.Fields.Update()
        d.Save()
        d.Close(False)
    finally:
        word.Quit()
    return True


if __name__ == "__main__":
    build()
    if refresh_fields_with_word(OUT):
        print("Table of contents refreshed via Word.")
    else:
        print("comtypes/Word not available: update the TOC field in Word (F9).")
