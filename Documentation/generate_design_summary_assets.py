from __future__ import annotations

from io import BytesIO
from pathlib import Path
from xml.etree import ElementTree as ET
import html
import math

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor
from markdown_it import MarkdownIt


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "DesignSummary.md"
DOCX_PATH = ROOT / "LundaLoggern_Design_Summary.docx"
DRAWIO_PATH = ROOT / "LundaLoggern_Architecture.drawio"
CLASS_SVG_PATH = ROOT / "LundaLoggern_Class_Diagram.svg"
FLOW_SVG_PATH = ROOT / "LundaLoggern_Data_Flow.svg"
PIN_SVG_PATH = ROOT / "LundaLoggern_TDisplayS3_Pin_Layout.svg"

INK = "#18333F"
MUTED = "#536B73"
TEAL = "#087E83"
BLUE = "#286B9B"
GREEN = "#4E7B58"
AMBER = "#A96828"
LINE = "#66818A"
WHITE = "#FFFFFF"


def text(x: int, y: int, value: str, size: int = 17, color: str = INK,
         weight: str = "400", anchor: str = "start") -> str:
    return (f'<text x="{x}" y="{y}" font-family="Aptos, Segoe UI, sans-serif" '
            f'font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" '
            f'fill="{color}">{html.escape(value)}</text>')


def card(x: int, y: int, w: int, h: int, title: str, lines: list[str], color: str) -> str:
    header = (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="9" '
              f'fill="{WHITE}" stroke="{color}" stroke-width="2"/>'
              f'<path d="M{x+9} {y} H{x+w-9} Q{x+w} {y} {x+w} {y+9} '
              f'V{y+43} H{x} V{y+9} Q{x} {y} {x+9} {y}" fill="{color}"/>'
              + text(x + 15, y + 28, title, 18, WHITE, "700"))
    rows = "".join(text(x + 15, y + 68 + row * 22, line, 14) for row, line in enumerate(lines))
    return header + rows


def edge(path: str, label: str = "", x: int = 0, y: int = 0, dashed: bool = False) -> str:
    dash = ' stroke-dasharray="7 6"' if dashed else ""
    label_svg = text(x, y, label, 13, MUTED, "600") if label else ""
    return (f'<path d="{path}" fill="none" stroke="{LINE}" stroke-width="2" '
            f'marker-end="url(#arrow)"{dash}/>' + label_svg)


def svg(title: str, width: int, height: int, body: str) -> str:
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}"><defs><marker id="arrow" viewBox="0 0 10 10" '
            f'refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">'
            f'<path d="M0 0 L10 5 L0 10 z" fill="{LINE}"/></marker></defs>'
            f'<rect width="100%" height="100%" fill="#FAFCFB"/>'
            f'{text(38, 48, title, 26, INK, "700")}{body}</svg>')


CLASS_NODES = [
    {"id": "main", "label": "main.cpp\nsetup() / loop()", "x": 570, "y": 82, "w": 310, "h": 82, "color": BLUE},
    {"id": "cie", "label": "ServoCIEData\nCIE parser / channel processing", "x": 520, "y": 255, "w": 360, "h": 160, "color": TEAL},
    {"id": "sd", "label": "SDManager\nSD lifecycle / rolling logs", "x": 230, "y": 475, "w": 370, "h": 110, "color": GREEN},
    {"id": "cli", "label": "HostCli\nUSB commands / opt-in streams", "x": 650, "y": 490, "w": 300, "h": 100, "color": AMBER},
    {"id": "wifi", "label": "WifiApServer\nSoftAP / HTTP / SD actions", "x": 1030, "y": 475, "w": 350, "h": 110, "color": BLUE},
    {"id": "renderer", "label": "ImageRenderer\nTFT / updated by main.cpp", "x": 990, "y": 255, "w": 360, "h": 145, "color": GREEN},
    {"id": "time", "label": "DateTime\nRTC / formatted timestamps", "x": 40, "y": 255, "w": 300, "h": 110, "color": AMBER},
    {"id": "button", "label": "Button\npress events", "x": 1040, "y": 82, "w": 300, "h": 82, "color": AMBER},
    {"id": "qr", "label": "QRHandler\nnot wired into main runtime", "x": 1040, "y": 665, "w": 300, "h": 90, "color": MUTED},
]
CLASS_EDGES = [
    {"from": "main", "to": "cie", "label": "coordinates"},
    {"from": "main", "to": "button", "label": "polls"},
    {"from": "cie", "to": "sd", "label": "writes logs"},
    {"from": "cie", "to": "cli", "label": "emits rows"},
    {"from": "main", "to": "renderer", "label": "status updates"},
    {"from": "sd", "to": "wifi", "label": "SD file actions", "dashed": True},
    {"from": "time", "to": "sd", "label": "timestamps", "dashed": True},
    {"from": "cli", "to": "sd", "label": "LIST / DUMP", "dashed": True},
    {"from": "main", "to": "wifi", "label": "services HTTP", "dashed": True},
]

FLOW_NODES = [
    {"id": "vent", "label": "SERVO-u", "x": 35, "y": 155, "w": 180, "h": 100, "color": MUTED},
    {"id": "cie", "label": "ServoCIEData", "x": 380, "y": 145, "w": 270, "h": 120, "color": TEAL},
    {"id": "sd", "label": "SDManager", "x": 800, "y": 145, "w": 260, "h": 120, "color": GREEN},
    {"id": "wifi", "label": "WifiApServer", "x": 1200, "y": 145, "w": 220, "h": 120, "color": BLUE},
    {"id": "main", "label": "main.cpp loop", "x": 380, "y": 405, "w": 300, "h": 115, "color": BLUE},
    {"id": "renderer", "label": "ImageRenderer", "x": 920, "y": 405, "w": 280, "h": 115, "color": GREEN},
    {"id": "button", "label": "Button", "x": 380, "y": 585, "w": 180, "h": 90, "color": AMBER},
    {"id": "cli", "label": "HostCli", "x": 800, "y": 585, "w": 260, "h": 95, "color": AMBER},
    {"id": "time", "label": "DateTime / RTC", "x": 1200, "y": 585, "w": 220, "h": 95, "color": MUTED},
]
FLOW_EDGES = [
    {"from": "vent", "to": "cie", "label": "RS232 / CIE bytes"},
    {"from": "cie", "to": "sd", "label": "parsed values"},
    {"from": "sd", "to": "wifi", "label": "file operations"},
    {"from": "cie", "to": "main", "label": "identity / phase"},
    {"from": "main", "to": "renderer", "label": "display state"},
    {"from": "main", "to": "wifi", "label": "HTTP servicing", "dashed": True},
    {"from": "main", "to": "button", "label": "operator events", "dashed": True},
    {"from": "cie", "to": "cli", "label": "enabled streams"},
    {"from": "time", "to": "sd", "label": "timestamps", "dashed": True},
]

PIN_NODES = [
    {"id": "board", "label": "LilyGo T-Display S3\nESP32-S3", "x": 440, "y": 150, "w": 350, "h": 360, "color": BLUE},
    {"id": "transceiver", "label": "RS232 transceiver\nLogic-side pins", "x": 45, "y": 205, "w": 300, "h": 135, "color": AMBER},
    {"id": "ventilator", "label": "SERVO-u ventilator\nRS232 line side", "x": 45, "y": 425, "w": 300, "h": 95, "color": MUTED},
    {"id": "sdreader", "label": "MicroSD reader\n3.3 V logic", "x": 890, "y": 205, "w": 285, "h": 210, "color": GREEN},
]
PIN_EDGES = [
    {"from": "board", "to": "transceiver", "label": "GPIO17 TXD2 -> DI"},
    {"from": "transceiver", "to": "board", "label": "RO -> GPIO16 RXD2"},
    {"from": "transceiver", "to": "ventilator", "label": "RS232 TX/RX pair"},
    {"from": "board", "to": "sdreader", "label": "GPIO11 MOSI, 13 SCLK, 10 CS"},
    {"from": "sdreader", "to": "board", "label": "GPIO12 MISO"},
]


def class_svg() -> str:
    body = [
        edge("M725 164 V215 H700 V255", "coordinates", 746, 208),
        edge("M880 122 H1040", "polls", 948, 108),
        edge("M520 335 H420 V475", "writes logs", 424, 327),
        edge("M700 415 V490", "emits rows", 715, 463),
        edge("M880 335 H980 V320 H990", "status updates", 895, 315),
        edge("M600 530 H1030", "SD file actions", 725, 516, True),
        edge("M340 310 H420 V475", "timestamps", 345, 297, True),
        edge("M650 540 H600", "LIST / DUMP", 580, 565, True),
        edge("M880 120 H960 V455 H1030", "services HTTP", 930, 445, True),
    ]
    body += [card(n["x"], n["y"], n["w"], n["h"], n["label"].split("\n")[0], n["label"].split("\n")[1:], n["color"]) for n in CLASS_NODES]
    body.append(text(40, 805, "QRHandler owns a TFT_eSPI and QRCode instance, but is not currently used by main.cpp.", 14, MUTED))
    return svg("LundaLoggern | Class Relationships", 1430, 835, "".join(body))


def flow_svg() -> str:
    body = [
        edge("M215 205 H380", "RS232 / CIE", 250, 190),
        edge("M650 205 H800", "parse / scale", 675, 190),
        edge("M1060 205 H1200", "SD files", 1090, 190),
        edge("M515 265 V405", "identity / phase", 525, 350),
        edge("M680 462 H920", "display updates", 745, 447),
        edge("M680 480 H1110 V265 H1200", "HTTP servicing", 875, 463, True),
        edge("M470 520 V585", "button", 482, 565, True),
        edge("M550 250 H730 V630 H800", "enabled streams", 590, 615),
        edge("M1200 630 H1100 V270 H1060", "RTC time", 1097, 610, True),
    ]
    for n in FLOW_NODES:
        title, *lines = n["label"].split("\n")
        body.append(card(n["x"], n["y"], n["w"], n["h"], title, lines, n["color"]))
    body += [text(38, 745, "Curves stream over USB after a complete sample set; this firmware does not write them to SD.", 14, MUTED),
             text(38, 775, "WiFi is operator-enabled. HostCli data streams are off by default.", 14, MUTED)]
    return svg("LundaLoggern | Data Flow", 1460, 805, "".join(body))


def pin_svg() -> str:
    body = [
        edge("M440 250 H345", "GPIO17 TXD2 -> DI", 345, 235),
        edge("M345 300 H440", "RO -> GPIO16 RXD2", 345, 322),
        edge("M195 340 V425", "RS232 line-side pair", 210, 390),
        edge("M790 255 H890", "MOSI 11 / SCLK 13 / CS 10", 795, 238),
        edge("M890 350 H790", "MISO 12", 805, 372),
    ]
    body.append(card(440, 150, 350, 360, "LilyGo T-Display S3", [
        "ESP32-S3 GPIO assignments", "", "GPIO16  UART2 RXD2", "GPIO17  UART2 TXD2", "", "GPIO12  HSPI MISO", "GPIO11  HSPI MOSI", "GPIO13  HSPI SCLK", "GPIO10  HSPI CS", "", "GPIO14  Interaction button"], BLUE))
    body.append(card(45, 205, 300, 135, "RS232 transceiver", ["Logic side: DI / RO", "Use between ESP32 and", "ventilator RS232 port"], AMBER))
    body.append(card(45, 425, 300, 95, "SERVO-u ventilator", ["RS232 line side"], MUTED))
    body.append(card(890, 205, 285, 210, "MicroSD reader", ["GPIO12  DO / MISO", "GPIO11  DI / MOSI", "GPIO13  CLK / SCK", "GPIO10  CS", "3.3 V logic; shared GND"], GREEN))
    body.append(text(45, 590, "UART2: 38 400 baud, 8E1. Do not connect ESP32 GPIO directly to RS232 voltage levels.", 15, MUTED))
    body.append(text(45, 620, "SD bus: HSPI on GPIO12/11/13 with GPIO10 chip select.", 15, MUTED))
    return svg("LilyGo T-Display S3 | UART2 and SD Reader Wiring", 1230, 670, "".join(body))


PNG_LINKS = {
    "class": [
        ([(725, 164), (725, 215), (700, 215), (700, 255)], "coordinates", (746, 202)),
        ([(880, 122), (1040, 122)], "polls", (948, 108)),
        ([(520, 335), (420, 335), (420, 475)], "writes logs", (424, 327)),
        ([(700, 415), (700, 490)], "emits rows", (715, 463)),
        ([(880, 335), (980, 335), (980, 320), (990, 320)], "status updates", (895, 315)),
        ([(600, 530), (1030, 530)], "SD file actions", (725, 516)),
        ([(340, 310), (420, 310), (420, 475)], "timestamps", (345, 297)),
        ([(650, 540), (600, 540)], "LIST / DUMP", (580, 565)),
        ([(880, 122), (960, 122), (960, 455), (1030, 455)], "services HTTP", (930, 445)),
    ],
    "flow": [
        ([(215, 205), (380, 205)], "RS232 / CIE", (250, 190)),
        ([(650, 205), (800, 205)], "parse / scale", (675, 190)),
        ([(1060, 205), (1200, 205)], "SD files", (1090, 190)),
        ([(515, 265), (515, 405)], "identity / phase", (525, 350)),
        ([(680, 462), (920, 462)], "display updates", (745, 447)),
        ([(680, 480), (1110, 480), (1110, 265), (1200, 265)], "HTTP servicing", (875, 463)),
        ([(470, 520), (470, 585)], "button", (482, 565)),
        ([(550, 250), (730, 250), (730, 630), (800, 630)], "enabled streams", (590, 615)),
        ([(1200, 630), (1100, 630), (1100, 270), (1060, 270)], "RTC time", (1097, 610)),
    ],
    "pin": [
        ([(440, 250), (345, 250)], "GPIO17 TXD2 -> DI", (345, 235)),
        ([(345, 300), (440, 300)], "RO -> GPIO16 RXD2", (345, 322)),
        ([(195, 340), (195, 425)], "RS232 line-side pair", (210, 390)),
        ([(790, 255), (890, 255)], "MOSI 11 / SCLK 13 / CS 10", (795, 238)),
        ([(890, 350), (790, 350)], "MISO 12", (805, 372)),
    ],
}


def png_font(size: int, bold: bool = False):
    font_dir = Path("C:/Windows/Fonts")
    font_name = "segoeuib.ttf" if bold else "segoeui.ttf"
    font_path = font_dir / font_name
    try:
        return ImageFont.truetype(str(font_path), size)
    except OSError:
        return ImageFont.load_default()


def render_png(kind: str) -> BytesIO:
    if kind == "class":
        title, width, height, nodes, notes = "LundaLoggern | Class Relationships", 1430, 835, CLASS_NODES, [
            "QRHandler owns a TFT_eSPI and QRCode instance, but is not currently used by main.cpp."]
    elif kind == "flow":
        title, width, height, nodes, notes = "LundaLoggern | Data Flow", 1460, 805, FLOW_NODES, [
            "Curves stream over USB after a complete sample set; this firmware does not write them to SD.",
            "WiFi is operator-enabled. HostCli data streams are off by default."]
    else:
        title, width, height, nodes, notes = "LilyGo T-Display S3 | UART2 and SD Reader Wiring", 1230, 670, PIN_NODES, [
            "UART2: 38 400 baud, 8E1. Do not connect ESP32 GPIO directly to RS232 voltage levels.",
            "SD bus: HSPI on GPIO12/11/13 with GPIO10 chip select."]
    scale = 2
    image = Image.new("RGB", (width * scale, height * scale), "#FAFCFB")
    draw = ImageDraw.Draw(image)
    draw.text((38 * scale, 16 * scale), title, font=png_font(26 * scale, True), fill=INK)

    for points, label, label_xy in PNG_LINKS[kind]:
        scaled_points = [(x * scale, y * scale) for x, y in points]
        draw.line(scaled_points, fill=LINE, width=3 * scale, joint="curve")
        x1, y1 = scaled_points[-2]
        x2, y2 = scaled_points[-1]
        angle = math.atan2(y2 - y1, x2 - x1)
        length = 11 * scale
        wing = 6 * scale
        base_x = x2 - length * math.cos(angle)
        base_y = y2 - length * math.sin(angle)
        left = (base_x + wing * math.sin(angle), base_y - wing * math.cos(angle))
        right = (base_x - wing * math.sin(angle), base_y + wing * math.cos(angle))
        draw.polygon([scaled_points[-1], left, right], fill=LINE)
        draw.text((label_xy[0] * scale, label_xy[1] * scale), label,
                  font=png_font(12 * scale, True), fill=MUTED)

    for node in nodes:
        x, y, w, h = (node[key] * scale for key in ("x", "y", "w", "h"))
        color = node["color"]
        draw.rounded_rectangle((x, y, x + w, y + h), radius=16 * scale,
                               fill=WHITE, outline=color, width=3 * scale)
        draw.rounded_rectangle((x, y, x + w, y + 44 * scale), radius=14 * scale, fill=color)
        draw.rectangle((x, y + 30 * scale, x + w, y + 44 * scale), fill=color)
        lines = node["label"].split("\n")
        draw.text((x + 14 * scale, y + 7 * scale), lines[0],
                  font=png_font(16 * scale, True), fill=WHITE)
        for line_num, line in enumerate(lines[1:]):
            draw.text((x + 14 * scale, y + (58 + line_num * 21) * scale), line,
                      font=png_font(12 * scale), fill=INK)
    note_y = (height - 35 - (len(notes) - 1) * 25) * scale
    for note in notes:
        draw.text((38 * scale, note_y), note, font=png_font(13 * scale), fill=MUTED)
        note_y += 25 * scale
    output = BytesIO()
    image.save(output, format="PNG")
    output.seek(0)
    return output


def drawio_page(name: str, nodes: list[dict], edges: list[dict], width: int, height: int) -> ET.Element:
    diagram = ET.Element("diagram", {"id": name.lower().replace(" ", "-"), "name": name})
    model = ET.SubElement(diagram, "mxGraphModel", {
        "dx": "1430", "dy": "820", "grid": "1", "gridSize": "10", "guides": "1",
        "tooltips": "1", "connect": "1", "arrows": "1", "fold": "1", "page": "1",
        "pageScale": "1", "pageWidth": str(width), "pageHeight": str(height), "math": "0", "shadow": "0",
    })
    root = ET.SubElement(model, "root")
    ET.SubElement(root, "mxCell", {"id": "0"})
    ET.SubElement(root, "mxCell", {"id": "1", "parent": "0"})
    for number, item in enumerate(edges, 2):
        style = "edgeStyle=orthogonalEdgeStyle;rounded=0;html=1;endArrow=block;endFill=1;strokeColor=#66818A;"
        if item.get("dashed"):
            style += "dashed=1;"
        cell = ET.SubElement(root, "mxCell", {
            "id": f"e{number}", "value": item["label"], "style": style,
            "edge": "1", "parent": "1", "source": item["from"], "target": item["to"],
        })
        ET.SubElement(cell, "mxGeometry", {"relative": "1", "as": "geometry"})
    for node in nodes:
        title, *lines = node["label"].split("\n")
        label = "<b>" + html.escape(title) + "</b>" + ("<br>" + "<br>".join(html.escape(line) for line in lines) if lines else "")
        cell = ET.SubElement(root, "mxCell", {
            "id": node["id"], "value": label,
            "style": f"rounded=1;whiteSpace=wrap;html=1;fillColor=#FFFFFF;strokeColor={node['color']};fontColor=#18333F;fontSize=15;align=center;verticalAlign=middle;",
            "vertex": "1", "parent": "1",
        })
        ET.SubElement(cell, "mxGeometry", {
            "x": str(node["x"]), "y": str(node["y"]), "width": str(node["w"]),
            "height": str(node["h"]), "as": "geometry",
        })
    return diagram


def make_drawio() -> bytes:
    root = ET.Element("mxfile", {
        "host": "app.diagrams.net", "modified": "2026-11-05T00:00:00.000Z",
        "agent": "LundaLoggern documentation generator", "version": "24.7.17", "type": "device",
    })
    root.append(drawio_page("Class Diagram", CLASS_NODES, CLASS_EDGES, 1430, 835))
    root.append(drawio_page("Data Flow", FLOW_NODES, FLOW_EDGES, 1460, 805))
    root.append(drawio_page("T-Display S3 Pin Layout", PIN_NODES, PIN_EDGES, 1230, 670))
    return ET.tostring(root, encoding="utf-8", xml_declaration=True)


def plain(token) -> str:
    result = []
    for child in token.children or []:
        if child.type in {"text", "code_inline", "html_inline"}:
            result.append(child.content)
        elif child.type in {"softbreak", "hardbreak"}:
            result.append(" ")
    return "".join(result)


def add_inline(paragraph, token) -> None:
    styles: list[str] = []
    for item in token.children or []:
        if item.type.endswith("_open"):
            styles.append(item.type[:-5])
        elif item.type.endswith("_close"):
            if styles:
                styles.pop()
        elif item.type in {"softbreak", "hardbreak"}:
            paragraph.add_run(" ")
        elif item.type == "image":
            path = (ROOT / (item.attrGet("src") or "")).resolve()
            if path.exists() and path.suffix.lower() == ".svg":
                png = render_png("pin")
                image_paragraph = paragraph._parent.add_paragraph()
                image_paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
                image_paragraph.add_run().add_picture(png, width=Inches(6.45))
        elif item.type in {"text", "code_inline", "html_inline"}:
            run = paragraph.add_run(item.content)
            run.bold = "strong" in styles
            run.italic = "em" in styles
            if item.type == "code_inline" or "code_inline" in styles:
                run.font.name = "Consolas"
                run.font.size = Pt(8.5)
                run.font.color.rgb = RGBColor(8, 92, 96)
            if "link" in styles:
                run.font.color.rgb = RGBColor(40, 107, 155)
                run.underline = True


def add_table(document: Document, tokens: list, start: int) -> int:
    rows: list[list[tuple[str, bool]]] = []
    row: list[tuple[str, bool]] | None = None
    index = start + 1
    while index < len(tokens) and tokens[index].type != "table_close":
        item = tokens[index]
        if item.type == "tr_open":
            row = []
        elif item.type in {"th_open", "td_open"} and row is not None:
            row.append((plain(tokens[index + 1]), item.type == "th_open"))
        elif item.type == "tr_close" and row is not None:
            rows.append(row)
            row = None
        index += 1
    if rows:
        table = document.add_table(rows=0, cols=max(map(len, rows)))
        table.style = "Light Shading Accent 1"
        for row_index, values in enumerate(rows):
            cells = table.add_row().cells
            for col_index, (value, header) in enumerate(values):
                cell = cells[col_index]
                cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
                run = cell.paragraphs[0].add_run(value)
                run.font.size = Pt(8)
                run.bold = header or row_index == 0
    return index + 1


def add_code(document: Document, source: str) -> None:
    paragraph = document.add_paragraph()
    paragraph.paragraph_format.left_indent = Inches(0.15)
    paragraph.paragraph_format.space_after = Pt(6)
    for number, line in enumerate(source.rstrip("\n").splitlines() or [""]):
        if number:
            paragraph.add_run().add_break()
        run = paragraph.add_run(line)
        run.font.name = "Consolas"
        run.font.size = Pt(8)
        run.font.color.rgb = RGBColor(35, 65, 72)
    shading = OxmlElement("w:shd")
    shading.set(qn("w:fill"), "F1F5F4")
    paragraph._p.get_or_add_pPr().append(shading)


def add_diagram(document: Document, kind: str, caption: str) -> None:
    paragraph = document.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.add_run().add_picture(render_png(kind), width=Inches(6.45))
    label = document.add_paragraph(caption, style="Caption")
    label.alignment = WD_ALIGN_PARAGRAPH.CENTER


def make_docx(source: str, class_image: str, flow_image: str) -> None:
    tokens = MarkdownIt("commonmark").enable("table").enable("strikethrough").parse(source)
    document = Document()
    section = document.sections[0]
    section.top_margin = section.bottom_margin = Inches(0.62)
    section.left_margin = section.right_margin = Inches(0.68)
    styles = document.styles
    styles["Normal"].font.name = "Aptos"
    styles["Normal"].font.size = Pt(9.5)
    styles["Normal"].font.color.rgb = RGBColor(24, 51, 63)
    for name, size, color in [("Title", 24, (8, 92, 96)), ("Heading 1", 18, (8, 92, 96)),
                              ("Heading 2", 14, (40, 107, 155)), ("Heading 3", 11, (78, 123, 88))]:
        style = styles[name]
        style.font.name = "Aptos Display"
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor(*color)
    current_heading = ""
    list_stack: list[str] = []
    index = 0
    first_heading = True
    while index < len(tokens):
        item = tokens[index]
        if item.type == "heading_open":
            inline = tokens[index + 1]
            title = plain(inline)
            level = int(item.tag[1])
            if first_heading:
                paragraph = document.add_paragraph(style="Title")
                first_heading = False
            else:
                paragraph = document.add_heading(level=min(level, 3))
            add_inline(paragraph, inline)
            current_heading = title
            index += 3
            continue
        if item.type == "paragraph_open":
            inline = tokens[index + 1]
            style = "List Number" if list_stack and list_stack[-1] == "ordered" else "List Bullet" if list_stack else None
            paragraph = document.add_paragraph(style=style)
            add_inline(paragraph, inline)
            index += 3
            continue
        if item.type in {"bullet_list_open", "ordered_list_open"}:
            list_stack.append("ordered" if item.type == "ordered_list_open" else "bullet")
        elif item.type in {"bullet_list_close", "ordered_list_close"}:
            if list_stack:
                list_stack.pop()
        elif item.type == "table_open":
            index = add_table(document, tokens, index)
            continue
        elif item.type in {"fence", "code_block"}:
            if current_heading == "3.2 Class Diagram" and item.info.strip().lower() == "mermaid":
                add_diagram(document, "class", "Figure 1. Runtime class relationships.")
            elif current_heading == "3.5 Data Flow" and not item.info.strip():
                add_diagram(document, "flow", "Figure 2. Runtime data flow.")
            else:
                add_code(document, item.content)
        elif item.type == "hr":
            paragraph = document.add_paragraph("-" * 50)
            paragraph.paragraph_format.space_after = Pt(2)
        index += 1
    footer = document.sections[0].footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    footer.add_run("LundaLoggern | Design Summary").font.size = Pt(8)
    document.core_properties.title = "LundaLoggern - Design Summary"
    document.core_properties.subject = "Firmware architecture, wiring, and data flow"
    document.save(DOCX_PATH)


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    class_image = class_svg()
    flow_image = flow_svg()
    pin_image = pin_svg()
    CLASS_SVG_PATH.write_text(class_image, encoding="utf-8")
    FLOW_SVG_PATH.write_text(flow_image, encoding="utf-8")
    PIN_SVG_PATH.write_text(pin_image, encoding="utf-8")
    drawio = ET.Element("mxfile", {"host": "app.diagrams.net", "version": "24.7.17", "type": "device"})
    drawio.append(drawio_page("Class Diagram", CLASS_NODES, CLASS_EDGES, 1430, 835))
    drawio.append(drawio_page("Data Flow", FLOW_NODES, FLOW_EDGES, 1460, 805))
    drawio.append(drawio_page("T-Display S3 Pin Layout", PIN_NODES, PIN_EDGES, 1230, 670))
    DRAWIO_PATH.write_bytes(ET.tostring(drawio, encoding="utf-8", xml_declaration=True))
    make_docx(source, class_image, flow_image)
    print("Created:")
    for path in (DOCX_PATH, DRAWIO_PATH, CLASS_SVG_PATH, FLOW_SVG_PATH, PIN_SVG_PATH):
        print(f"  {path.name} ({path.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()