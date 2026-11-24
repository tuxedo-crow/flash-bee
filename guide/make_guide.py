"""Build the Flash Bee guides.

Usage:
  python3 make_guide.py <img_dir> <out.pdf>            the full guide (Flash Bee Guide.pdf)
  python3 make_guide.py --quick <img_dir> <out.pdf>    the two-page quick start (Flash Bee Quick Start.pdf)
"""
import io
import sys
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image,
                                Table, TableStyle, PageBreak, KeepTogether, CondPageBreak)
from reportlab.platypus.doctemplate import NextPageTemplate
from reportlab.graphics.shapes import Drawing, Circle, String, Line, Rect, Wedge
from reportlab.lib.utils import ImageReader

_args = [a for a in sys.argv[1:] if a != "--quick"]
QUICK = "--quick" in sys.argv[1:]
if len(_args) != 2:
    sys.exit(__doc__)
IMG, OUT = _args

YELLOW = colors.HexColor("#F5C400")
INK = colors.HexColor("#1D1D1F")
GREY = colors.HexColor("#5F6368")
LIGHT = colors.HexColor("#FFF6CC")
RED = colors.HexColor("#D9480F")
MAGENTA = colors.HexColor("#C2185B")
GREEN = colors.HexColor("#2E7D32")
SCREEN = colors.HexColor("#0E1220")
PINK_BG = colors.HexColor("#FCE4EC")
RED_BG = colors.HexColor("#FFE3D6")
GREEN_BG = colors.HexColor("#E3F4E4")

# colours of things drawn on the device screen
S_GOLD = colors.HexColor("#FFD43B")
S_ORANGE = colors.HexColor("#FF8C1A")
S_REDORG = colors.HexColor("#FF5A36")
S_RED = colors.HexColor("#E53935")
S_GREY = colors.HexColor("#9AA0A6")
S_DIM = colors.HexColor("#6B7280")
S_RING = colors.HexColor("#2C3350")
S_GAUGE = colors.HexColor("#2A2F45")

ss = getSampleStyleSheet()
H1 = ParagraphStyle("H1", parent=ss["Heading1"], fontName="Helvetica-Bold", fontSize=22, leading=26,
                    textColor=INK, spaceBefore=4, spaceAfter=10)
H2 = ParagraphStyle("H2", parent=ss["Heading2"], fontName="Helvetica-Bold", fontSize=14.5, leading=18,
                    textColor=INK, spaceBefore=12, spaceAfter=6)
H3 = ParagraphStyle("H3", parent=H2, fontSize=12, leading=15, spaceBefore=8, spaceAfter=4)
BODY = ParagraphStyle("Body", parent=ss["BodyText"], fontName="Helvetica", fontSize=10.5, leading=14.5,
                      textColor=INK, spaceAfter=6, bulletFontName="Helvetica")
SMALL = ParagraphStyle("Small", parent=BODY, fontSize=8.5, leading=11, textColor=GREY)
CAP = ParagraphStyle("Cap", parent=SMALL, alignment=TA_CENTER, spaceBefore=2, spaceAfter=10)
BUL = ParagraphStyle("Bul", parent=BODY, leftIndent=14, bulletIndent=3, spaceAfter=3)
BOXT = ParagraphStyle("BoxT", parent=BODY, fontName="Helvetica-Bold", fontSize=11, spaceAfter=3)
CELL = ParagraphStyle("Cell", parent=BODY, fontSize=9.5, leading=12.5, spaceAfter=0)
CELLB = ParagraphStyle("CellB", parent=CELL, fontName="Helvetica-Bold")
CELLS = ParagraphStyle("CellS", parent=CELL, fontSize=9, leading=11.5)
CELLSB = ParagraphStyle("CellSB", parent=CELLS, fontName="Helvetica-Bold")
CODE = ParagraphStyle("Code", parent=BODY, fontName="Courier", fontSize=9.5, leading=12.5, leftIndent=14,
                      backColor=colors.HexColor("#F2F2F2"), borderPadding=(4, 6, 4, 6), spaceBefore=4,
                      spaceAfter=10)

# quick start: bigger type, short lines
QTITLE = ParagraphStyle("QTitle", parent=H1, fontSize=26, leading=30, spaceAfter=2)
QSUB = ParagraphStyle("QSub", parent=BODY, fontSize=13, leading=16, textColor=GREY, spaceAfter=6)
QH = ParagraphStyle("QH", parent=H2, fontSize=16, leading=19, spaceBefore=7, spaceAfter=3)
QBODY = ParagraphStyle("QBody", parent=BODY, fontSize=12, leading=15.5, spaceAfter=4)
QBUL = ParagraphStyle("QBul", parent=QBODY, leftIndent=16, bulletIndent=3, spaceAfter=2.5)
QCELL = ParagraphStyle("QCell", parent=QBODY, spaceAfter=0)
QCELLB = ParagraphStyle("QCellB", parent=QCELL, fontName="Helvetica-Bold")

W, H = letter
M = 0.75 * inch
CW = W - 2 * M



def P(t, s=BODY):
    return Paragraph(t, s)


def bullets(items, style=BUL):
    return [Paragraph(i, style, bulletText="•") for i in items]


def checklist(items):
    """Rows with an empty tick box drawn in front (a font glyph is not reliable in every PDF viewer)."""
    def tick():
        d = Drawing(12, 12)
        d.add(Rect(1, 0, 9, 9, fillColor=None, strokeColor=INK, strokeWidth=0.8))
        return d
    t = Table([[tick(), P(i, BODY)] for i in items], colWidths=[18, CW - 18])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                           ("LEFTPADDING", (0, 0), (-1, -1), 2), ("RIGHTPADDING", (0, 0), (-1, -1), 2),
                           ("TOPPADDING", (0, 0), (0, -1), 3), ("TOPPADDING", (1, 0), (1, -1), 0),
                           ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]))
    return [t]


def img(name, width, caption=None):
    path = f"{IMG}/{name}.jpg"
    iw, ih = ImageReader(path).getSize()
    h = width * ih / iw
    out = [Image(path, width=width, height=h)]
    if caption:
        out.append(P(caption, CAP))
    return out


def box(title, paras, bg=LIGHT, edge=YELLOW, tstyle=BOXT, gap=True):
    inner = [P(title, tstyle)] + [p if not isinstance(p, str) else P(p) for p in paras]
    t = Table([[inner]], colWidths=[CW])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), bg),
                           ("LINEBEFORE", (0, 0), (0, -1), 4, edge),
                           ("LEFTPADDING", (0, 0), (-1, -1), 12), ("RIGHTPADDING", (0, 0), (-1, -1), 12),
                           ("TOPPADDING", (0, 0), (-1, -1), 8), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return KeepTogether([Spacer(1, 4), t, Spacer(1, 8)] if gap else [t])


def table(rows, widths, header=True, cell=CELL, cellb=CELLB, pad=4):
    data = [[c if not isinstance(c, str) else P(c, cellb if (header and r == 0) else cell) for c in row]
            for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    st = [("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#D0D0D0")),
          ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("LEFTPADDING", (0, 0), (-1, -1), 6), ("RIGHTPADDING", (0, 0), (-1, -1), 6),
          ("TOPPADDING", (0, 0), (-1, -1), pad), ("BOTTOMPADDING", (0, 0), (-1, -1), pad)]
    if header:
        st.append(("BACKGROUND", (0, 0), (-1, 0), YELLOW))
    t.setStyle(TableStyle(st))
    return t


def img_h(name, height, caption=None, max_w=None):
    path = f"{IMG}/{name}.jpg"
    iw, ih = ImageReader(path).getSize()
    w = height * iw / ih
    if max_w and w > max_w:
        w, height = max_w, max_w * ih / iw
    out = [Image(path, width=w, height=height)]
    if caption:
        out.append(P(caption, CAP))
    return out


def two_photos(a, b, cap_a, cap_b, h=1.75 * inch):
    w = CW / 2 - 6
    t = Table([[img_h(a, h, cap_a, w), img_h(b, h, cap_b, w)]], colWidths=[CW / 2, CW / 2])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                           ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3)]))
    return t


# ---------- screen drawings (from the firmware's 240 x 240 layout) ----------
def _frame(dw, dh, S):
    ox, oy = (dw - 240 * S) / 2, (dh - 240 * S) / 2

    def X(x): return ox + x * S
    def Y(y): return oy + (240 - y) * S
    return Drawing(dw, dh), X, Y, ox


def _txt(d, X, Y, S):
    k = S / 0.95                       # font sizes below are tuned for 0.95 pt per screen pixel

    def txt(x, y, s, size, col, font="Helvetica-Bold", anchor="middle"):
        sz = size * k
        d.add(String(X(x), Y(y) - sz * 0.35, s, fontName=font, fontSize=sz, fillColor=col, textAnchor=anchor))
    return txt


def _callouts(d, X, Y, S, ox, dw):
    def call(x, y, side, label):
        lx = 6 if side == "L" else dw - 6
        ty = Y(y)
        d.add(Line(X(x), ty, (ox - 8) if side == "L" else (ox + 240 * S + 8), ty, strokeColor=GREY, strokeWidth=0.5))
        d.add(Circle(X(x), ty, 1.5, fillColor=RED, strokeColor=None))
        d.add(String(lx, ty - 3, label, fontName="Helvetica", fontSize=8, fillColor=INK,
                     textAnchor="start" if side == "L" else "end"))
    return call


def main_face(d, X, Y, S, mode):
    """The Main page. mode: 'wait' (no strike yet), 'activity' (a storm at 15 mi), 'shelter' (6 mi, countdown)."""
    txt = _txt(d, X, Y, S)
    shelter = mode == "shelter"
    d.add(Circle(X(120), Y(120), 120 * S, fillColor=SCREEN, strokeColor=S_RED if shelter else INK, strokeWidth=3))
    d.add(Wedge(X(120), Y(120), 116 * S, -45, 225, radius1=106 * S, fillColor=S_GAUGE, strokeColor=None))
    if mode != "wait":   # gauge filled from 7:30 clockwise by energy / 2097151
        e = 290 if mode == "activity" else 410
        d.add(Wedge(X(120), Y(120), 116 * S, 225 - 270 * e / 2097.151, 225, radius1=106 * S, fillColor=S_GOLD,
                    strokeColor=None))
    if shelter:
        d.add(Circle(X(120), Y(120), 117 * S, fillColor=None, strokeColor=S_RED, strokeWidth=2))
    for r in (90, 58):
        d.add(Circle(X(120), Y(120), r * S, fillColor=None, strokeColor=S_RING, strokeWidth=0.8))
    for r in (80, 40):   # the listening rings, drifting in toward the centre
        d.add(Circle(X(120), Y(120), r * S, fillColor=None,
                     strokeColor=colors.HexColor("#B23A2E") if shelter else colors.HexColor("#1FA37A"), strokeWidth=1.4))
    txt(120, 16, "LIGHTNING DETECTOR", 6.5, S_GREY, "Helvetica")
    txt(120, 27, "NF2 WD2 SR2 PORT", 5.5, S_DIM, "Helvetica")
    if mode == "wait":
        txt(120, 84, "--", 30, S_DIM)
        txt(120, 108, "waiting...", 6.5, S_GREY, "Helvetica")
        strikes, energy, ecol = "0", "0", S_DIM
    elif mode == "activity":
        txt(120, 40, "1m ago   3 in 15m", 6.5, S_GREY, "Helvetica")
        txt(120, 80, "15", 40, S_GOLD)
        txt(120, 112, "mi", 15, S_GOLD)
        strikes, energy, ecol = "6", "290K", S_GOLD
    else:
        txt(120, 40, "20s ago   7 in 15m", 6.5, S_GREY, "Helvetica")
        txt(120, 80, "6", 40, S_GOLD)
        txt(120, 112, "mi", 15, S_GOLD)
        strikes, energy, ecol = "7", "410K", S_GOLD
    d.add(Line(X(44), Y(128), X(196), Y(128), strokeColor=S_RING, strokeWidth=0.8))
    txt(40, 140, "STRIKES", 5.5, S_GREY, "Helvetica", "start")
    txt(40, 156, strikes, 12, S_GOLD, anchor="start")
    txt(200, 140, "ENERGY", 5.5, S_GREY, "Helvetica", "end")
    txt(200, 156, energy, 12, ecol, anchor="end")
    if mode != "wait":
        txt(120, 140, "APPROACHING", 6, S_REDORG)
    # battery icon + percentage
    d.add(Rect(X(102), Y(172), 12 * S, 7 * S, fillColor=None, strokeColor=S_GREY, strokeWidth=0.6))
    d.add(Rect(X(104), Y(170), 6.5 * S, 3 * S, fillColor=S_GREY, strokeColor=None))
    d.add(Rect(X(114), Y(170), 2 * S, 3 * S, fillColor=S_GREY, strokeColor=None))
    txt(119, 168.5, "82%", 6, S_GREY, "Helvetica", "start")
    if shelter:
        d.add(Rect(X(42), Y(206), 156 * S, 32 * S, rx=5, ry=5, fillColor=S_RED, strokeColor=None))
        txt(120, 191, "SHELTER", 17, colors.white)
        txt(120, 216, "ALL CLEAR IN 29:40", 6, S_REDORG)
    else:
        d.add(Rect(X(30), Y(210), 180 * S, 36 * S, rx=4, ry=4, fillColor=None, strokeColor=S_RING, strokeWidth=0.8))
        heights = [] if mode == "wait" else [6, 14, 4, 22, 9, 4]
        for i, hh in enumerate(heights):
            d.add(Rect(X(32 + i * 9), Y(206), 7 * S, hh * S, strokeColor=None,
                       fillColor=S_GOLD if i == len(heights) - 1 else S_ORANGE))
        if mode == "wait":
            txt(120, 216, "ENERGY HISTORY", 6, S_GREY, "Helvetica")
        else:
            txt(120, 216, "ACTIVITY", 6.5, S_ORANGE)
    for i in range(6):
        d.add(Circle(X(95 + i * 10), Y(228), 2.2 * S if i == 0 else 1.5 * S,
                     fillColor=S_GOLD if i == 0 else S_DIM, strokeColor=None))


def screen_map():
    """Main page during a storm, with callouts."""
    dw, dh, S = CW, 260, 0.95
    d, X, Y, ox = _frame(dw, dh, S)
    main_face(d, X, Y, S, "activity")
    call = _callouts(d, X, Y, S, ox, dw)
    call(70, 27, "L", "Sensor settings + profile")
    call(62, 40, "L", "Time since last strike")
    call(95, 80, "L", "Distance to the storm")
    call(88, 140, "L", "Storm trend")
    call(48, 158, "L", "Strikes since switched on")
    call(90, 216, "L", "Alert (here: ACTIVITY)")
    call(198, 42, "R", "Energy gauge (last strike)")
    call(173, 60, "R", "Green rings = listening")
    call(190, 156, "R", "Energy of last strike")
    call(140, 168, "R", "Battery")
    call(150, 196, "R", "Energy history bars")
    call(140, 228, "R", "Page dots")
    return d


def shelter_screen():
    dw, dh, S = CW, 215, 0.85
    d, X, Y, ox = _frame(dw, dh, S)
    main_face(d, X, Y, S, "shelter")
    call = _callouts(d, X, Y, S, ox, dw)
    call(60, 191, "L", "Big flashing banner")
    call(80, 216, "L", "30-minute all-clear countdown")
    call(190, 100, "R", "Flashing red ring")
    call(173, 60, "R", "Rings turn red too")
    return d


def mini_screen(mode, dw, S=0.6):
    d, X, Y, ox = _frame(dw, 240 * S + 6, S)
    main_face(d, X, Y, S, mode)
    return d


def settings_screen():
    """Settings page, with callouts."""
    dw, dh, S = CW, 215, 0.85
    d, X, Y, ox = _frame(dw, dh, S)
    txt = _txt(d, X, Y, S)
    call = _callouts(d, X, Y, S, ox, dw)
    d.add(Circle(X(120), Y(120), 120 * S, fillColor=SCREEN, strokeColor=INK, strokeWidth=3))

    def btn(x, y, w, h, label, value, sub=None):
        d.add(Rect(X(x), Y(y + h), w * S, h * S, rx=6, ry=6, fillColor=None, strokeColor=S_DIM, strokeWidth=1.6))
        cx = x + w / 2
        txt(cx, y + 10, label, 5.5, S_GREY, "Helvetica")
        txt(cx, y + (24 if sub else 29), value, 11, colors.white)
        if sub:
            txt(cx, y + 38, sub, 5.5, S_ORANGE)
    txt(120, 20, "SETTINGS", 10, S_GOLD)
    btn(44, 42, 152, 46, "PROFILE", "AUTO", sub="PORTABLE")
    btn(28, 94, 88, 46, "UNITS", "MILES")
    d.add(Rect(X(124), Y(94 + 46), 88 * S, 46 * S, rx=6, ry=6, fillColor=None, strokeColor=S_DIM, strokeWidth=1.6))
    txt(168, 105, "BRIGHTNESS", 5.5, S_GREY, "Helvetica")
    for i in range(3):
        bh = 6 + i * 5
        d.add(Rect(X(151 + i * 12), Y(132), 10 * S, bh * S, fillColor=S_GOLD, strokeColor=S_GOLD, strokeWidth=0.6))
    btn(44, 146, 152, 46, "SCREEN OFF AFTER", "5 MIN")
    txt(120, 203, "tap a button to change", 5.5, colors.HexColor("#4B5563"), "Helvetica")
    call(60, 65, "L", "Profile (tap to change)")
    call(60, 117, "L", "Units: miles or km")
    call(60, 169, "L", "When the screen goes dark")
    call(190, 117, "R", "Brightness (3 levels)")
    call(160, 80, "R", "AUTO shows the choice it made")
    return d


def page_strip():
    names = ["Main", "Storm\nchart", "Flash-\nto-bang", "Records", "Settings", "Nerd\nstats"]
    dw, dh = CW, 92
    d = Drawing(dw, dh)
    step = dw / 6
    for i, n in enumerate(names):
        cx = step * (i + 0.5)
        d.add(Circle(cx, 52, 30, fillColor=SCREEN, strokeColor=YELLOW if i == 0 else INK, strokeWidth=2.5))
        lines = n.split("\n")
        for j, ln in enumerate(lines):
            d.add(String(cx, 54 - (j - (len(lines) - 1) / 2) * 11 - 3, ln, fontName="Helvetica-Bold",
                         fontSize=8.5, fillColor=colors.white, textAnchor="middle"))
        d.add(String(cx, 6, str(i + 1), fontName="Helvetica", fontSize=8, fillColor=GREY, textAnchor="middle"))
        if i < 5:
            d.add(String(cx + step / 2, 49, "<  >", fontName="Helvetica-Bold", fontSize=9, fillColor=GREY,
                         textAnchor="middle"))
    return d


def range_scale():
    """How far the sensor hears, and the 10 mile danger zone."""
    dw, dh = CW, 70
    d = Drawing(dw, dh)
    x0, x1 = 20, dw - 20
    def X(mi): return x0 + (x1 - x0) * mi / 25
    d.add(Rect(X(0), 30, X(10) - X(0), 14, fillColor=colors.HexColor("#FFD8C2"), strokeColor=None))
    d.add(Rect(X(10), 30, X(25) - X(10), 14, fillColor=colors.HexColor("#FFF1B8"), strokeColor=None))
    d.add(Line(X(0), 30, X(25), 30, strokeColor=INK, strokeWidth=1))
    for mi in range(0, 26, 5):
        d.add(Line(X(mi), 26, X(mi), 30, strokeColor=INK))
        d.add(String(X(mi), 14, f"{mi} mi", fontName="Helvetica", fontSize=8, fillColor=INK, textAnchor="middle"))
    d.add(String(X(5), 34, "Within striking distance", fontName="Helvetica-Bold", fontSize=7.5,
                 fillColor=RED, textAnchor="middle"))
    d.add(String(X(17.5), 34, "Storm in range: keep watching", fontName="Helvetica-Bold", fontSize=7.5,
                 fillColor=colors.HexColor("#8A6D00"), textAnchor="middle"))
    d.add(String(X(0), 52, "You", fontName="Helvetica-Bold", fontSize=9, fillColor=INK, textAnchor="middle"))
    d.add(String(X(25), 52, "Edge of range (about 25 mi / 40 km)", fontName="Helvetica", fontSize=8,
                 fillColor=GREY, textAnchor="end"))
    return d


# ---------- page furniture ----------
CREDIT = "Based on \"Flash Bee\" by gokux (Instructables), CC BY-NC-SA 4.0"


class GuideDoc(BaseDocTemplate):
    """Records the page each chapter (H1) starts on, for the contents list and the page header."""

    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)
        self.chapters = []

    def afterFlowable(self, f):
        if isinstance(f, Paragraph) and f.style.name == "H1":
            self.chapters.append((f.getPlainText(), self.page))


def make_on_page(chapters):
    def on_page(c, doc):
        c.saveState()
        c.setFillColor(YELLOW)
        c.rect(0, H - 0.32 * inch, W, 0.32 * inch, stroke=0, fill=1)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 8.5)
        c.drawString(M, H - 0.21 * inch, "FLASH BEE  |  Handheld Lightning Detector")
        title = ""
        for t, p in chapters:
            if p <= doc.page:
                title = t
        c.setFont("Helvetica", 8.5)
        c.drawRightString(W - M, H - 0.21 * inch, title)
        c.setFont("Helvetica", 8)
        c.setFillColor(GREY)
        c.drawRightString(W - M, 0.45 * inch, f"Page {doc.page}")
        c.drawString(M, 0.45 * inch, CREDIT)
        c.restoreState()
    return on_page


def on_cover(c, doc):
    c.saveState()
    c.setFillColor(INK)
    c.rect(0, 0, W, H, stroke=0, fill=1)
    c.setFillColor(YELLOW)
    c.rect(0, H - 2.6 * inch, W, 2.6 * inch, stroke=0, fill=1)
    bolt(c, W - 1.9 * inch, H - 0.35 * inch, 1.0)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 46)
    c.drawString(M, H - 1.35 * inch, "FLASH BEE")
    c.setFont("Helvetica-Bold", 16)
    c.drawString(M, H - 1.75 * inch, "Handheld Lightning Detector")
    c.setFont("Helvetica", 12)
    c.drawString(M, H - 2.1 * inch, "How it works, how to use it, and how to build your own")
    path = f"{IMG}/device.jpg"
    iw, ih = ImageReader(path).getSize()
    ph = 5.6 * inch
    pw = ph * iw / ih
    c.drawImage(path, (W - pw) / 2, 1.55 * inch, width=pw, height=ph)
    c.setFillColor(YELLOW)
    c.setFont("Helvetica-Bold", 20)
    c.drawCentredString(W / 2, 0.95 * inch, "AS3935 + XIAO ESP32-C3 + Round Display")
    c.setFillColor(colors.HexColor("#B0B0B0"))
    c.setFont("Helvetica", 8)
    c.drawCentredString(W / 2, 0.5 * inch,
                        "Based on \"Flash Bee\" by gokux on Instructables (CC BY-NC-SA 4.0). This guide is shared under the same license.")
    c.restoreState()


def bolt(c, bx, by, k):
    c.setFillColor(INK)
    p = c.beginPath()
    pts = [(0, 0), (-0.55, -1.05), (-0.18, -1.05), (-0.5, -1.95), (0.42, -0.72), (0.05, -0.72), (0.4, 0)]
    p.moveTo(bx + pts[0][0] * inch * k, by + pts[0][1] * inch * k)
    for x, y in pts[1:]:
        p.lineTo(bx + x * inch * k, by + y * inch * k)
    p.close()
    c.drawPath(p, stroke=0, fill=1)


# ======================================================================================
# Full guide
# ======================================================================================
def guide_story(toc):
    def pg(title):
        return str(toc.get(title, ""))

    CH = ["What is Flash Bee?", "How the sensor detects lightning", "Using Flash Bee",
          "Storm safety and the 30-30 rule", "Experiments and reference", "Build your own Flash Bee",
          "How the firmware works", "Troubleshooting and care"]
    s = [NextPageTemplate("body"), PageBreak()]

    # ---------- 1. What it is ----------
    s += [P(CH[0], H1),
          P("Flash Bee is a handheld lightning detector. It picks up the radio burst that every lightning "
            "flash gives off and shows <b>how far away the storm is</b>, <b>how strong the strikes are</b>, "
            "and <b>whether the storm is approaching or moving away</b>. Its range is about "
            "25 miles (40 km), so it often detects a storm before any thunder is audible."),
          P("The round touchscreen has six pages: the main lightning display, a storm chart of the last hour, "
            "a flash-to-bang timer for your own measurements, all-time records, settings, and a page of "
            "raw sensor data. A two-level alert, ACTIVITY and SHELTER, turns the numbers into a clear "
            "instruction about when to go inside."),
          box("Quick start", bullets([
              "Slide the power switch on the bottom of the case to on. The screen shows the <b>FLASH BEE</b> splash, then <b>AS3935</b> and a status line that ends in <b>CAL OK</b>, "
              "then the main screen. (On the very first start it also shows <b>TUNING ANTENNA...</b> for a few seconds. "
              "Let it finish.)",
              "<b>\"--\" and \"waiting...\"</b> with green rings drifting in toward the middle means it is listening, "
              "not stuck. It can stay like that for days between storms.",
              "<b>A small orange ACTIVITY</b> means lightning was detected recently. A <b>big flashing SHELTER</b> means "
              "go indoors now.",
              "The screen turns off after a set time to save battery. <b>Touch it</b> to wake it.",
              "<b>Swipe left or right</b> to change pages.",
              "<b>Hold a finger still on the screen for 3 seconds</b> to start a demo storm. Hold again to stop it.",
              "Charge with a USB-C cable. The switch must be <b>on</b> while it charges.",
              "The two-page <b>Flash Bee Quick Start</b> has the same first steps in bigger print.",
          ])),
          P("Contents", H2),
          table([["", "Chapter", "Page"]] +
                [[str(i + 1), t + (" (this page)" if i == 0 else ""), pg(t)] for i, t in enumerate(CH)],
                [0.4 * inch, CW - 1.0 * inch, 0.6 * inch]),
          PageBreak()]

    # ---------- 2. How it works ----------
    s += [P(CH[1], H1),
          P("A lightning flash is a huge electric discharge: tens of thousands of amps for a small fraction of a "
            "second. A current that large, changing that fast, radiates a burst of <b>radio waves</b> called a "
            "<b>sferic</b> (short for atmospheric). The crackle on an AM radio during a storm is sferics."),
          P("Flash Bee's sensor is the <b>AS3935 Franklin lightning sensor</b> (named after Benjamin Franklin). "
            "Its antenna is a small coil in a resonant circuit tuned to <b>500 kHz</b>, where lightning's signal is "
            "strong. (Flash Bee trims the tuning itself on the first start.) "
            "For every burst the coil picks up, the chip analyses the waveform:"),
          *bullets([
              "<b>Lightning</b>: the pulses match a lightning signal. The chip reports a strike, with an energy value "
              "and a distance estimate.",
              "<b>Disturber</b>: a signal that is not lightning, from phone chargers, computers, LED lights or motors. "
              "The chip rejects it. In the everyday profiles (STATION and PORTABLE) Flash Bee tells the chip not to "
              "report them at all. In the STORMCHASE profile they are counted on the Nerd stats page.",
              "<b>Noise too high</b>: the steady background noise is above the chip's <b>noise floor</b> threshold. "
              "Flash Bee raises the noise floor setting (NF), making the chip less sensitive, and lowers it when it is quiet.",
          ]),
          P("How it estimates distance", H2),
          P("One sensor cannot locate a flash, so the chip estimates the distance from the <b>strength</b> of the "
            "signals, and the estimate improves as more strikes arrive from the same storm. That is why distances come "
            "in steps (like 6, 9, 12 miles) and refer to the <b>edge of the storm</b>, not one bolt. The chip "
            "clears its distance statistics after 15 minutes without strikes, so Flash Bee dims an old distance after "
            "15 minutes."),
          P("Why it detects storms before thunder", H2),
          P("Radio waves travel at the speed of light: from 10 miles away they arrive in about 0.00005 seconds. "
            "Sound takes about <b>5 seconds per mile</b>, so thunder from 10 miles away takes almost a minute to "
            "arrive, and thunder is rarely audible beyond about 10 miles. Flash Bee detects strikes much farther away."),
          KeepTogether(img("pads", CW * 0.45, "The AS3935 lightning sensor board, front (top) and back (bottom). The antenna coil is "
               "the long part marked MA5532-AE. Photo: gokux.")),
          PageBreak()]

    # ---------- 3. Using it ----------
    s += [P(CH[2], H1),
          P("Swipe left or right across the screen to move between six pages. The dots at the bottom show which page "
            "you are on. If you leave it on another page for 2 minutes, it goes back to the Main page by itself, and "
            "a real strike always jumps straight to Main (unless you are timing a flash-to-bang)."),
          page_strip(),
          Spacer(1, 6),
          P("1. Main page", H2),
          screen_map(),
          P("The drawing above shows the Main page during a storm (with made-up numbers).", CAP),
          table([["On screen", "What it means"],
                 ["Big number + mi", "Distance to the storm. <b>OVERHEAD</b> = less than 1 mile. <b>&gt;25 mi</b> = heard, but too far to measure. <b>--</b> with \"distance unknown\" = a strike the chip could not measure."],
                 ["1m ago", "Time since the last strike. After 15 minutes without a strike the distance turns dim grey: it is an old reading."],
                 ["3 in 15m", "Strikes with a distance in the last 15 minutes. More strikes = a more active storm."],
                 ["APPROACHING / STEADY / MOVING AWAY", "Where the storm is going, from the distances of the strikes in the last 10 minutes. Needs at least 3 strikes over 2 minutes."],
                 ["STRIKES", "Strikes counted since you switched it on."],
                 ["ENERGY + gauge", "How strong the last strike was. The colour goes from dim red (weak) through orange and gold to white (very strong). The number has no unit; it is the chip's own scale."],
                 ["Energy history bars", "The last 20 strikes, oldest on the left. The gold bar is the newest."],
                 ["ACTIVITY / SHELTER / ALL CLEAR IN", "The alert, under the bars. See the next section."],
                 ["Battery", "How full the battery is. Turns red-orange under 15 %. \"--\" means no reading. See Battery and charging below."],
                 ["WiFi bars", "Only if WiFi has been set up: small bars right of the battery, filled when it is connected."],
                 ["STAT / PORT / CHAS", "The sensor profile in use, shown after NF WD SR (see Settings). STAT = home, PORT = on the move, CHAS (red-orange) = storm chasing."],
                 ["NF WD SR", "The sensor's filter settings. NF (noise floor) rises by itself near radio noise and drops after a quiet minute. WD (watchdog threshold) and SR (spike rejection) are set by the profile. A red <b>ENV TOO NOISY</b> here means the radio noise is too loud for the sensor to be trusted."],
                 ["Green rings", "Two green rings drift in toward the middle while it listens: it is working, not stuck. They do not point at the storm, because the sensor cannot tell which direction a strike came from. They turn red during SHELTER."],
                 ["SENSOR LOST", "A red screen: the sensor stopped answering. Flash Bee tries to reconnect every 3 seconds and the other pages still work."]],
                [1.75 * inch, CW - 1.75 * inch]),
          P("When a strike arrives, the whole screen flashes. If the screen was dark, it lights up first.", BODY),
          CondPageBreak(3 * inch),
          P("The two-step alert", H2),
          P("At the bottom of the Main page, Flash Bee shows one of three alert states:"),
          table([["State", "What you see", "What it means"],
                 ["Quiet", "The ENERGY HISTORY label", "No lightning detected in the last 15 minutes."],
                 ["<b>ACTIVITY</b>", "A small orange ACTIVITY label", "Lightning was detected in the last 15 minutes, but not close and not many strikes. Keep watching."],
                 ["<b>SHELTER</b>", "A big flashing red and orange SHELTER banner and a flashing ring (on the other pages, a small flashing SHELTER at the top)", "<b>Go indoors.</b> A strike 10 miles (16 km) or closer in the last 30 minutes, or 5 or more strikes in the last 15 minutes (strikes too far to measure do not count here)."]],
                [1.0 * inch, 2.4 * inch, CW - 3.4 * inch]),
          Spacer(1, 6),
          shelter_screen(),
          P("The Main page in SHELTER (made-up numbers). You cannot dismiss SHELTER: it goes away by itself when "
            "the storm has passed. The screen stays on while SHELTER is showing.", CAP),
          P("Under the banner you see either the all-clear countdown (after a close strike) or how many strikes "
            "there were in the last 15 minutes. When the countdown reaches zero, <b>ALL CLEAR</b> shows in green "
            "for a minute. Chapter 4 explains the 30-minute rule behind it."),
          CondPageBreak(3 * inch),
          P("2. Storm chart", H2),
          P("The last hour of lightning. Each dot is one strike: <b>left</b> is an hour ago, <b>right</b> is now, "
            "<b>low</b> is close, <b>high</b> is far (up to 25 miles). The dot colour shows the strike's energy, and "
            "the newest dot has a ring around it. The dashed red line is 10 miles. A line of dots sloping <b>down to "
            "the right</b> means the storm is getting closer. The bars at the bottom count all strikes in each "
            "5 minutes, so you can see a storm build up and die down."),
          P("3. Flash-to-bang", H2),
          P("Measure the distance by ear and compare it with the sensor. Tap the big circle the moment you <b>see</b> the "
            "flash. Tap again the moment you <b>hear</b> the thunder. Flash Bee shows the seconds and the distance "
            "(about 5 seconds per mile). If the sensor picked up a strike in the last 60 seconds, it shows the "
            "sensor's distance next to yours, so you can compare. Tap once more to start again. The last 3 results "
            "stay on screen. If no thunder comes within 60 seconds, it says \"too far to hear\"."),
          P("4. Records", H2),
          P("All-time records: <b>closest strike</b>, <b>strongest strike</b>, <b>biggest storm</b> (most strikes), "
            "<b>all strikes</b> ever, and <b>number of storms</b>. A new storm starts when there has been no strike for "
            "30 minutes. Records are kept when you switch off. Demo strikes never count. To clear them, "
            "<b>hold your finger on the RESET button at the bottom of this page for 3 seconds</b> until the bar fills "
            "and it says RECORDS + ALERT RESET. This also clears the alert and the 30-minute countdown. A quick tap does nothing, so a stray touch cannot erase them."),
          KeepTogether([P("5. Settings", H2),
                        P("Tap a button to change it. Your choices are saved within about 10 seconds and kept when "
                          "you switch off."),
                        settings_screen(),
                        P("The Settings page.", CAP)]),
          table([["Setting", "What it does"],
                 ["Profile", "Tap to step through <b>AUTO</b>, <b>STATION</b>, <b>PORTABLE</b> and <b>STORMCHASE</b>. "
                             "Leave it on <b>AUTO</b>: it uses STATION while Flash Bee is connected to the home WiFi "
                             "and PORTABLE everywhere else, and shows which one under the word AUTO. Without WiFi, "
                             "AUTO is always PORTABLE."],
                 ["Units", "Miles or kilometres."],
                 ["Brightness", "Three levels. Lower saves battery."],
                 ["Screen off after", "How long the screen stays lit with no touch and no strike: 30 seconds, 1, 2, 5 or "
                                      "10 minutes, or NEVER (5 minutes to start with). When it goes dark, Flash Bee is "
                                      "still listening. A strike or a touch lights it again. <b>The touch that wakes it "
                                      "only wakes it</b>, so it will not press a button by accident. It never goes dark "
                                      "while SHELTER is showing."]],
                [1.4 * inch, CW - 1.4 * inch]),
          P("The three profiles", H3),
          table([["Profile", "Use it", "What the sensor does"],
                 ["STATION", "At home (AUTO picks it while on WiFi)", "Ignores disturbers. Shows nothing until the chip has "
                  "heard 5 strikes within 15 minutes, so one stray far-off flash does not count."],
                 ["PORTABLE", "On the move (AUTO picks it without WiFi)", "Ignores disturbers. Reports every strike."],
                 ["STORMCHASE", "Only while you watch a storm yourself", "The most sensitive setting. Reports every strike and "
                  "counts every disturber, so it gives false strikes if you leave it on. It shows <b>THIS RUN ONLY</b> "
                  "and goes back to your saved profile when you switch off."]],
                [1.15 * inch, 1.85 * inch, CW - 3.0 * inch]),
          KeepTogether([P("6. Nerd stats", H2),
          P("The raw numbers, one per row: noise floor, watchdog and spike rejection (NF/WD/SR), the sensor state, the "
            "profile, gain, the alert state, disturbers and noise alarms since switch-on, the sensor's calibration "
            "and antenna frequency (CAL / ANT, for example <b>OK 499.2k</b>), how long it has been on, the battery "
            "voltage and percentage, strikes since switch-on and in this storm, restarts, and the WiFi and MQTT state. "
            "Carry it around the house and watch NF rise near computers and chargers. (With STATION or PORTABLE the "
            "sensor ignores disturbers, so that counter stays at 0. STORMCHASE counts them.)")]),
          P("At the bottom is a <b>RESET FILTERS</b> button. Tap it if the sensor seems deaf after a noisy spell "
            "(for example after a phone charger was next to it): it puts the sensor's noise settings "
            "back to normal. It does not change the alert."),
          CondPageBreak(3.2 * inch),
          P("Battery and charging", H2),
          *bullets([
              "The small battery on the Main page shows how full the battery is. The Nerd stats page shows the same "
              "percentage and the battery voltage.",
              "The percentage follows a lithium-ion discharge curve: <b>100 % at 4.20 volts, 15 % at 3.70 volts, "
              "0 % at 3.30 volts</b>. Under 15 % the battery turns red-orange: charge it soon.",
              "A full charge lasts <b>about 10 hours with WiFi on</b>. (In one test night it ran unplugged for about "
              "9.3 hours on its 400 mAh battery and was still going.) How long it lasts with WiFi off has not been "
              "measured yet. A shorter screen-off time and lower brightness help.",
              "To charge, plug a USB-C cable into Flash Bee and into a USB charger. <b>The power switch must be ON</b>: "
              "the switch sits between the battery and the charger, so with the switch off the battery is not "
              "connected and does not charge.",
              "<b>While it is plugged in, the percentage reads high</b>: the charger holds the voltage up. It reaches "
              "100&nbsp;% before the battery is full, so leave it plugged in longer. The number is only accurate on battery.",
              "\"--\" in place of the percentage means no reading. On USB power that usually means the power switch "
              "is off (see Troubleshooting).",
          ]),
          CondPageBreak(2.2 * inch),
          P("Demo mode", H2),
          P("To see it work without a storm, <b>hold a finger still on the screen for 3 "
            "seconds</b> on any page. A bar fills up, then a simulated storm starts: it comes in from far away, passes "
            "close, and moves off again, so you see APPROACHING and MOVING AWAY, the chart, the alert and the "
            "all-clear countdown."),
          box("How to tell the demo from real data", bullets([
              "The title says <b>DEMO - NOT REAL</b> and a <b>magenta (pink) ring</b> goes round the screen on every page.",
              "Hold 3 seconds again to stop it. It also stops by itself after 10 minutes, when you switch off, and "
              "<b>immediately if real lightning is detected</b>.",
              "Demo strikes never go into your records or the real storm chart.",
          ]), bg=PINK_BG, edge=MAGENTA),
          PageBreak()]

    # ---------- 4. Safety ----------
    s += [P(CH[3], H1),
          box("Not a safety device", [
              P("<b>Flash Bee is a hobby instrument, not a safety device.</b> It can miss strikes, and it cannot tell "
                "you where the next bolt will land. If you hear thunder, you are close enough to be struck: "
                "<b>go indoors</b>, whatever the screen says."),
          ], bg=RED_BG, edge=RED),
          P("Lightning can strike <b>10 miles</b> or more away from the storm that makes it, sometimes from a "
            "clear sky ahead of the storm (a \"bolt from the blue\"). That is why Flash Bee treats 10 miles as the "
            "danger line:"),
          range_scale(),
          Spacer(1, 4),
          *bullets([
              "<b>Strikes at 10 miles or closer:</b> go inside a real building (with plumbing and wiring) or a hard-topped "
              "car. Tents, porches, sheds, dugouts and trees are not safe.",
              "<b>The 30-30 rule:</b> when the time from flash to bang is 30 seconds or less (about 6 miles), go inside. "
              "Then wait <b>30 minutes after the last thunder</b> before going back out.",
              "<b>ALL CLEAR countdown:</b> every real strike at 10 miles or closer starts a 30:00 countdown on the Main "
              "page, in red, and SHELTER shows while it runs. Each new close strike starts it again. When it reaches "
              "zero, it shows <b>ALL CLEAR</b> in green: 30 minutes without close lightning. Still use your ears: if "
              "you hear thunder, the 30 minutes start again.",
              "<b>Indoors:</b> stay off corded phones, away from windows during close strikes, and out of the shower or bath.",
          ]),
          P("South Dakota storms", H2),
          P("The Great Plains get some of the most intense thunderstorms in the world, mostly from late spring to "
            "late summer, often in the afternoon and evening. Big storms can be seen and heard from far away on flat "
            "ground, which makes them spectacular to watch and easy to underestimate. Watch from inside, with Flash Bee "
            "near a window."),
          PageBreak()]

    # ---------- 5. Nerd corner ----------
    s += [P(CH[4], H1),
          P("Four experiments to run with Flash Bee, and some reference numbers."),
          P("1. Sensor distance vs. flash-to-bang", H2),
          P("During a storm, use the Flash-to-bang page for several strikes and write down your distance and the "
            "sensor's distance each time. Which is larger, and by how much? The sensor estimates the distance to the "
            "edge of the storm from signal strength, while thunder timing measures one bolt. Does the gap change as "
            "the storm gets closer?"),
          P("2. Track a storm", H2),
          P("Leave Flash Bee on the Storm chart page during a whole storm. Sketch or photograph the chart every 10 minutes. "
            "When was it closest? How long from first strike to all-clear? Compare with weather radar."),
          P("3. Map local radio noise", H2),
          P("Open Nerd stats and watch <b>NF</b> (the noise floor) while you carry Flash Bee near a laptop, a phone "
            "charger, an LED bulb, a microwave, and outside away from the house. Where does NF climb? It drops one step "
            "after each quiet minute. To count <b>disturbers</b> too, switch the profile to STORMCHASE for the test "
            "(and back to AUTO afterwards, or switch off and on): the DIST/NOISE row then counts both. Why would the "
            "everyday profiles ignore disturbers?"),
          P("4. Spark test", H2),
          P("A piezo grill or candle lighter makes a small spark when you click it. Click one a few inches from Flash Bee "
            "(not touching it, and never light a flame near it). The spark is a short, fast current pulse, so it "
            "radiates a radio burst, as lightning does on a vastly larger scale. Does Flash Bee report a strike, a "
            "disturber, or nothing? Try different distances. "
            "<b>Note:</b> a spark that counts as a strike is a real strike to Flash Bee. It goes into your records, "
            "and if it reads 10 miles or closer it starts the 30-minute countdown and SHELTER."),
          P("Numbers worth knowing", H2),
          table([["Fact", "Value"],
                 ["Speed of sound", "about 1 mile in 5 seconds (1 km in 3 seconds)"],
                 ["Speed of the radio signal", "the speed of light: 186,000 miles per second"],
                 ["Sensor listening frequency", "500 kHz (just below the AM radio band)"],
                 ["Sensor range", "about 25 miles (40 km)"],
                 ["Distance steps the sensor reports", "overhead, then about 3, 4, 5, 6, 7, 9, 11, 12, 15, 17, 19, 21, 23, 25 miles"],
                 ["Lightning channel temperature", "around 50,000 degrees F, hotter than the surface of the sun"],
                 ["Danger distance", "10 miles: go inside"]],
                [2.3 * inch, CW - 2.3 * inch]),
          PageBreak()]

    # ---------- 6. Build your own ----------
    s += [P(CH[5], H1),
          P("Flash Bee was designed by <b>gokux</b> and published on Instructables as \"Flash Bee: Handheld Lightning "
            "Sensing and Ranging Device\". The unit in this guide follows that design (gokux's case and wiring), with one extra "
            "wire and improved software. This chapter has everything needed to build a second one. The build photos are "
            "from gokux's original instructable. The code and print files are in the project repository."),
          box("Safety", [
              P("Three parts of this build can hurt you. <b>Soldering</b>: the iron is hot enough to burn, and the fumes "
                "should not be breathed; work with good airflow. <b>Cutting</b>: removing a socket and trimming 3D prints "
                "needs a craft knife or flush cutters; cut away from your hand. The <b>lithium battery</b>: a pinched, "
                "punctured or shorted LiPo can catch fire, so never cut both wires at once, and keep the bare ends apart."),
          ], bg=RED_BG, edge=RED),
          P("Two ways to build it", H2),
          table([["", "A. Soldered, gokux's case (the unit shown)", "B. Solderless case (new design)"],
                 ["Case", "gokux's 3D-printed case, files in the <b>Flash Bee</b> folder", "OpenSCAD case, files in the <b>enclosure</b> folder"],
                 ["Sensor link", "Four wires soldered to the XIAO (D4, D5, 3V3, GND), plus the IRQ wire to D2", "A Grove cable and a microSD \"sniffer\" adapter in the display's microSD slot"],
                 ["Soldering", "Sensor pads and all wires", "Only the four pad bridges on the sensor"],
                 ["Status", "Built and in use", "Designed but <b>not yet checked on hardware</b>"]],
                [0.95 * inch, (CW - 0.95 * inch) / 2, (CW - 0.95 * inch) / 2]),
          Spacer(1, 4),
          P("The steps below are route A. Route B is summed up at the end of the chapter."),
          KeepTogether(img("parts", CW * 0.62, "All the parts of gokux's build: 3D-printed case pieces, slide switch, round display, "
               "XIAO ESP32-C3, lightning sensor and battery. Photo: gokux.")),
          CondPageBreak(2.5 * inch),
          P("Parts list (route A)", H2),
          table([["Part", "What it does"],
                 ["Seeed Studio XIAO ESP32-C3", "The microcontroller board that runs the firmware. It has WiFi and Bluetooth (WiFi is optional)."],
                 ["Round Display for Seeed Studio XIAO", "1.28 inch round touchscreen, 240 x 240 pixels. It also charges the battery and has a microSD slot."],
                 ["Grove Lightning Sensor AS3935", "Detects lightning up to 40 km away. Answers on I2C address 3."],
                 ["902025 3.7 V 400 mAh LiPo battery", "About 10 hours per charge with WiFi on. The display charges at about 485 mA, "
                  "so use a cell of at least 500 mAh, or check that a 400 mAh cell allows a 0.5 A charge."],
                 ["Slide switch", "Power on/off, in the battery's + wire."],
                 ["2 x M2x5 mm and 2 x M3x10 mm screws", "M2 hold the round display, M3 close the case."],
                 ["Connecting wires (5)", "Four for the sensor, one for the IRQ wire."],
                 ["B7000 glue", "Holds the battery, the sensor and the covers."],
                 ["Also needed", "3D printer and filament, soldering iron, a Mac with a USB-C <b>data</b> cable for flashing."]],
                [2.2 * inch, CW - 2.2 * inch]),
          CondPageBreak(3 * inch),
          P("Step 1. Print the case", H2),
          P("The print files are in the <b>Flash Bee</b> folder of the project:"),
          table([["File", "Part"],
                 ["back+body.stl", "Main body (holds the switch, battery and sensor)"],
                 ["front+cover+.stl", "Front cover (yellow)"],
                 ["addon+.stl", "Decorative frame glued on top of the front cover (black)"],
                 ["antenna+cover+.stl", "Antenna cover over the sensor"],
                 ["Flash Bee.3mf", "PrusaSlicer project with the body, antenna cover and front cover (PETG, 0.15 mm layers, Prusa Core One)"],
                 ["Flash Bee Top.3mf", "PrusaSlicer project with the front cover alone (Prusa XL)"],
                 ["Flash+Bee_.step", "gokux's 3D model, for changing the design"]],
                [1.7 * inch, CW - 1.7 * inch]),
          Spacer(1, 4),
          P(f"gokux used a yellow and black colour scheme. Trim any support material off with a craft knife or "
            "flush cutters."),
          CondPageBreak(3.2 * inch),
          P(f"Step 2. Prepare the sensor", H2),
          *bullets([
              "Remove the white Grove (JST) socket from the sensor board.",
              "Solder-bridge four pads: <b>0-3V</b>, <b>REG</b>, <b>CS</b> and <b>SI1</b>. This puts the chip in I2C "
              "mode at 3.3 V. In the left photo the pads are circled in green and the bridges are marked red.",
              "Solder four wires to the pads where the socket was: <b>SDA</b>, <b>SCL</b>, <b>VCC</b> and <b>GND</b>.",
              "Solder a fifth wire to the pad marked <b>IRQ</b> on the back of the board. This wire was not in gokux's "
              "build: it lets Flash Bee tune the antenna, and it wakes the XIAO from light sleep as soon as the sensor "
              "signals an event.",
          ]),
          two_photos("pads", "wires",
                     "The pads to bridge (green circles, bridges in red). The IRQ pad is at the bottom of the back side.",
                     "Four wires soldered on where the socket was."),
          P("Photos: gokux.", CAP),
          CondPageBreak(4 * inch),
          P(f"Step 3. Wire the sensor to the XIAO", H2),
          *bullets([
              "Plug the XIAO ESP32-C3 onto the back of the Round Display.",
              "Set the Round Display's own power switch (on the edge of the board) to <b>ON</b>.",
              "Set the <b>KE switch</b> to ON. It is a tiny 2-toggle DIP switch between the microSD slot and the XIAO "
              "header pins. Set <b>both</b> toggles ON: one connects the battery reading to D0, the other the backlight "
              "to D6. Without it the battery shows \"--\".",
              "Solder the sensor wires to the XIAO as in the table.",
          ]),
          table([["Sensor pad", "XIAO pin", "Job"],
                 ["SDA", "D4", "Data (I2C)"],
                 ["SCL", "D5", "Clock (I2C)"],
                 ["VCC", "3V3", "3.3 volt power"],
                 ["GND", "GND", "Ground"],
                 ["IRQ", "D2", "Interrupt request: antenna tuning and wake-up"]],
                [1.2 * inch, 1.2 * inch, CW - 2.4 * inch]),
          Spacer(1, 4),
          P("I2C is a two-wire bus (data and clock) that several chips can share, each with its own address. The "
            "touchscreen uses the same two wires: the lightning sensor is address 3 and the touch chip is 0x2E. On the unit shown the IRQ wire is "
            "soldered straight to the XIAO's D2 pin."),
          two_photos("display", "wiring",
                     "The XIAO plugged onto the back of the Round Display. Photo: gokux.",
                     "gokux's wiring diagram. It does not show the IRQ wire. Photo: gokux.", h=1.9 * inch),
          CondPageBreak(3.5 * inch),
          P(f"Step 4. Switch and battery", H2),
          *bullets([
              "Fit the slide switch into its slot in the main body.",
              "Glue the battery into the bottom of the body.",
              "Solder the battery's <b>+</b> (red) wire to one end of the switch. Run a wire from the middle of the switch "
              "to the <b>+</b> of the Round Display's battery socket (JST 1.25). The battery's <b>-</b> (black) wire goes "
              "straight to the <b>-</b> of that socket. gokux's battery had no plug, so he soldered the wires to the "
              "socket's pins.",
              "Check the polarity against the BAT marking on the display before you connect anything.",
              "Because the switch is between the battery and the charger, Flash Bee must be <b>switched on to charge</b>.",
          ]),
          two_photos("switch", "battery",
                     "The slide switch in its slot.",
                     "Battery glued in, + wire soldered to the switch."),
          P("Photos: gokux.", CAP),
          CondPageBreak(3.5 * inch),
          P("Step 5. Close the case", H2),
          *bullets([
              "Glue the sensor onto the main body.",
              "Put the display assembly in, with the USB-C port where you can reach it from outside.",
              "Glue the antenna cover over the sensor.",
              "Put the front cover on and glue the decorative frame on top of it. From the back, fix the round display "
              "with the two M2 screws and close the case with the two M3 screws.",
          ]),
          two_photos("sensor_in", "display_in",
                     "The sensor goes into the main body.",
                     "The display assembly goes in on top."),
          *img("closing", CW * 0.42, "The front cover goes on. Photos on this page: gokux."),
          CondPageBreak(4 * inch),
          P(f"Step 6. Put the software on it", H2),
          P("This needs a Mac, a copy of the Flash Bee project folder, and a USB-C <b>data</b> cable (some cables only "
            "charge). The script <b>flash.sh</b> does everything: it installs <b>arduino-cli</b> with Homebrew if it is "
            "missing (so the Mac needs Homebrew), installs the ESP32 tools and the display libraries, compiles the "
            "program, uploads it and opens the serial monitor."),
          *bullets([
              "Switch Flash Bee on, plug it into the Mac, and <b>tap the screen</b> so it is awake. (While the screen is "
              "off it can go into light sleep, which switches its USB port off.)",
              "Open Terminal, go to the project folder, and run:",
          ]),
          P("bash flash.sh c3", CODE),
          *bullets([
              "The first run downloads a lot and takes a while. At the end it shows the serial monitor with Flash Bee's "
              "log. Press <b>Ctrl-C</b> to leave it.",
              "If it says <b>No /dev/cu.usbmodem* port</b>: tap the screen, check the cable, and run it again. If it still "
              "fails, hold the XIAO's <b>BOOT</b> button, tap <b>RESET</b>, release BOOT, and run it again.",
              "If the upload fails with \"device reports readiness to read but returned no data\", another program has "
              "the port open (an old serial monitor, for example). Close it and try again.",
              "If it warns about a <b>TFT_eSPI</b> library: that library clashes with Seeed_GFX. Rename or remove it.",
              "The README's Flashing section has the Arduino IDE route instead.",
          ]),
          P("Step 7. The first start", H2),
          P("Watch the screen after the upload (or the next time you switch on):"),
          table([["Screen", "What is happening"],
                 ["FLASH BEE", "The splash, with the owner's line under it if one is set (console: <font face=\"Courier\">splash &lt;text&gt;</font>)."],
                 ["AS3935 / INITIALIZING...", "Looking for the sensor on D4/D5."],
                 ["TUNING ANTENNA...", "First start only: it tries all 16 tuning settings of the antenna (about 4 seconds), "
                  "measures each one through the IRQ wire, keeps the one closest to 500 kHz, and remembers it."],
                 ["ANT 499.2k CAL OK (green)", "Done: the antenna is at 499.2 kHz (your number will differ a little) and the "
                  "sensor's clocks are calibrated. Then the Main page appears."],
                 ["CAL FAIL (orange)", "Calibration failed. Switch off and on away from computers and chargers."],
                 ["NOT FOUND (red)", "No sensor answered: check the four wires and the four pad bridges."]],
                [1.9 * inch, CW - 1.9 * inch]),
          Spacer(1, 4),
          P("In the serial monitor you should see <font face='Courier'>[AS3935] Found on D4/D5</font>, the antenna "
            "lines <font face='Courier'>[ANT] cap ...</font> and <font face='Courier'>[CAL] ... -&gt; OK</font>. "
            "<font face='Courier'>[ANT] no pulses on IRQ</font> means the IRQ wire is not connected: everything else "
            "works, but the antenna stays untuned and Flash Bee will not use light sleep. To tune again later, type "
            "<font face='Courier'>tune</font> in the serial monitor and press Enter."),
          CondPageBreak(3.5 * inch),
          P("Step 8. First-test checklist", H2),
          *checklist([
              "The start screen ends in a green <b>CAL OK</b>.",
              "The Main page shows <b>--</b>, <b>waiting...</b> and green rings moving in.",
              "Swipe through all six pages; the dots at the bottom follow.",
              "On Settings, tap each button and watch it change.",
              "<b>Demo:</b> hold a finger still for 3 seconds. DEMO - NOT REAL and a magenta ring appear, fake strikes "
              "come in. Hold 3 seconds again to stop.",
              f"<b>Spark test</b>: click a piezo lighter (no flame) a few inches from the sensor. Look for a strike "
              "or a change on Nerd stats. A spark that counts goes into the records and may start SHELTER; clear the "
              "records afterwards on the Records page if you want a clean start.",
              "Unplug the USB cable: the battery shows a percentage, not \"--\".",
              "Plug in again with the switch ON: it charges.",
              "Leave it alone for 5 minutes: the screen goes dark. One touch wakes it.",
          ]),
          P("Optional: WiFi and Home Assistant are set up from the Mac too. See chapter 7.", SMALL),
          CondPageBreak(3 * inch),
          P("Route B: the solderless case (not yet tested)", H2),
          P("A second case needs no wires soldered to the XIAO. It is in the <b>enclosure</b> folder "
            "(OpenSCAD source <b>flashbee_case.scad</b>; ready STL and sliced files for the Prusa Core One+ in PETG in "
            "<b>enclosure/exports</b>). Outside size 58 x 84.5 x 30 mm. It has not been built and checked yet, so "
            "treat it as an experiment."),
          *bullets([
              "<b>Parts:</b> XIAO ESP32-C3 with pre-soldered headers, Round Display for XIAO, Grove AS3935 (socket kept), "
              "a microSD sniffer adapter with its 8-pin header fitted, a Grove to 4-pin female jumper cable (20 cm), a 1S "
              "LiPo with a JST 1.25 plug up to 26 x 26 x 12 mm (902025 or 102525), and 2 x M3x10 screws.",
              "<b>Print</b> the fit_test first (8 min): the display must drop into the ring and sit flat. Then front "
              "(face down, 55 min), back (31 min) and lever (2 min).",
              f"<b>Solder</b> only the four pad bridges (0-3V, REG, CS, SI1). Set the KE switch ON.",
              "<b>Wire</b> the Grove cable to the sniffer: yellow SCL to DO/DAT0, white SDA to CS/CD/DAT3, red VCC to VCC, "
              "black GND to GND. Push the sniffer fully into the display's microSD slot.",
              "<b>Software:</b> on this route D2 carries the sensor's SDA, so there is no IRQ wire. Before flashing, set "
              "<font face='Courier'>USE_IRQ_TUNING</font> to 0 in FlashBee/FlashBee.h. Without it the antenna is not "
              "tuned and Flash Bee does not use light sleep.",
              "The lever works the display's own switch, and the charger stays on with the switch off, so this case "
              "charges while off. If the picture is sideways, send <font face='Courier'>set rot 1</font> (or "
              "<font face='Courier'>set rot 3</font>) in the serial monitor.",
              "The README's \"Solderless enclosure\" section has the full assembly order.",
          ]),
          PageBreak()]

    # ---------- 7. Software ----------
    s += [P(CH[6], H1),
          P("The firmware (the program that runs on the XIAO) is written in C++ with the Arduino framework. It started "
            "as gokux's code and was then fixed and extended for this unit. This chapter describes what it does."),
          P("When you switch on", H2),
          *bullets([
              "It checks that the lightning sensor answers (the screen says NOT FOUND if it does not).",
              "It resets the sensor and applies your sensor profile.",
              "The very first time, it <b>tunes the antenna</b> (the <b>TUNING ANTENNA...</b> message): it tries all 16 "
              "settings of the antenna's tuning capacitor, measures each frequency through the IRQ wire, and keeps the "
              "one closest to 500 kHz. It remembers the result and does not repeat it.",
              "It <b>calibrates</b> the sensor's internal clocks against the antenna. That is the <b>CAL OK</b> at "
              "start-up, after the antenna frequency (for example <b>ANT 499.2k CAL OK</b>).",
              "It loads your saved settings and records from the XIAO's flash memory.",
          ]),
          P("While it runs", H2),
          *bullets([
              "About 14 times a second it polls the sensor's interrupt register, which reports <b>lightning</b>, "
              "<b>disturber</b>, <b>noise too high</b>, or nothing.",
              "On lightning it reads the energy and distance, flashes the screen, adds the strike to the chart, the "
              "trend, the 15-minute count and your records, and starts the all-clear countdown if it was within 10 miles.",
              "On noise alarms it raises the noise floor, at most once every 2 seconds, and lowers it again after a "
              "quiet minute. (The original code could leave the sensor nearly deaf for minutes after a burst of noise; "
              "this was one of the fixes.)",
              "Every minute it reads the sensor's settings back. If the chip has reset itself (after a power dip, for "
              "example), it sets it up again.",
              "It watches the touchscreen: a short tap, a sideways swipe (at least 50 pixels), or a 3-second hold with "
              "the finger kept still.",
              "It draws each frame into an off-screen buffer (a sprite), then copies it to the display in one go, "
              "which avoids flicker.",
              "<b>Light sleep</b> (CPU paused, memory kept): while the screen is off, WiFi is off and no computer is "
              "plugged in, the XIAO sleeps between checks. It wakes at least once a second, and at once on an IRQ "
              "signal or a touch. While asleep its USB port is off, so a computer may not see it: tap the screen first.",
              "If the program hangs, a <b>watchdog</b> timer (which the program must keep resetting) restarts it "
              "within 8 seconds.",
          ]),
          P("Improvements over the original", H2),
          table([["Change", "Why"],
                 ["Correct sensor gain values and a proper start-up calibration", "The original used a gain value that is not in the chip's datasheet and never really calibrated the sensor."],
                 ["Antenna tuning on the IRQ wire, saved", "This sensor's antenna was 515.5 kHz untuned (3 % off). Tuned, it is 499.2 kHz."],
                 ["Noise-floor and sensitivity fixes", "Stops the sensor going deaf after noise bursts and stops it becoming over-sensitive."],
                 ["Gauge drawn the right way round", "The energy gauge started at the 1:30 position; it now fills from bottom-left over the top."],
                 ["Time since last strike, dimming after 15 min", "So an old distance never looks like a new one."],
                 ["Storm trend, 15-minute count, storm chart", "To see where a storm is heading."],
                 ["Flash-to-bang, records, all-clear countdown", "Experiments, collecting, and the 30-30 safety rule."],
                 ["Touch: pages, settings, demo", "The original did not use the touchscreen at all."],
                 ["Sensor profiles (AUTO, STATION, PORTABLE, STORMCHASE)", "Replace the old Indoor/Outdoor choice. Each profile sets the sensor up for where you are and how closely you are watching."],
                 ["Two-step alert: ACTIVITY and SHELTER", "So the screen tells you what to do, not just what the numbers are."],
                 ["Screen-off timer and light sleep", "Saves battery without missing a strike."],
                 ["Battery % from a lithium battery curve", "The old straight-line formula showed 0 % after 7.7 hours while the battery still had more than an hour left."],
                 ["Sensor-lost recovery, settings check, watchdog", "It recovers by itself instead of freezing."],
                 ["Miles, battery %, brightness", "Settings you can change without a computer."]],
                [2.5 * inch, CW - 2.5 * inch], cell=CELLS, cellb=CELLSB, pad=3),
          CondPageBreak(3.5 * inch),
          P("Optional: WiFi and Home Assistant", H2),
          P("Flash Bee works fully without a network. <b>WiFi is off when you get it</b>, and while it is off the radio "
            "never starts, so it costs no battery. If the house runs Home Assistant, Flash Bee can join the home "
            "WiFi and report to it. This needs the Mac and a USB-C data cable, once. "
            "The link is not encrypted, so use it on your own home network only."),
          *bullets([
              "With Flash Bee plugged in and awake, run this in the project folder:",
          ]),
          P("uv run --with pyserial tools/provision.py", CODE),
          *bullets([
              "(or <font face='Courier'>python3 tools/provision.py</font> if pyserial is installed). It asks for the WiFi "
              "name and password, an OTA password for over-the-air updates, and, if used, the <b>MQTT</b> broker (the "
              "server that relays Flash Bee's messages to Home Assistant) with its user and password. Press Enter to skip any question. Passwords "
              "are not shown while you type, and Flash Bee never shows them on its screen.",
              "The same settings can be typed one line at a time into the serial console (<b>wifi ssid</b>, "
              "<b>wifi pass</b>, <b>mqtt host</b>, and so on). The README lists them all. They apply at once and are "
              "remembered when you switch off.",
              "In Home Assistant, a device called <b>Flash Bee</b> then appears by itself (MQTT discovery): strikes, "
              "distance, the alert state, the battery and more, plus a menu to change the profile.",
              "With the profile on AUTO, Flash Bee then uses STATION at home. With WiFi on it does not use light sleep, "
              "so the battery runs down faster than with WiFi off.",
          ]),
          PageBreak()]

    # ---------- 8. Troubleshooting ----------
    s += [P(CH[7], H1),
          table([["What you see", "What to do"],
                 ["\"waiting...\" for hours or days", "Normal. It only shows numbers when there is lightning within about 25 miles. Try the demo to check the screen works."],
                 ["Lots of false strikes on a clear day", "You are probably near something electrically noisy. Move away from computers, chargers and LED lights, "
                  "make sure the profile is on AUTO (not STORMCHASE), then tap RESET FILTERS on the Nerd stats page."],
                 ["Misses a storm you can hear", "Put it near a window: metal roofs and walls block the signal. STATION waits for 5 strikes in 15 minutes before it reports, "
                  "and STATION and PORTABLE ignore disturbers. If you are watching the storm yourself, try STORMCHASE for a while."],
                 ["Stays deaf after a noisy spell, or says ENV TOO NOISY", "Move away from the noise, then tap RESET FILTERS on the Nerd stats page."],
                 ["SENSOR LOST", "The sensor stopped answering. Flash Bee keeps trying every 3 seconds. If it does not come back, switch off and on."],
                 ["The screen is black", "It is probably just asleep (the Screen off after setting). Touch it: the first touch wakes it. If that does nothing, the battery may be flat: plug it in with the switch on."],
                 ["NOT FOUND at start-up", "The sensor is not answering. Switch off and on again. If it stays, a wire inside has come loose: the case has to be opened (chapter 6, step 3)."],
                 ["CAL FAIL at start-up", "Switch off and on again away from computers and chargers. It still detects lightning, but distances may be less accurate."],
                 ["Battery shows \"--\"", "Switch it on: with the switch off, the battery is not connected, even on USB. If it still shows \"--\", the KE switch on the display board must be set ON, which means opening the case (chapter 6, step 3)."],
                 ["Battery jumps up when plugged in, or shows 100 % too soon", "Normal: on the charger the reading is too high. Leave it plugged in a while after 100 %. The number is right again on battery."],
                 ["Does not charge", "The power switch must be ON while it is plugged in."],
                 ["The computer does not see Flash Bee", "Tap the screen: while the screen is off it can be in light sleep, with its USB port off. Use a data cable, not a charge-only one."],
                 ["Screen is stuck or acting strangely", "Switch off, wait 5 seconds, switch on. Your records and settings are kept."],
                 ["Magenta ring and DEMO - NOT REAL", "Demo mode is on. Hold a finger still on the screen for 3 seconds to stop it, or switch off and on."]],
                [2.0 * inch, CW - 2.0 * inch], cell=CELLS, cellb=CELLSB, pad=3),
          P("Care", H2),
          *bullets([
              "It is not waterproof. Keep it dry: watch storms from inside, or keep it under cover.",
              "Do not leave it in a hot car or in direct sun for hours. Heat ages a lithium battery and can make it swell.",
              "Charge it every few weeks even when you are not using it, so the battery does not run completely flat.",
              "The screen is glass. A case or pouch in a backpack is a good idea.",
          ]),
          box("Credits", [
              P("Flash Bee was designed by <b>gokux</b> and published on Instructables (\"Flash Bee: Handheld Lightning "
                "Sensing and Ranging Device\", March 2026) under the Creative Commons Attribution-NonCommercial-"
                "ShareAlike 4.0 license. The build photos and wiring diagram in this guide are from that instructable. "
                "This guide and the improved software are shared under the same license.", SMALL),
          ], gap=False),
          ]
    return s


def build_guide():
    def make_doc(target, chapters):
        doc = GuideDoc(target, pagesize=letter, leftMargin=M, rightMargin=M, topMargin=0.7 * inch,
                       bottomMargin=0.75 * inch, title="Flash Bee: Handheld Lightning Detector",
                       author="Flash Bee build", subject="User guide and build guide")
        frame = Frame(M, 0.75 * inch, CW, H - 1.45 * inch, id="f")
        doc.addPageTemplates([PageTemplate(id="cover", frames=[frame], onPage=on_cover),
                              PageTemplate(id="body", frames=[frame], onPage=make_on_page(chapters))])
        return doc

    first = make_doc(io.BytesIO(), [])          # pass 1: find the chapter pages
    first.build(guide_story({}))
    toc = {t: p for t, p in first.chapters}
    final = make_doc(OUT, first.chapters)      # pass 2: same layout, with page numbers and running titles
    final.build(guide_story(toc))
    if final.chapters != first.chapters:
        sys.exit(f"chapter pages moved between passes: {first.chapters} -> {final.chapters}")
    return final.page


# ======================================================================================
# Quick start (two pages)
# ======================================================================================
def quick_story():
    half = CW / 2 - 8

    def two_col(left, right, widths=(CW / 2, CW / 2)):
        t = Table([[left, right]], colWidths=list(widths))
        t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                               ("LEFTPADDING", (0, 0), (-1, -1), 2), ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                               ("TOPPADDING", (0, 0), (-1, -1), 0), ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))
        return t

    def qb(items):
        return bullets(items, QBUL)

    s = [P("Flash Bee Quick Start", QTITLE),
         P("The first five minutes with Flash Bee.", QSUB),

         P("1. Switch it on", QH),
         *qb(["Slide the power switch on the bottom of the case to ON. The screen says <b>AS3935</b>, then a green line that ends in "
              "<b>CAL OK</b>. Then the Main page comes up.",
              "Slide the switch back to turn it off.",
              "Screen dark? It turns off to save battery but keeps listening. <b>Touch it once</b> to wake it. "
              "That first touch only wakes it.",
              ]),

         P("2. The Main page", QH),
         two_col([mini_screen("wait", half, 0.5),
                  P("<b>Waiting.</b> \"--\" and \"waiting...\" with green rings moving in: it is listening, "
                    "not stuck. It can stay like this for days.", QBODY)],
                 [mini_screen("activity", half, 0.5),
                  P("<b>Strike.</b> The screen flashes. The big number is the estimated distance to the storm, "
                    "in miles.", QBODY)]),
         *qb(["<b>OVERHEAD</b> means less than 1 mile away. <b>&gt;25</b> means too far to measure.",
              "<b>1m ago</b> is the time since the last strike. <b>STRIKES</b> counts them since you switched on.",
              "<b>APPROACHING</b>: the storm is getting closer. <b>MOVING AWAY</b>: it is leaving. "
              "<b>STEADY</b>: its distance is not changing much.",
              ]),

         P("3. Swipe to change pages", QH),
         P("Swipe left or right across the screen. The dots at the bottom show which page you are on.", QBODY),
         table([["1", "<b>Main</b>: the lightning right now."],
                ["2", "<b>Storm chart</b>: every strike of the last hour."],
                ["3", "<b>Flash-to-bang</b>: tap at the flash, tap at the thunder. It works out how far."],
                ["4", "<b>Records</b>: all-time closest and strongest strikes, biggest storm."],
                ["5", "<b>Settings</b>: profile, miles or km, brightness, when the screen goes dark."],
                ["6", "<b>Nerd stats</b>: the sensor's numbers and the battery voltage."]],
               [0.35 * inch, CW - 0.35 * inch], header=False, cell=QCELL),
         P("After 2 minutes it goes back to Main by itself. Real lightning also brings you back to Main.", QBODY),
         PageBreak(),

         P("4. The alert: what to do", QH),
         P("Look at the bottom of the Main page.", QBODY),
         two_col([Spacer(1, 14), mini_screen("shelter", CW * 0.3, 0.55)],
                 table([["You see", "It means", "Do this"],
                        ["<b>ACTIVITY</b><br/>small, orange", "Lightning in the last 15 minutes.", "Keep watching. Be ready to go in."],
                        ["<b>SHELTER</b><br/>big, flashing red", "Lightning 10 miles away or closer, or 5 or more strikes in 15 minutes.", "<b>Go indoors now.</b>"],
                        ["<b>ALL CLEAR IN 29:40</b>", "A 30-minute countdown. Every close strike starts it again from 30:00.", "Stay inside until it ends."],
                        ["<b>ALL CLEAR</b><br/>green", "30 minutes with no close lightning.", "OK to go out, if you hear no thunder."]],
                       [1.35 * inch, 2.0 * inch, CW * 0.7 - 3.35 * inch - 8], cell=QCELL, cellb=QCELLB),
                 widths=(CW * 0.3, CW * 0.7)),
         P("You cannot switch SHELTER off. It goes away by itself when the storm has passed.", QBODY),

         P("5. Charging", QH),
         *qb(["Plug a USB-C cable into Flash Bee and into a USB charger.",
              "<b>The switch must be ON</b>, or it will not charge.",
              "While it is plugged in, the battery number reads too high. It shows 100 % before the battery is full, "
              "so leave it plugged in a while longer. Unplugged, the number is right.",
              "When the battery turns red-orange (under 15 %), charge it soon. A full battery lasts about 10 hours "
              "(tested with WiFi on).",
              ]),

         P("6. Try the demo", QH),
         *qb(["Hold one finger still on the screen for <b>3 seconds</b>. A bar fills up, then a fake storm starts.",
              "<b>DEMO - NOT REAL</b> and a pink ring mean the strikes are simulated.",
              "Hold 3 seconds again to stop. It also stops by itself after 10 minutes. Demo strikes never go into "
              "your records.",
              ]),
         box("Safety", [P("Flash Bee is a hobby device, not a safety device. It can miss lightning. "
                          "<b>If you can hear thunder, go indoors</b>, whatever the screen says.", QBODY)],
             bg=RED_BG, edge=RED, tstyle=ParagraphStyle("QBoxT", parent=BOXT, fontSize=14, leading=17)),
         P("Everything else is in the <b>Flash Bee Guide</b>: all the settings, records, storm safety, experiments "
           "to try, and how to build your own Flash Bee.", QBODY),
         ]
    return s


def build_quick():
    def on_page(c, doc):
        c.saveState()
        c.setFillColor(YELLOW)
        c.rect(0, H - 0.32 * inch, W, 0.32 * inch, stroke=0, fill=1)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 8.5)
        c.drawString(M, H - 0.21 * inch, "FLASH BEE  |  Quick Start")
        c.setFont("Helvetica", 8.5)
        c.drawRightString(W - M, H - 0.21 * inch, "Full guide: Flash Bee Guide.pdf")
        c.setFont("Helvetica", 8)
        c.setFillColor(GREY)
        c.drawRightString(W - M, 0.45 * inch, f"Page {doc.page} of 2")
        c.drawString(M, 0.45 * inch, CREDIT)
        c.restoreState()

    doc = BaseDocTemplate(OUT, pagesize=letter, leftMargin=M, rightMargin=M, topMargin=0.6 * inch,
                          bottomMargin=0.7 * inch, title="Flash Bee Quick Start", author="Flash Bee build",
                          subject="Quick start")
    frame = Frame(M, 0.7 * inch, CW, H - 1.3 * inch, id="f")
    doc.addPageTemplates([PageTemplate(id="q", frames=[frame], onPage=on_page)])
    doc.build(quick_story())
    return doc.page


if __name__ == "__main__":
    pages = build_quick() if QUICK else build_guide()
    print("wrote", OUT, f"({pages} pages)")
