#!/usr/bin/env python3

"""Regression for Scripter setText preserving existing text styles.

For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
"""

import os
import tempfile

import scribus


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def character_style_at(frame, position):
    scribus.selectText(position, 1, frame)
    return scribus.getCharacterStyle(frame)


def paragraph_style_at(frame, position):
    scribus.selectText(position, 1, frame)
    return scribus.getParagraphStyle(frame)


check(
    scribus.newDocument(
        scribus.PAPER_A4,
        (20, 20, 20, 20),
        scribus.PORTRAIT,
        1,
        scribus.UNIT_POINTS,
        scribus.PAGE_1,
        0,
        1,
    ),
    "could not create document",
)
scribus.createCharStyle(name="First Run", fillcolor="Red")
scribus.createCharStyle(name="Second Run", fillcolor="Blue")
scribus.createParagraphStyle("Heading")
scribus.createParagraphStyle("Body")

frame = scribus.createText(50, 50, 300, 80, "Styled Text")
scribus.setText("ABCDE12345", frame)
scribus.selectText(0, 5, frame)
scribus.setCharacterStyle("First Run", frame)
scribus.selectText(5, 5, frame)
scribus.setCharacterStyle("Second Run", frame)

scribus.setText("VWXYZ67890", frame)
check(scribus.getAllText(frame) == "VWXYZ67890", "same-length replacement failed")
check(character_style_at(frame, 0) == "First Run", "first character run was lost")
check(character_style_at(frame, 5) == "Second Run", "second character run was lost")

scribus.setText("Short", frame)
check(character_style_at(frame, 0) == "First Run", "shorter replacement lost styling")
scribus.setText("Longer text", frame)
check(character_style_at(frame, 0) == "First Run", "longer replacement lost first style")
check(character_style_at(frame, 10) == "First Run", "new suffix did not inherit styling")

paragraph_frame = scribus.createText(50, 150, 300, 80, "Styled Paragraphs")
scribus.setText("First\nSecond", paragraph_frame)
scribus.selectText(0, 1, paragraph_frame)
scribus.setParagraphStyle("Heading", paragraph_frame)
scribus.selectText(6, 1, paragraph_frame)
scribus.setParagraphStyle("Body", paragraph_frame)
scribus.setText("Title\nDetail", paragraph_frame)
check(paragraph_style_at(paragraph_frame, 0) == "Heading", "first paragraph style was lost")
check(paragraph_style_at(paragraph_frame, 6) == "Body", "second paragraph style was lost")

empty_frame = scribus.createText(50, 250, 300, 80, "Cleared Text")
scribus.setText("Old", empty_frame)
scribus.selectText(0, 3, empty_frame)
scribus.setCharacterStyle("First Run", empty_frame)
scribus.setText("", empty_frame)
scribus.setText("Again", empty_frame)
check(character_style_at(empty_frame, 0) == "First Run", "empty replacement lost styling")

output_dir = os.environ.get("SCRIBUS_TEST_OUTPUT_DIR", tempfile.gettempdir())
os.makedirs(output_dir, exist_ok=True)
document_path = os.path.join(output_dir, "settext-preserves-styles.sla")
scribus.saveDocAs(document_path)
scribus.closeDoc()
check(scribus.openDoc(document_path), "could not reopen styled document")
check(character_style_at(frame, 0) == "First Run", "first style did not persist")
check(character_style_at(frame, 10) == "First Run", "suffix style did not persist")
check(paragraph_style_at(paragraph_frame, 0) == "Heading", "heading did not persist")
check(paragraph_style_at(paragraph_frame, 6) == "Body", "body style did not persist")
check(character_style_at(empty_frame, 0) == "First Run", "cleared frame style did not persist")
scribus.closeDoc()

print("SETTEXT_STYLES_QA_PASSED", flush=True)
