#!/usr/bin/env python3

"""List-marker formatting must survive native SLA save/reopen.

For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
"""

import os
import tempfile
from xml.etree import ElementTree

import scribus


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def story_text(frame):
    scribus.selectText(0, 0, frame)
    return scribus.getAllText(frame)


def marker_positions(frame):
    return [i for i, char in enumerate(story_text(frame)) if char == "\x19"]


def formatting_at(frame, position):
    scribus.selectText(position, 1, frame)
    return (scribus.getFontSize(frame), scribus.getTextColor(frame))


def set_marker_formatting(frame, expected):
    scribus.layoutText(frame)
    positions = marker_positions(frame)
    check(len(positions) == 2, f"expected two list markers in {frame}")
    for position, (size, color) in zip(positions, expected):
        scribus.selectText(position, 1, frame)
        scribus.setFontSize(size, frame)
        scribus.setTextColor(color, frame)


def check_story(frame, expected):
    scribus.layoutText(frame)
    text = story_text(frame)
    check(text == "\x19First item\r\x19Second item", f"changed or duplicated text in {frame}: {text!r}")
    positions = marker_positions(frame)
    for position, expected_format in zip(positions, expected):
        actual = formatting_at(frame, position)
        check(actual == expected_format, f"{frame} marker {position}: {actual!r} != {expected_format!r}")
        check(formatting_at(frame, position + 1) == (12.0, "Black"), f"marker formatting leaked into body text in {frame}")


output_dir = os.environ.get("SCRIBUS_TEST_OUTPUT_DIR", tempfile.gettempdir())
os.makedirs(output_dir, exist_ok=True)
document_path = os.path.join(output_dir, "list-marker-formatting.sla")

check(scribus.newDocument(
    scribus.PAPER_A4, (20, 20, 20, 20), scribus.PORTRAIT, 1,
    scribus.UNIT_POINTS, scribus.PAGE_1, 0, 1,
), "could not create document")
scribus.createParagraphStyle(name="Bullets", bullet="\u2022")
scribus.createParagraphStyle(name="Numbers")
scribus.createParagraphStyle(name="Styled Bullets", bullet="\u2022")
scribus.createParagraphStyle(name="Normal")
scribus.createCharStyle(name="Marker Accent", fontsize=18, fillcolor="Blue")

frames = {}
for row, style in enumerate(("Bullets", "Numbers", "Styled Bullets")):
    frame = scribus.createText(50, 50 + 150 * row, 300, 120, style + " frame")
    scribus.setText("First item\nSecond item", frame)
    scribus.setParagraphStyle(style, frame)
    frames[style] = frame
scribus.saveDocAs(document_path)
scribus.closeDoc()

# Build the numbering and named paragraph-effect fixture with the native
# attributes; createParagraphStyle does not yet expose those settings.
tree = ElementTree.parse(document_path)
styles = {node.get("Name"): node for node in tree.iter("ParagraphStyle")}
styles["Numbers"].set("Numeration", "1")
styles["Numbers"].set("NumerationName", "<local block>")
styles["Numbers"].set("NumerationFormat", "0")
styles["Numbers"].set("NumerationStart", "3")
styles["Styled Bullets"].set("ParagraphEffectCharStyle", "Marker Accent")
tree.write(document_path, encoding="utf-8", xml_declaration=True)
check(scribus.openDoc(document_path), "could not open list fixture")

expected = {
    "Bullets": [(24.0, "Red"), (20.0, "Green")],
    "Numbers": [(22.0, "Red"), (16.0, "Blue")],
    "Styled Bullets": [(18.0, "Blue"), (18.0, "Blue")],
}
for style in ("Bullets", "Numbers"):
    set_marker_formatting(frames[style], expected[style])
for style, frame in frames.items():
    check_story(frame, expected[style])

for extension in ("sla", "sla.gz"):
    roundtrip_path = os.path.join(output_dir, "formatted-list-roundtrip." + extension)
    scribus.saveDocAs(roundtrip_path)
    scribus.closeDoc()
    check(scribus.openDoc(roundtrip_path), "could not reopen " + extension)
    for style, frame in frames.items():
        check_story(frame, expected[style])

# The restored markers remain generated list content: removing the list style
# removes the markers while keeping the words intact.
for frame in frames.values():
    scribus.selectText(0, 0, frame)
    scribus.setParagraphStyle("Normal", frame)
    scribus.layoutText(frame)
    check(scribus.getAllText(frame) == "First item\rSecond item", "list removal left a stale marker")
scribus.closeDoc()
print("LIST_MARKER_FORMATTING_PASSED", flush=True)
